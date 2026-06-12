from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")


class BoardVolumeConfigTest(unittest.TestCase):
    def test_board_run_defaults_target_raw_full_256x256x64_volume(self):
        makefile = read("Makefile")
        host = read("ps/host.cpp")
        run_sh = read("run.sh")

        self.assertIn("BOARD_GRID_ROWS ?= 256", makefile)
        self.assertIn("BOARD_GRID_DEPTH ?= 64", makefile)
        self.assertIn("BOARD_DATA_ITER ?= 16384", makefile)
        self.assertIn("BOARD_DATA_DIR ?= ./board_data", makefile)
        self.assertIn("--package.sd_dir $(BOARD_DATA_DIR)", makefile)

        self.assertIn(
            "constexpr int DEFAULT_ITER = BOARD_GRID_ROWS * BOARD_GRID_DEPTH;",
            host,
        )
        self.assertIn(
            "constexpr int BOARD_OUTPUT_ROWS = BOARD_GRID_ROWS * BOARD_GRID_DEPTH;",
            host,
        )
        self.assertNotIn("BOARD_RAW_ROWS_PER_DEPTH", host)
        self.assertNotIn("board_volume_mode", host)

        self.assertIn("ITER=${2:-16384}", run_sh)
        self.assertIn("INPUT_TXT=${3:-./board_data/input.txt}", run_sh)

    def test_toppl_directly_streams_and_stores_every_raw_volume_row(self):
        toppl = read("pl/TopPL.cpp")

        self.assertIn("#define TRI1PLIO_MAX_ITER 16384", toppl)
        self.assertIn("#define TRI1PLIO_MAX_DDR_WORDS 262144", toppl)
        self.assertIn("constexpr int kBoardGridRows = 256;", toppl)
        self.assertIn("constexpr int kBoardGridDepth = 64;", toppl)
        self.assertIn(
            "constexpr int kBoardIter = kBoardGridRows * kBoardGridDepth;",
            toppl,
        )
        self.assertNotIn("kBoardRawRowsPerDepth", toppl)
        self.assertNotIn("board_volume_mode", toppl)
        self.assertNotIn("in_plane_r", toppl)
        self.assertIn("out_words.write(input[i]);", toppl)
        self.assertIn("output[i] = in_words.read();", toppl)

    def test_board_path_uses_128_bit_plio(self):
        top_graph = read("aie/TopGraph.h")
        toppl = read("pl/TopPL.cpp")
        gen_case = read("data/gen_case.py")
        convert = read("data/convert.py")

        self.assertIn("plio_128_bits", top_graph)
        self.assertNotIn("plio_64_bits", top_graph)

        self.assertIn("using plio_word_t = ap_uint<128>;", toppl)
        self.assertIn("constexpr int kPlioWordBits = 128;", toppl)
        self.assertIn("constexpr int kIntsPerPlioWord = 4;", toppl)
        self.assertIn(
            "constexpr int kPlioWordsPerDdrWord = kIntsPerDdrWord / kIntsPerPlioWord;",
            toppl,
        )

        self.assertIn("write_plio128_i32", gen_case)
        self.assertNotIn("write_plio64_i32", gen_case)
        self.assertIn("4 int32 per line", gen_case)
        self.assertIn("write_plio128_i32", convert)
        self.assertNotIn("write_plio64_i32", convert)

    def test_toppl_has_four_stage_dataflow_with_axis_registers(self):
        toppl = read("pl/TopPL.cpp")

        self.assertIn("void read_ddr_words(", toppl)
        self.assertIn("void pack_to_plio128(", toppl)
        self.assertIn("void unpack_from_plio128(", toppl)
        self.assertIn("void write_ddr_words(", toppl)
        self.assertIn("#pragma HLS INTERFACE axis port=to_aie register_mode=both", toppl)
        self.assertIn("#pragma HLS INTERFACE axis port=from_aie register_mode=both", toppl)
        self.assertIn("hls::stream<ddr_word_t> input_words;", toppl)
        self.assertIn("hls::stream<ddr_word_t> output_words;", toppl)
        self.assertIn("#pragma HLS STREAM variable=input_words depth=128", toppl)
        self.assertIn("#pragma HLS STREAM variable=output_words depth=128", toppl)

        self.assertRegex(
            toppl,
            r"#pragma HLS DATAFLOW\s+read_ddr_words\(input, total_words, input_words\);\s+"
            r"pack_to_plio128\(input_words, total_words, to_aie\);\s+"
            r"unpack_from_plio128\(from_aie, total_words, output_words\);\s+"
            r"write_ddr_words\(output, total_words, output_words\);",
        )

    def test_host_reports_pipeline_hardware_run_time(self):
        host = read("ps/host.cpp")

        self.assertIn("const auto hw_t0 = Clock::now();", host)
        self.assertIn("const auto hw_t1 = Clock::now();", host)
        self.assertIn("const long long hardware_run_us = elapsed_us(hw_t0, hw_t1);", host)
        self.assertIn(
            'std::printf("hardware_run_us      : %lld\\n", hardware_run_us);',
            host,
        )
        self.assertNotIn("graph_pipeline_us", host)
        self.assertNotIn("toppl_t0", host)

    def test_host_reports_aie_output_event_profile_without_replacing_wall_time(self):
        host = read("ps/host.cpp")

        self.assertIn(
            "constexpr double AIE_EVENT_COUNTER_FREQ_HZ = 1250000000.0;",
            host,
        )
        self.assertIn(
            "adf::event::io_stream_start_to_bytes_transferred_cycles",
            host,
        )
        self.assertIn("topStencil.out_plio[0]", host)
        self.assertIn("adf::event::read_profiling(aie_profile_handle)", host)
        self.assertIn("adf::event::stop_profiling(aie_profile_handle)", host)
        self.assertIn('std::printf("aie_out_event_cycles : %lld\\n", aie_out_cycles);', host)
        self.assertIn('std::printf("aie_out_cycles_row   : %.3f\\n", aie_out_cycles_per_row);', host)
        self.assertIn('std::printf("aie_out_us_est_1p25  : %.3f\\n", aie_out_us_est);', host)
        self.assertIn('std::printf("hardware_us_row      : %.3f\\n", hardware_us_per_row);', host)
        self.assertIn('std::printf("hardware_run_us      : %lld\\n", hardware_run_us);', host)

    def test_aie_input_broadcast_fifo_depths_are_tuned_for_board_trace(self):
        config = read("aie/Config.h")
        core_graph = read("aie/ProcessGraph/StencilCoreGraph.h")
        xrt_ini = read("xrt.ini")

        self.assertIn("constexpr int kInputObjectFifoDepth = 8;", config)
        self.assertIn("constexpr int kDelayedInputObjectFifoDepth = 24;", config)
        self.assertIn("constexpr int kFluxInterObjectFifoDepth = 24;", config)
        self.assertIn("constexpr int kOutputObjectFifoDepth = 16;", config)
        self.assertIn("fifo_depth(net_in_lap)     = hdiff_cfg::kInputObjectFifoDepth;", core_graph)
        self.assertIn("fifo_depth(net_in_flux1)   = hdiff_cfg::kDelayedInputObjectFifoDepth;", core_graph)
        self.assertIn("fifo_depth(net_in_flux2)   = hdiff_cfg::kDelayedInputObjectFifoDepth;", core_graph)
        self.assertIn("fifo_depth(net_lap_f1)      = hdiff_cfg::kFluxInterObjectFifoDepth;", core_graph)
        self.assertIn("fifo_depth(net_lap_f2)      = hdiff_cfg::kFluxInterObjectFifoDepth;", core_graph)
        self.assertIn("fifo_depth(net_f1_f2)       = hdiff_cfg::kFluxInterObjectFifoDepth;", core_graph)
        self.assertIn("fifo_depth(net_out)         = hdiff_cfg::kOutputObjectFifoDepth;", core_graph)
        self.assertIn("profile=true", xrt_ini)
        self.assertIn("stall_trace=all", xrt_ini)
        self.assertIn("aie_profile=true", xrt_ini)
        self.assertIn("aie_status=true", xrt_ini)


if __name__ == "__main__":
    unittest.main()
