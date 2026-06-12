#include "../aie/Config.h"

#include <ap_int.h>
#include <hls_stream.h>

#define TRI_TOPPL_MAX_FIRINGS 16384
#define TRI_TOPPL_MAX_INPUT_DDR_WORDS 524288
#define TRI_TOPPL_MAX_OUTPUT_DDR_WORDS 524288

namespace {

using ddr_word_t = ap_uint<512>;
using plio_word_t = ap_uint<128>;

constexpr int kPlioWordBits = 128;
constexpr int kIntsPerPlioWord = 4;
constexpr int kIntsPerDdrWord = 16;
constexpr int kPlioWordsPerDdrWord = kIntsPerDdrWord / kIntsPerPlioWord;
constexpr int kDdrWordsPerRow = hdiff_cfg::kRowElems / kIntsPerDdrWord;

static_assert(hdiff_cfg::kRowElems % kIntsPerDdrWord == 0,
              "512-bit DDR packing requires whole rows");
static_assert(kPlioWordsPerDdrWord * kPlioWordBits == 512,
              "four 128-bit PLIO words must exactly cover one DDR word");
static_assert(hdiff_cfg::kLapInputSampleElems % kIntsPerDdrWord == 0,
              "input sample must be DDR-word aligned");
static_assert(hdiff_cfg::kLapInputMarginElems % kIntsPerDdrWord == 0,
              "input margin must be DDR-word aligned");
static_assert(hdiff_cfg::kOutputElems % kIntsPerDdrWord == 0,
              "output sample must be DDR-word aligned");

void write_plio_word(ddr_word_t word,
                     int chunk,
                     hls::stream<plio_word_t>& out) {
#pragma HLS INLINE
    const int hi = (chunk + 1) * kPlioWordBits - 1;
    const int lo = chunk * kPlioWordBits;
    out.write(word.range(hi, lo));
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
           int firing_cnt,
           hls::stream<plio_word_t>& to_aie,
           hls::stream<plio_word_t>& from_aie) {
#pragma HLS INTERFACE m_axi port=input offset=slave bundle=gmem0 depth=TRI_TOPPL_MAX_INPUT_DDR_WORDS
#pragma HLS INTERFACE m_axi port=output offset=slave bundle=gmem1 depth=TRI_TOPPL_MAX_OUTPUT_DDR_WORDS
#pragma HLS INTERFACE s_axilite port=input bundle=control
#pragma HLS INTERFACE s_axilite port=output bundle=control
#pragma HLS INTERFACE s_axilite port=firing_cnt bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS INTERFACE axis port=to_aie register_mode=both
#pragma HLS INTERFACE axis port=from_aie register_mode=both

    const int input_elems =
        firing_cnt * hdiff_cfg::kLapInputSampleElems +
        hdiff_cfg::kLapInputMarginElems;
    const int output_elems = firing_cnt * hdiff_cfg::kOutputElems;
    const int input_words = input_elems / kIntsPerDdrWord;
    const int output_words = output_elems / kIntsPerDdrWord;

    hls::stream<ddr_word_t> input_words_stream;
    hls::stream<ddr_word_t> output_words_stream;

#pragma HLS STREAM variable=input_words_stream depth=128
#pragma HLS STREAM variable=output_words_stream depth=128

#pragma HLS DATAFLOW
    read_ddr_words(input, input_words, input_words_stream);
    pack_to_plio128(input_words_stream, input_words, to_aie);
    unpack_from_plio128(from_aie, output_words, output_words_stream);
    write_ddr_words(output, output_words, output_words_stream);
}

} // extern "C"
