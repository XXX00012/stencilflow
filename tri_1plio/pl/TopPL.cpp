#include "../aie/Config.h"

#include <ap_int.h>
#include <hls_stream.h>

#define TRI1PLIO_MAX_ITER 512
#define TRI1PLIO_MAX_DDR_WORDS 8192

namespace {

using ddr_word_t = ap_uint<512>;
using plio_word_t = ap_uint<64>;

constexpr int kCols = hdiff_cfg::kRowElems;
constexpr int kWarmupRows = hdiff_cfg::kLapWarmupIterations;
constexpr int kIntsPerPlioWord = 2;
constexpr int kIntsPerDdrWord = 16;
constexpr int kPlioWordsPerDdrWord = kIntsPerDdrWord / kIntsPerPlioWord;
constexpr int kDdrWordsPerRow = kCols / kIntsPerDdrWord;

static_assert(kCols == 256, "tri_1plio TopPL expects 256 columns");
static_assert(kCols % kIntsPerDdrWord == 0,
              "512-bit DDR packing requires whole rows");

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

void send_input(const ddr_word_t* input,
                int iter_cnt,
                hls::stream<plio_word_t>& to_aie) {
#pragma HLS INLINE off
    for (int r = 0; r < iter_cnt; ++r) {
        for (int w = 0; w < kDdrWordsPerRow; ++w) {
            send_ddr_word(input[r * kDdrWordsPerRow + w], to_aie);
        }
    }
}

void recv_output(ddr_word_t* output,
                 int iter_cnt,
                 hls::stream<plio_word_t>& from_aie) {
#pragma HLS INLINE off
    for (int r = 0; r < iter_cnt; ++r) {
        for (int w = 0; w < kDdrWordsPerRow; ++w) {
            const ddr_word_t word = recv_ddr_word(from_aie);
            if (r >= kWarmupRows) {
                output[(r - kWarmupRows) * kDdrWordsPerRow + w] = word;
            }
        }
    }
}

} // namespace

extern "C" {

void TopPL(const ddr_word_t* input,
           ddr_word_t* output,
           int iter_cnt,
           hls::stream<plio_word_t>& to_aie,
           hls::stream<plio_word_t>& from_aie) {
#pragma HLS INTERFACE m_axi port=input offset=slave bundle=gmem0 depth=TRI1PLIO_MAX_DDR_WORDS
#pragma HLS INTERFACE m_axi port=output offset=slave bundle=gmem1 depth=TRI1PLIO_MAX_DDR_WORDS
#pragma HLS INTERFACE s_axilite port=input bundle=control
#pragma HLS INTERFACE s_axilite port=output bundle=control
#pragma HLS INTERFACE s_axilite port=iter_cnt bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS INTERFACE axis port=to_aie
#pragma HLS INTERFACE axis port=from_aie

#pragma HLS DATAFLOW
    send_input(input, iter_cnt, to_aie);
    recv_output(output, iter_cnt, from_aie);
}

} // extern "C"
