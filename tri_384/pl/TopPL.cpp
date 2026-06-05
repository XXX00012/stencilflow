#include "../aie/Config.h"

#include <ap_int.h>
#include <hls_stream.h>

#define TRI384_VOLUME_DDR_WORDS 262144

namespace {

using ddr_word_t = ap_uint<512>;
using plio_word_t = ap_uint<64>;

constexpr int kGridRows = 256;
constexpr int kGridDepth = 64;
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
constexpr int kVolumeDdrWords = TRI384_VOLUME_DDR_WORDS;

static_assert(kCols == 256, "tri_384 TopPL expects 256 columns");
static_assert(kNumLanes == 128, "tri_384 TopPL expects 128 AIE lanes");
static_assert(kGridDepth * kLanesPerDepth == kNumLanes,
              "lane count must match depth split");
static_assert(kWarmupRows == 4, "tri_384 TopPL expects four warmup rows");
static_assert(kIterations == kRowsPerLane + kWarmupRows,
              "AIE graph run count must be useful rows plus warmup rows");
static_assert(kCols % kIntsPerDdrWord == 0,
              "512-bit DDR packing requires whole rows");
static_assert(kVolumeDdrWords == kGridDepth * kGridRows * kDdrWordsPerRow,
              "DDR word depth must match the 256x256x64 volume");

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
        plio_word_t packed = word.range((i + 1) * 64 - 1, i * 64);
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

template <int LaneOffset>
void send_lane(const ddr_word_t* input,
               int lane_base,
               hls::stream<plio_word_t>& to_aie) {
#pragma HLS INLINE off
    for (int raw_r = 0; raw_r < kIterations; ++raw_r) {
        for (int w = 0; w < kDdrWordsPerRow; ++w) {
            send_ddr_word(
                lane_input_word<LaneOffset>(input, lane_base, raw_r, w),
                to_aie);
        }
    }
}

template <int LaneOffset>
void recv_lane(ddr_word_t* output,
               int lane_base,
               hls::stream<plio_word_t>& from_aie) {
#pragma HLS INLINE off
    for (int raw_r = 0; raw_r < kIterations; ++raw_r) {
        for (int w = 0; w < kDdrWordsPerRow; ++w) {
            store_lane_output_word<LaneOffset>(
                output, lane_base, raw_r, w, recv_ddr_word(from_aie));
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
#pragma HLS INTERFACE m_axi port=input offset=slave bundle=gmem0 depth=TRI384_VOLUME_DDR_WORDS
#pragma HLS INTERFACE m_axi port=output offset=slave bundle=gmem1 depth=TRI384_VOLUME_DDR_WORDS
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

#pragma HLS DATAFLOW
    send_lane<0>(input, lane_base, to_aie0);
    send_lane<1>(input, lane_base, to_aie1);
    send_lane<2>(input, lane_base, to_aie2);
    send_lane<3>(input, lane_base, to_aie3);
    send_lane<4>(input, lane_base, to_aie4);
    send_lane<5>(input, lane_base, to_aie5);
    send_lane<6>(input, lane_base, to_aie6);
    send_lane<7>(input, lane_base, to_aie7);

    recv_lane<0>(output, lane_base, from_aie0);
    recv_lane<1>(output, lane_base, from_aie1);
    recv_lane<2>(output, lane_base, from_aie2);
    recv_lane<3>(output, lane_base, from_aie3);
    recv_lane<4>(output, lane_base, from_aie4);
    recv_lane<5>(output, lane_base, from_aie5);
    recv_lane<6>(output, lane_base, from_aie6);
    recv_lane<7>(output, lane_base, from_aie7);
}

} // extern "C"
