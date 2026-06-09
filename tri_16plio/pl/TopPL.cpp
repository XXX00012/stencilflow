#include "../aie/Config.h"

#include <ap_int.h>
#include <hls_stream.h>

#define TRI_VOLUME_DDR_WORDS 32768

namespace {

using ddr_word_t = ap_uint<512>;
using plio_word_t = ap_uint<64>;

constexpr int kGridRows = 256;
constexpr int kGridDepth = 8;
constexpr int kCols = hdiff_cfg::kRowElems;
constexpr int kNumLanes = hdiff_cfg::kNumLanes;
constexpr int kLanesPerDepth = 2;
constexpr int kRowsPerLane = kGridRows / kLanesPerDepth;
constexpr int kWarmupRows = hdiff_cfg::kLapWarmupIterations;
constexpr int kStencilRadiusRows = kWarmupRows / 2;
constexpr int kIterations = hdiff_cfg::kDefaultIterations;
constexpr int kLanesPerPl = 8;

constexpr int kIntsPerPlioWord = 2;
constexpr int kIntsPerDdrWord = 16;
constexpr int kPlioWordsPerDdrWord = kIntsPerDdrWord / kIntsPerPlioWord;
constexpr int kDdrWordsPerRow = kCols / kIntsPerDdrWord;
constexpr int kWordsPerLane = kIterations * kDdrWordsPerRow;
constexpr int kVolumeDdrWords = TRI_VOLUME_DDR_WORDS;

static_assert(kCols == 256, "tri_16plio TopPL expects 256 columns");
static_assert(kNumLanes == 16, "tri_16plio TopPL expects 16 AIE lanes");
static_assert(kGridDepth * kLanesPerDepth == kNumLanes,
              "lane count must match depth split");
static_assert(kWarmupRows == 4, "tri_16plio TopPL expects four warmup rows");
static_assert(kIterations == kRowsPerLane + kWarmupRows,
              "AIE graph run count must be useful rows plus warmup rows");
static_assert(kCols % kIntsPerDdrWord == 0,
              "512-bit DDR packing requires whole rows");
static_assert(kVolumeDdrWords == kGridDepth * kGridRows * kDdrWordsPerRow,
              "DDR word depth must match the input volume");

int volume_word_index(int depth, int row, int ddr_word_col) {
#pragma HLS INLINE
    return (depth * kGridRows + row) * kDdrWordsPerRow + ddr_word_col;
}

int lane_depth(int lane) {
#pragma HLS INLINE
    return lane / kLanesPerDepth;
}

int lane_part(int lane) {
#pragma HLS INLINE
    return lane % kLanesPerDepth;
}

void send_ddr_word(ddr_word_t word, hls::stream<plio_word_t>& out) {
#pragma HLS INLINE
    for (int i = 0; i < kPlioWordsPerDdrWord; ++i) {
#pragma HLS PIPELINE II=1
        const plio_word_t packed = word.range((i + 1) * 64 - 1, i * 64);
        out.write(packed);
    }
}

ddr_word_t recv_ddr_word(hls::stream<plio_word_t>& in) {
#pragma HLS INLINE
    ddr_word_t word = 0;
    for (int i = 0; i < kPlioWordsPerDdrWord; ++i) {
#pragma HLS PIPELINE II=1
        const plio_word_t packed = in.read();
        word.range((i + 1) * 64 - 1, i * 64) = packed;
    }
    return word;
}

template <int LaneOffset>
ddr_word_t lane_input_word(const ddr_word_t* input,
                           int lane_base,
                           int raw_r,
                           int ddr_word_col) {
#pragma HLS INLINE
    const int lane = lane_base + LaneOffset;
    const int depth = lane_depth(lane);
    const int part = lane_part(lane);
    const int out_start_row = part * kRowsPerLane;
    const int src_start_row = out_start_row - kStencilRadiusRows;
    const int src_row = src_start_row + raw_r;

    if ((src_row >= 0) && (src_row < kGridRows)) {
        return input[volume_word_index(depth, src_row, ddr_word_col)];
    }
    return 0;
}

template <int LaneOffset>
void store_lane_output_word(ddr_word_t* output,
                            int lane_base,
                            int raw_r,
                            int ddr_word_col,
                            ddr_word_t word) {
#pragma HLS INLINE
    const int lane = lane_base + LaneOffset;
    const int depth = lane_depth(lane);
    const int part = lane_part(lane);
    const int out_start_row = part * kRowsPerLane;
    const int out_row = out_start_row + raw_r - kWarmupRows;

    if (raw_r >= kWarmupRows) {
        output[volume_word_index(depth, out_row, ddr_word_col)] = word;
    }
}

void read_input_words(const ddr_word_t* input,
                      int lane_base,
                      hls::stream<ddr_word_t>& lane0,
                      hls::stream<ddr_word_t>& lane1,
                      hls::stream<ddr_word_t>& lane2,
                      hls::stream<ddr_word_t>& lane3,
                      hls::stream<ddr_word_t>& lane4,
                      hls::stream<ddr_word_t>& lane5,
                      hls::stream<ddr_word_t>& lane6,
                      hls::stream<ddr_word_t>& lane7) {
#pragma HLS INLINE off
    for (int raw_r = 0; raw_r < kIterations; ++raw_r) {
        for (int w = 0; w < kDdrWordsPerRow; ++w) {
            lane0.write(lane_input_word<0>(input, lane_base, raw_r, w));
            lane1.write(lane_input_word<1>(input, lane_base, raw_r, w));
            lane2.write(lane_input_word<2>(input, lane_base, raw_r, w));
            lane3.write(lane_input_word<3>(input, lane_base, raw_r, w));
            lane4.write(lane_input_word<4>(input, lane_base, raw_r, w));
            lane5.write(lane_input_word<5>(input, lane_base, raw_r, w));
            lane6.write(lane_input_word<6>(input, lane_base, raw_r, w));
            lane7.write(lane_input_word<7>(input, lane_base, raw_r, w));
        }
    }
}

void send_lane_words(hls::stream<ddr_word_t>& lane_words,
                     hls::stream<plio_word_t>& to_aie) {
#pragma HLS INLINE off
    for (int i = 0; i < kWordsPerLane; ++i) {
        send_ddr_word(lane_words.read(), to_aie);
    }
}

void recv_lane_words(hls::stream<plio_word_t>& from_aie,
                     hls::stream<ddr_word_t>& lane_words) {
#pragma HLS INLINE off
    for (int i = 0; i < kWordsPerLane; ++i) {
        lane_words.write(recv_ddr_word(from_aie));
    }
}

void write_output_words(ddr_word_t* output,
                        int lane_base,
                        hls::stream<ddr_word_t>& lane0,
                        hls::stream<ddr_word_t>& lane1,
                        hls::stream<ddr_word_t>& lane2,
                        hls::stream<ddr_word_t>& lane3,
                        hls::stream<ddr_word_t>& lane4,
                        hls::stream<ddr_word_t>& lane5,
                        hls::stream<ddr_word_t>& lane6,
                        hls::stream<ddr_word_t>& lane7) {
#pragma HLS INLINE off
    for (int raw_r = 0; raw_r < kIterations; ++raw_r) {
        for (int w = 0; w < kDdrWordsPerRow; ++w) {
            store_lane_output_word<0>(output, lane_base, raw_r, w, lane0.read());
            store_lane_output_word<1>(output, lane_base, raw_r, w, lane1.read());
            store_lane_output_word<2>(output, lane_base, raw_r, w, lane2.read());
            store_lane_output_word<3>(output, lane_base, raw_r, w, lane3.read());
            store_lane_output_word<4>(output, lane_base, raw_r, w, lane4.read());
            store_lane_output_word<5>(output, lane_base, raw_r, w, lane5.read());
            store_lane_output_word<6>(output, lane_base, raw_r, w, lane6.read());
            store_lane_output_word<7>(output, lane_base, raw_r, w, lane7.read());
        }
    }
}

} // namespace

