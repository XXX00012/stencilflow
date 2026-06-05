#include <adf.h>
#include <aie_api/aie.hpp>
#include <cstdint>
#include "include.h"
#include "hdiff.h"

using namespace adf;

#define kernel_load 14

namespace {

constexpr unsigned kMaskWordsPerRow = COL / 8;

inline unsigned int make_take_zero_mask(v8int32 product) {
  return gt16(concat(product, undef_v8int32()), 0,
              0x76543210, 0xFEDCBA98,
              null_v16int32(), 0,
              0x76543210, 0xFEDCBA98);
}

} // namespace

void hdiff_com(input_buffer<int32_t>& in_ms_package,
               output_buffer<int32_t>& out_mask_package) {
  // Input product layout:
  //   [ms_left | ms_right | ms_up | ms_down]
  // Output mask layout:
  //   [mask_left | mask_right | mask_up | mask_down]

  int32_t* __restrict ms_base   = in_ms_package.data();
  int32_t* __restrict mask_base = out_mask_package.data();

  v8int32* __restrict ms_left  = (v8int32*)(ms_base + (0 * COL));
  v8int32* __restrict ms_right = (v8int32*)(ms_base + (1 * COL));
  v8int32* __restrict ms_up    = (v8int32*)(ms_base + (2 * COL));
  v8int32* __restrict ms_down  = (v8int32*)(ms_base + (3 * COL));

  int32_t* __restrict mask_left  = mask_base + (0 * kMaskWordsPerRow);
  int32_t* __restrict mask_right = mask_base + (1 * kMaskWordsPerRow);
  int32_t* __restrict mask_up    = mask_base + (2 * kMaskWordsPerRow);
  int32_t* __restrict mask_down  = mask_base + (3 * kMaskWordsPerRow);

  for (unsigned i = 0; i < COL / 8; ++i)
    chess_prepare_for_pipelining
    chess_loop_range(1, ) {
      mask_left[i]  = (int32_t)make_take_zero_mask(ms_left[i]);
      mask_right[i] = (int32_t)make_take_zero_mask(ms_right[i]);
      mask_up[i]    = (int32_t)make_take_zero_mask(ms_up[i]);
      mask_down[i]  = (int32_t)make_take_zero_mask(ms_down[i]);
    }
}
