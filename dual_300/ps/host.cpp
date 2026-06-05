#include "TopGraph.h"
#include "./ProcessUnit/include.h"

#include <adf/adf_api/XRTConfig.h>
#include <experimental/xrt_device.h>
#include <experimental/xrt_kernel.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

TopStencilGraph topStencil;

namespace {

constexpr int NUM_UNITS          = STENCIL_NUM_UNITS;  // 150
constexpr int kNumInputFiles      = 9;  // input_plio0.txt ~ input_plio8.txt
constexpr int DEFAULT_ITER        = 2;
constexpr int PREVIEW             = 16;
constexpr int OUT_WORDS_PER_ITER  = COL;

bool load_stream_file(const std::string& path, int32_t* buf, int elems) {
    std::ifstream fin(path);
    if (!fin.is_open()) {
        std::fprintf(stderr, "[warn] cannot open %s\n", path.c_str());
        return false;
    }
    long long v = 0;
    int cnt = 0;
    while (fin >> v) {
        if (cnt >= elems) break;
        buf[cnt++] = static_cast<int32_t>(v);
    }
    if (cnt != elems) {
        std::fprintf(stderr,
                     "[warn] %s element count mismatch: got %d, expect %d\n",
                     path.c_str(), cnt, elems);
        return false;
    }
    return true;
}

void fill_ramp_input(int32_t* buf, int elems, int seed) {
    for (int i = 0; i < elems; ++i) {
        buf[i] = static_cast<int32_t>(seed * 10000 + i);
    }
}

void zero_output(int32_t* out, int elems) {
    for (int i = 0; i < elems; ++i) out[i] = 0;
}

void dump_output_matrix(const std::string& path, const int32_t* out, int iter_cnt) {
    std::ofstream fout(path);
    if (!fout.is_open()) {
        std::fprintf(stderr, "[warn] cannot open %s for write\n", path.c_str());
        return;
    }
    for (int it = 0; it < iter_cnt; ++it) {
        const int32_t* row = out + it * OUT_WORDS_PER_ITER;
        for (int c = 0; c < COL; ++c) {
            if (c) fout << ' ';
            fout << row[c];
        }
        fout << '\n';
    }
}

void print_preview(const char* tag, const int32_t* p, int n) {
    std::printf("%s", tag);
    for (int i = 0; i < n; ++i) {
        std::printf(" %d", p[i]);
    }
    std::printf("\n");
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::fprintf(stderr,
                     "Usage: %s <xclbin> [iter_cnt] [input_prefix] [output_txt]\n",
                     argv[0]);
        return EXIT_FAILURE;
    }

    const std::string xclbin_path = argv[1];
    const int iter_cnt            = (argc >= 3) ? std::atoi(argv[2]) : DEFAULT_ITER;
    const std::string in_prefix   = (argc >= 4) ? argv[3] : "./data/input_plio";
    const std::string out_path    = (argc >= 5) ? argv[4] : "./data/aie_out_gmio.txt";

    if (iter_cnt <= 0) {
        std::fprintf(stderr, "[error] iter_cnt must be > 0\n");
        return EXIT_FAILURE;
    }

    const int elems_per_input = iter_cnt * COL;
    const int out_elems       = iter_cnt * OUT_WORDS_PER_ITER;

    const std::size_t bytes_per_input = elems_per_input * sizeof(int32_t);
    const std::size_t out_bytes       = out_elems * sizeof(int32_t);

    auto dhdl = xrtDeviceOpen(0);
    if (!dhdl) {
        std::fprintf(stderr, "[error] xrtDeviceOpen failed\n");
        return EXIT_FAILURE;
    }

    int ret = xrtDeviceLoadXclbinFile(dhdl, xclbin_path.c_str());
    if (ret) {
        std::fprintf(stderr, "[error] xrtDeviceLoadXclbinFile failed\n");
        xrtDeviceClose(dhdl);
        return EXIT_FAILURE;
    }

    xuid_t uuid;
    xrtDeviceGetXclbinUUID(dhdl, uuid);
    adf::registerXRT(dhdl, uuid);

    // Allocate input/output buffers for each unit
    int32_t* inbuf [NUM_UNITS];
    int32_t* outbuf[NUM_UNITS];