extern "C" {

void TopPL(const ddr_word_t* input,
           ddr_word_t* output,
           int lane_base,
           hls::stream<plio_word_t>& to_aie0,
           hls::stream<plio_word_t>& to_aie1,
           hls::stream<plio_word_t>& to_aie2,
           hls::stream<plio_word_t>& to_aie3,
           hls::stream<plio_word_t>& to_aie4,
           hls::stream<plio_word_t>& to_aie5,
           hls::stream<plio_word_t>& to_aie6,
           hls::stream<plio_word_t>& to_aie7,
           hls::stream<plio_word_t>& from_aie0,
           hls::stream<plio_word_t>& from_aie1,
           hls::stream<plio_word_t>& from_aie2,
           hls::stream<plio_word_t>& from_aie3,
           hls::stream<plio_word_t>& from_aie4,
           hls::stream<plio_word_t>& from_aie5,
           hls::stream<plio_word_t>& from_aie6,
           hls::stream<plio_word_t>& from_aie7) {
#pragma HLS INTERFACE m_axi port=input offset=slave bundle=gmem0 depth=TRI_VOLUME_DDR_WORDS
#pragma HLS INTERFACE m_axi port=output offset=slave bundle=gmem1 depth=TRI_VOLUME_DDR_WORDS
#pragma HLS INTERFACE s_axilite port=input bundle=control
#pragma HLS INTERFACE s_axilite port=output bundle=control
#pragma HLS INTERFACE s_axilite port=lane_base bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS INTERFACE axis port=to_aie0
#pragma HLS INTERFACE axis port=to_aie1
#pragma HLS INTERFACE axis port=to_aie2
#pragma HLS INTERFACE axis port=to_aie3
#pragma HLS INTERFACE axis port=to_aie4
#pragma HLS INTERFACE axis port=to_aie5
#pragma HLS INTERFACE axis port=to_aie6
#pragma HLS INTERFACE axis port=to_aie7
#pragma HLS INTERFACE axis port=from_aie0
#pragma HLS INTERFACE axis port=from_aie1
#pragma HLS INTERFACE axis port=from_aie2
#pragma HLS INTERFACE axis port=from_aie3
#pragma HLS INTERFACE axis port=from_aie4
#pragma HLS INTERFACE axis port=from_aie5
#pragma HLS INTERFACE axis port=from_aie6
#pragma HLS INTERFACE axis port=from_aie7

    hls::stream<ddr_word_t> in0;
    hls::stream<ddr_word_t> in1;
    hls::stream<ddr_word_t> in2;
    hls::stream<ddr_word_t> in3;
    hls::stream<ddr_word_t> in4;
    hls::stream<ddr_word_t> in5;
    hls::stream<ddr_word_t> in6;
    hls::stream<ddr_word_t> in7;
    hls::stream<ddr_word_t> out0;
    hls::stream<ddr_word_t> out1;
    hls::stream<ddr_word_t> out2;
    hls::stream<ddr_word_t> out3;
    hls::stream<ddr_word_t> out4;
    hls::stream<ddr_word_t> out5;
    hls::stream<ddr_word_t> out6;
    hls::stream<ddr_word_t> out7;

#pragma HLS STREAM variable=in0 depth=16
#pragma HLS STREAM variable=in1 depth=16
#pragma HLS STREAM variable=in2 depth=16
#pragma HLS STREAM variable=in3 depth=16
#pragma HLS STREAM variable=in4 depth=16
#pragma HLS STREAM variable=in5 depth=16
#pragma HLS STREAM variable=in6 depth=16
#pragma HLS STREAM variable=in7 depth=16
#pragma HLS STREAM variable=out0 depth=16
#pragma HLS STREAM variable=out1 depth=16
#pragma HLS STREAM variable=out2 depth=16
#pragma HLS STREAM variable=out3 depth=16
#pragma HLS STREAM variable=out4 depth=16
#pragma HLS STREAM variable=out5 depth=16
#pragma HLS STREAM variable=out6 depth=16
#pragma HLS STREAM variable=out7 depth=16

#pragma HLS DATAFLOW
    read_input_words(input, lane_base, in0, in1, in2, in3, in4, in5, in6, in7);

    send_lane_words(in0, to_aie0);
    send_lane_words(in1, to_aie1);
    send_lane_words(in2, to_aie2);
    send_lane_words(in3, to_aie3);
    send_lane_words(in4, to_aie4);
    send_lane_words(in5, to_aie5);
    send_lane_words(in6, to_aie6);
    send_lane_words(in7, to_aie7);

    recv_lane_words(from_aie0, out0);
    recv_lane_words(from_aie1, out1);
    recv_lane_words(from_aie2, out2);
    recv_lane_words(from_aie3, out3);
    recv_lane_words(from_aie4, out4);
    recv_lane_words(from_aie5, out5);
    recv_lane_words(from_aie6, out6);
    recv_lane_words(from_aie7, out7);

    write_output_words(output, lane_base, out0, out1, out2, out3,
                       out4, out5, out6, out7);
}

} // extern "C"
