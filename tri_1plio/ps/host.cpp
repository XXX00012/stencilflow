#include "TopGraph.h"
#include "./ProcessUnit/include.h"

#include <adf/adf_api/XRTConfig.h>
#include <experimental/xrt_bo.h>
#include <experimental/xrt_device.h>
#include <experimental/xrt_kernel.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

TopStencilGraph topStencil("stencil");

namespace {

constexpr int DEFAULT_ITER = 6;
constexpr int PREVIEW = 16;
constexpr int WARMUP_ROWS = hdiff_cfg::kLapWarmupIterations;
constexpr int OUT_WORDS_PER_ITER = COL;

using Clock = std::chrono::high_resolution_clock;

long long elapsed_us(Clock::time_point start, Clock::time_point stop) {
    return std::chrono::duration_cast<std::chrono::microseconds>(stop - start)
        .count();
}

bool load_input_file(const std::string& path, std::vector<std::int32_t>& buf) {
    std::ifstream fin(path);
    if (!fin.is_open()) {
        std::fprintf(stderr, "[error] cannot open %s\n", path.c_str());
        return false;
    }

    long long v = 0;
    int cnt = 0;
    while (cnt < static_cast<int>(buf.size()) && (fin >> v)) {
        buf[cnt++] = static_cast<std::int32_t>(v);
    }

    if (cnt != static_cast<int>(buf.size())) {
        std::fprintf(stderr,
                     "[error] %s element count mismatch: got %d, expect %zu\n",
                     path.c_str(),
                     cnt,
                     buf.size());
        return false;
    }

    if (fin >> v) {
        std::fprintf(stderr,
                     "[warn] %s has extra elements after expected %zu; ignored\n",
                     path.c_str(),
                     buf.size());
    }

    return true;
}

void dump_output_matrix(const std::string& path,
                        const std::int32_t* out,
                        int iter_cnt) {
    std::ofstream fout(path);
    if (!fout.is_open()) {
        std::fprintf(stderr, "[warn] cannot open %s for write\n", path.c_str());
        return;
    }

    for (int it = 0; it < iter_cnt; ++it) {
        const std::int32_t* row = out + it * OUT_WORDS_PER_ITER;
        for (int c = 0; c < COL; ++c) {
            if (c) fout << ' ';
            fout << row[c];
        }
        fout << '\n';
    }
}

void print_preview(const char* tag, const std::int32_t* p, int n) {
    std::printf("%s", tag);
    for (int i = 0; i < n; ++i) {
        std::printf(" %d", p[i]);
    }
    std::printf("\n");
}

xrt::kernel open_toppl_kernel(const xrt::device& device,
                              const xrt::uuid& uuid) {
    try {
        return xrt::kernel(device, uuid, "TopPL:{TopPL_1}");
    } catch (const std::exception&) {
        try {
            return xrt::kernel(device, uuid, "TopPL_1");
        } catch (const std::exception&) {
            return xrt::kernel(device, uuid, "TopPL");
        }
    }
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
        (argc >= 4) ? argv[3] : "./data/input.txt";
    const std::string output_path =
        (argc >= 5) ? argv[4] : "./aie_out_plio.txt";

    if (iter_cnt <= 0) {
        std::fprintf(stderr, "[error] iter_cnt must be > 0\n");
        return EXIT_FAILURE;
    }
    if (iter_cnt <= WARMUP_ROWS) {
        std::fprintf(stderr,
                     "[error] iter_cnt must be greater than warmup rows %d\n",
                     WARMUP_ROWS);
        return EXIT_FAILURE;
    }

    const int input_elems = iter_cnt * COL;
    const int output_rows = iter_cnt - WARMUP_ROWS;
    const int output_elems = output_rows * OUT_WORDS_PER_ITER;
    const std::size_t input_bytes =
        static_cast<std::size_t>(input_elems) * sizeof(std::int32_t);
    const std::size_t output_bytes =
        static_cast<std::size_t>(output_elems) * sizeof(std::int32_t);

    std::vector<std::int32_t> input(input_elems, 0);
    if (!load_input_file(input_path, input)) {
        return EXIT_FAILURE;
    }

    print_preview("input preview        :", input.data(),
                  std::min(PREVIEW, input_elems));
    std::printf("mapping              : DDR -> TopPL -> 1 AIE PLIO lane -> TopPL -> DDR\n");
    std::printf("input rows           : %d x %d int32\n", iter_cnt, COL);
    std::printf("graph.run            : %d; keep raw output rows %d..%d\n",
                iter_cnt,
                WARMUP_ROWS,
                iter_cnt - 1);
    std::fflush(stdout);

    bool graph_initialized = false;

    try {
        auto device = xrt::device(0);
        auto xrt_uuid = device.load_xclbin(xclbin_path);

        auto dhdl = xrtDeviceOpenFromXcl(device);
        if (!dhdl) {
            std::fprintf(stderr, "[error] xrtDeviceOpenFromXcl failed\n");
            return EXIT_FAILURE;
        }

        xuid_t adf_uuid;
        xrtDeviceGetXclbinUUID(dhdl, adf_uuid);
        adf::registerXRT(dhdl, adf_uuid);

        auto toppl = open_toppl_kernel(device, xrt_uuid);
        auto input_bo = xrt::bo(device, input_bytes, toppl.group_id(0));
        auto output_bo = xrt::bo(device, output_bytes, toppl.group_id(1));

        auto input_map = input_bo.map<std::int32_t*>();
        auto output_map = output_bo.map<std::int32_t*>();
        std::memcpy(input_map, input.data(), input_bytes);
        std::memset(output_map, 0, output_bytes);

        const auto t0 = Clock::now();
        input_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        output_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);

        std::printf("[stage] topStencil.init begin\n");
        std::fflush(stdout);
        topStencil.init();
        graph_initialized = true;
        std::printf("[stage] topStencil.init done\n");
        std::fflush(stdout);

        const auto hw_t0 = Clock::now();

        std::printf("[stage] TopPL submit begin\n");
        std::fflush(stdout);
        auto toppl_run = toppl(input_bo, output_bo, iter_cnt);
        std::printf("[stage] TopPL submit done\n");
        std::fflush(stdout);

        std::printf("[stage] topStencil.run begin\n");
        std::fflush(stdout);
        topStencil.run(iter_cnt);
        std::printf("[stage] topStencil.run done\n");
        std::fflush(stdout);

        std::printf("[stage] TopPL wait begin\n");
        std::fflush(stdout);
        toppl_run.wait();
        std::printf("[stage] TopPL wait done\n");
        std::fflush(stdout);

        std::printf("[stage] graph wait begin\n");
        std::fflush(stdout);
        topStencil.wait();
        std::printf("[stage] graph wait done\n");
        std::fflush(stdout);

        topStencil.end();
        graph_initialized = false;

        const auto hw_t1 = Clock::now();
        output_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        const auto t1 = Clock::now();

        print_preview("output preview       :", output_map,
                      std::min(PREVIEW, output_elems));
        dump_output_matrix(output_path, output_map, output_rows);

        std::printf("hardware_run_us      : %lld\n", elapsed_us(hw_t0, hw_t1));
        std::printf("end_to_end_us        : %lld\n", elapsed_us(t0, t1));
        std::printf("output dumped        : %s\n", output_path.c_str());
    } catch (const std::exception& ex) {
        if (graph_initialized) {
            try {
                topStencil.end();
            } catch (...) {
            }
        }
        std::fprintf(stderr, "[error] XRT/AIE execution failed: %s\n", ex.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