    for (int i = 0; i < NUM_UNITS; ++i) {
        inbuf[i] = reinterpret_cast<int32_t*>(adf::GMIO::malloc(bytes_per_input));
        outbuf[i] = reinterpret_cast<int32_t*>(adf::GMIO::malloc(out_bytes));
        if (!inbuf[i] || !outbuf[i]) {
            std::fprintf(stderr, "[error] GMIO::malloc failed for unit %d\n", i);
            for (int j = 0; j <= i; ++j) {
                if (inbuf[j]) adf::GMIO::free(inbuf[j]);
                if (outbuf[j]) adf::GMIO::free(outbuf[j]);
            }
            xrtDeviceClose(dhdl);
            return EXIT_FAILURE;
        }
    }

    // Load or generate input data (cycle through input_plio0~8.txt)
    bool ok = true;
    for (int i = 0; i < NUM_UNITS; ++i) {
        const std::string path = in_prefix + std::to_string(i % kNumInputFiles) + ".txt";
        if (!load_stream_file(path, inbuf[i], elems_per_input)) {
            ok = false;
            break;
        }
    }
    if (!ok) {
        std::fprintf(stderr, "[warn] input files incomplete, fallback to ramp input\n");
        for (int i = 0; i < NUM_UNITS; ++i) {
            fill_ramp_input(inbuf[i], elems_per_input, i);
        }
    }

    for (int i = 0; i < NUM_UNITS; ++i) {
        zero_output(outbuf[i], out_elems);
    }
    print_preview("input0 preview:", inbuf[0], PREVIEW);

    topStencil.init();
    auto t0 = std::chrono::high_resolution_clock::now();

    topStencil.run(iter_cnt);

    // Start profiling on first output
    topStencil.out_plio[0].aie2gm_nb(outbuf[0], out_bytes);
    for (int i = 1; i < NUM_UNITS; ++i) {
        topStencil.out_plio[i].aie2gm_nb(outbuf[i], out_bytes);
    }

    adf::event::handle handle = adf::event::start_profiling(
        topStencil.out_plio[0],
        adf::event::io_stream_start_to_bytes_transferred_cycles,
        static_cast<uint32_t>(out_bytes));

    if (handle == adf::event::invalid_handle) {
        std::fprintf(stderr,
                     "[error] invalid profiling handle "
                     "(likely no available performance counters on this interface tile)\n");
        topStencil.end();
        for (int i = 0; i < NUM_UNITS; ++i) {
            adf::GMIO::free(inbuf[i]);
            adf::GMIO::free(outbuf[i]);
        }
        xrtDeviceClose(dhdl);
        return EXIT_FAILURE;
    }

    // Transfer all inputs
    for (int i = 0; i < NUM_UNITS; ++i) {
        topStencil.in_plio[i].gm2aie_nb(inbuf[i], bytes_per_input);
    }

    // Wait for all outputs
    for (int i = 0; i < NUM_UNITS; ++i) {
        topStencil.out_plio[i].wait();
    }
    topStencil.wait();

    long long cycle_count = adf::event::read_profiling(handle);
    adf::event::stop_profiling(handle);

    const double freq_hz = 450000000.0;
    const double time_seconds = static_cast<double>(cycle_count) / freq_hz;

    std::printf("========================================\n");
    std::printf("Total units             : %d\n", NUM_UNITS);
    std::printf("Total AIE cores         : %d\n", NUM_UNITS * 2);
    std::printf("Event API cycles        : %lld\n", cycle_count);
    std::printf("Time (per 1st output)   : %.6f s\n", time_seconds);
    std::printf("========================================\n");

    auto t1 = std::chrono::high_resolution_clock::now();
    topStencil.end();

    const auto dur_us =
        std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

    std::printf("End-to-end time: %lld us\n", static_cast<long long>(dur_us));
    print_preview("output preview:", outbuf[0], PREVIEW);

    dump_output_matrix(out_path, outbuf[0], iter_cnt);

    for (int i = 0; i < NUM_UNITS; ++i) {
        adf::GMIO::free(inbuf[i]);
        adf::GMIO::free(outbuf[i]);
    }
    xrtDeviceClose(dhdl);
    return 0;
}
