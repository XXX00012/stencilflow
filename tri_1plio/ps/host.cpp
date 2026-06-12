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

constexpr int WARMUP_ROWS = hdiff_cfg::kLapWarmupIterations;
constexpr int BOARD_GRID_ROWS = 20;
constexpr int BOARD_GRID_DEPTH = 1;
constexpr int DEFAULT_ITER = BOARD_GRID_ROWS * BOARD_GRID_DEPTH;
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
    if (iter_cnt % hdiff_cfg::kRowsPerCall != 0) {
        std::fprintf(stderr,
                     "[error] iter_cnt %d must be divisible by kRowsPerCall %d\n",
                     iter_cnt,
                     hdiff_cfg::kRowsPerCall);
        return EXIT_FAILURE;
    }

    const int input_elems = iter_cnt * COL;
    const int output_rows = iter_cnt;
    const int output_elems = output_rows * OUT_WORDS_PER_ITER;
    const std::size_t input_bytes =
        static_cast<std::size_t>(input_elems) * sizeof(std::int32_t);
    const std::size_t output_bytes =
        static_cast<std::size_t>(output_elems) * sizeof(std::int32_t);

    std::vector<std::int32_t> input(input_elems, 0);
    if (!load_input_file(input_path, input)) {
        return EXIT_FAILURE;
    }

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

        input_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        output_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);

        topStencil.init();
        graph_initialized = true;

        const auto pl_t0 = Clock::now();
        auto toppl_run = toppl(input_bo, output_bo, iter_cnt);

        const auto aie_t0 = Clock::now();
        topStencil.run(iter_cnt / hdiff_cfg::kRowsPerCall);
        toppl_run.wait();
        const auto pl_t1 = Clock::now();
        topStencil.wait();
        const auto aie_t1 = Clock::now();

        topStencil.end();
        graph_initialized = false;

        const long long pl_transfer_us = elapsed_us(pl_t0, pl_t1);
        const long long aie_run_us = elapsed_us(aie_t0, aie_t1);

        output_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);

        dump_output_matrix(output_path, output_map, output_rows);

        std::printf("pl_transfer_us : %lld\n", pl_transfer_us);
        std::printf("aie_run_us     : %lld\n", aie_run_us);
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
