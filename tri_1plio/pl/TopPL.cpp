#include "../aie/Config.h"

#include <ap_int.h>
#include <hls_stream.h>

#define TRI1PLIO_MAX_ITER 16384
#define TRI1PLIO_MAX_DDR_WORDS 262144

namespace {

using ddr_word_t = ap_uint<512>;
using plio_word_t = ap_uint<128>;

constexpr int kCols = hdiff_cfg::kRowElems;
constexpr int kPlioWordBits = 128;
constexpr int kIntsPerPlioWord = 4;
constexpr int kIntsPerDdrWord = 16;
constexpr int kPlioWordsPerDdrWord = kIntsPerDdrWord / kIntsPerPlioWord;
constexpr int kDdrWordsPerRow = kCols / kIntsPerDdrWord;
constexpr int kBoardGridRows = 256;
constexpr int kBoardGridDepth = 64;
constexpr int kBoardIter = kBoardGridRows * kBoardGridDepth;

static_assert(kCols == 256, "tri_1plio TopPL expects 256 columns");
static_assert(kCols % kIntsPerDdrWord == 0,
              "512-bit DDR packing requires whole rows");
static_assert(kPlioWordBits * kPlioWordsPerDdrWord == 512,
              "PLIO packing must exactly cover one 512-bit DDR word");
static_assert(kBoardIter == TRI1PLIO_MAX_ITER,
              "tri_1plio board iter count must match max iter");
static_assert(kBoardIter * kDdrWordsPerRow == TRI1PLIO_MAX_DDR_WORDS,
              "tri_1plio DDR depth must cover board input rows");

void write_plio_word(ddr_word_t word,
                     int chunk,
                     hls::stream<plio_word_t>& out) {
#pragma HLS INLINE
    const int hi = (chunk + 1) * kPlioWordBits - 1;
    const int lo = chunk * kPlioWordBits;
    const plio_word_t packed = word.range(hi, lo);
    out.write(packed);
}

void read_ddr_words(const ddr_word_t* input,
                    int total_words,
                    hls::stream<ddr_word_t>& out_words) {
#pragma HLS INLINE off
    for (int i = 0; i < total_words; ++i) {
#pragma HLS PIPELINE II=1
        out_words.write(input[i]);
    }
}

void pack_to_plio128(hls::stream<ddr_word_t>& in_words,
                     int total_words,
                     hls::stream<plio_word_t>& to_aie) {
#pragma HLS INLINE off
    for (int word_idx = 0; word_idx < total_words; ++word_idx) {
        const ddr_word_t word = in_words.read();
        for (int chunk = 0; chunk < kPlioWordsPerDdrWord; ++chunk) {
#pragma HLS PIPELINE II=1
            write_plio_word(word, chunk, to_aie);
        }
    }
}

void unpack_from_plio128(hls::stream<plio_word_t>& from_aie,
                         int total_words,
                         hls::stream<ddr_word_t>& out_words) {
#pragma HLS INLINE off
    for (int word_idx = 0; word_idx < total_words; ++word_idx) {
        ddr_word_t word = 0;
        for (int chunk = 0; chunk < kPlioWordsPerDdrWord; ++chunk) {
#pragma HLS PIPELINE II=1
            const plio_word_t packed = from_aie.read();
            const int hi = (chunk + 1) * kPlioWordBits - 1;
            const int lo = chunk * kPlioWordBits;
            word.range(hi, lo) = packed;
        }
        out_words.write(word);
    }
}

void write_ddr_words(ddr_word_t* output,
                     int total_words,
                     hls::stream<ddr_word_t>& in_words) {
#pragma HLS INLINE off
    for (int i = 0; i < total_words; ++i) {
#pragma HLS PIPELINE II=1
        output[i] = in_words.read();
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
#pragma HLS INTERFACE axis port=to_aie register_mode=both
#pragma HLS INTERFACE axis port=from_aie register_mode=both

    const int total_words = iter_cnt * kDdrWordsPerRow;

    hls::stream<ddr_word_t> input_words;
    hls::stream<ddr_word_t> output_words;

#pragma HLS STREAM variable=input_words depth=128
#pragma HLS STREAM variable=output_words depth=128

#pragma HLS DATAFLOW
    read_ddr_words(input, total_words, input_words);
    pack_to_plio128(input_words, total_words, to_aie);
    unpack_from_plio128(from_aie, total_words, output_words);
    write_ddr_words(output, total_words, output_words);
}

} // extern "C"
