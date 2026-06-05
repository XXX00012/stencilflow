#include "TopGraph.h"
#include "./ProcessUnit/include.h"

#include <adf/adf_api/XRTConfig.h>
#include <experimental/xrt_device.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

TopStencilGraph topStencil;

namespace {

constexpr int DEFAULT_ITER = hdiff_cfg::kDefaultIterations;
constexpr int PREVIEW = 16;
constexpr int OUT_WORDS_PER_ITER = COL;

bool load_stream_file(const std::string& path, int32_t* buf, int elems) {
    std::ifstream fin(path);
    if (!fin.is_open()) {
        std::fprintf(stderr, "[warn] cannot open %s\n", path.c_str());
        return false;
    }

    long long v = 0;
    int cnt = 0;
    while (cnt < elems && (fin >> v)) {
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

void fill_ramp_input(int32_t* inbuf, int elems) {
    for (int i = 0; i < elems; ++i) {
        inbuf[i] = static_cast<int32_t>(i);
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
                     "Usage: %s <xclbin> [iter_cnt] [input_txt] [output_txt]\n",
                     argv[0]);
        return EXIT_FAILURE;
    }

    const std::string xclbin_path = argv[1];
    const int iter_cnt = (argc >= 3) ? std::atoi(argv[2]) : DEFAULT_ITER;
    const std::string input_path =
        (argc >= 4) ? argv[3] : "./data/input_plio.txt";
    const std::string out_path =
        (argc >= 5) ? argv[4] : "./data/aie_out_gmio.txt";

    if (iter_cnt <= 0) {
        std::fprintf(stderr, "[error] iter_cnt must be > 0\n");
        return EXIT_FAILURE;
    }

    const int input_elems = iter_cnt * COL;
    const int out_elems = iter_cnt * OUT_WORDS_PER_ITER;

    const std::size_t input_bytes =
        static_cast<std::size_t>(input_elems) * sizeof(int32_t);
    const std::size_t out_bytes =
        static_cast<std::size_t>(out_elems) * sizeof(int32_t);

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

    int32_t* inbuf =
        reinterpret_cast<int32_t*>(adf::GMIO::malloc(input_bytes));
    if (!inbuf) {
        std::fprintf(stderr, "[error] GMIO::malloc failed for input\n");
        xrtDeviceClose(dhdl);
        return EXIT_FAILURE;
    }

    int32_t* outbuf =
        reinterpret_cast<int32_t*>(adf::GMIO::malloc(out_bytes));
    if (!outbuf) {
        std::fprintf(stderr, "[error] GMIO::malloc failed for output\n");
        adf::GMIO::free(inbuf);
        xrtDeviceClose(dhdl);
        return EXIT_FAILURE;
    }

    if (!load_stream_file(input_path, inbuf, input_elems)) {
        std::fprintf(stderr, "[warn] input file incomplete, fallback to ramp input\n");
        fill_ramp_input(inbuf, input_elems);
    }

    zero_output(outbuf, out_elems);
    print_preview("input preview:", inbuf, PREVIEW);

    topStencil.init();
    auto t0 = std::chrono::high_resolution_clock::now();

    topStencil.run(iter_cnt);

    topStencil.out_plio[0].aie2gm_nb(outbuf, out_bytes);

    adf::event::handle handle = adf::event::start_profiling(
        topStencil.out_plio[0],
        adf::event::io_stream_start_to_bytes_transferred_cycles,
        static_cast<uint32_t>(out_bytes));

    if (handle == adf::event::invalid_handle) {
        std::fprintf(stderr,
                     "[error] invalid profiling handle "
                     "(likely no available performance counters on this interface tile)\n");
        topStencil.end();
        adf::GMIO::free(inbuf);
        adf::GMIO::free(outbuf);
        xrtDeviceClose(dhdl);
        return EXIT_FAILURE;
    }

    topStencil.in_plio[0].gm2aie_nb(inbuf, input_bytes);

    topStencil.out_plio[0].wait();
    topStencil.wait();

    long long cycle_count = adf::event::read_profiling(handle);
    adf::event::stop_profiling(handle);

    const double freq_hz = 450000000.0;
    const double time_seconds = static_cast<double>(cycle_count) / freq_hz;

    const double output_MBps =
        static_cast<double>(out_bytes) / time_seconds / (1024.0 * 1024.0);
    const double gross_bytes =
        static_cast<double>(input_bytes) + static_cast<double>(out_bytes);
    const double gross_MBps =
        gross_bytes / time_seconds / (1024.0 * 1024.0);

    std::printf("========================================\n");
    std::printf("Event API cycles        : %lld\n", cycle_count);
    std::printf("Output throughput       : %.3f MB/s\n", output_MBps);
    std::printf("Gross graph throughput  : %.3f MB/s\n", gross_MBps);
    std::printf("========================================\n");

    auto t1 = std::chrono::high_resolution_clock::now();
    topStencil.end();

    const auto dur_us =
        std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

    std::printf("End-to-end time: %lld us\n", static_cast<long long>(dur_us));
    print_preview("output preview:", outbuf, PREVIEW);

    dump_output_matrix(out_path, outbuf, iter_cnt);

    adf::GMIO::free(inbuf);
    adf::GMIO::free(outbuf);
    xrtDeviceClose(dhdl);
    return EXIT_SUCCESS;
}
