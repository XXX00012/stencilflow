#include <adf.h>
#include <aie_api/aie.hpp>
#include <cstdint>
#include "include.h"
#include "hdiff.h"

using namespace adf;

#define kernel_load 14

namespace {

constexpr unsigned kMaskWordsPerRow = COL / 8;

inline v8int32 apply_mask(unsigned int take_zero_mask, v8int32 diff) {
  v16int32 selected =
      select16(take_zero_mask,
               concat(diff, undef_v8int32()),
               null_v16int32());
  return ext_w(selected, 0);
}

} // namespace

void hdiff_sel(input_buffer<int32_t>& in_mask_package,
               input_buffer<int32_t>& in_sub_package,
               output_buffer<int32_t>& out_flux_package) {
  // Mask layout:
  //   [mask_left | mask_right | mask_up | mask_down]
  // Sub layout:
  //   [sub_left | sub_right | sub_up | sub_down]
  // Output selected flux layout:
  //   [flux_left | flux_right | flux_up | flux_down]

  int32_t* __restrict mask_base = in_mask_package.data();
  int32_t* __restrict sub_base  = in_sub_package.data();
  int32_t* __restrict out_base  = out_flux_package.data();

  const int32_t* __restrict mask_left  = mask_base + (0 * kMaskWordsPerRow);
  const int32_t* __restrict mask_right = mask_base + (1 * kMaskWordsPerRow);
  const int32_t* __restrict mask_up    = mask_base + (2 * kMaskWordsPerRow);
  const int32_t* __restrict mask_down  = mask_base + (3 * kMaskWordsPerRow);

  v8int32* __restrict sub_left  = (v8int32*)(sub_base + (0 * COL));
  v8int32* __restrict sub_right = (v8int32*)(sub_base + (1 * COL));
  v8int32* __restrict sub_up    = (v8int32*)(sub_base + (2 * COL));
  v8int32* __restrict sub_down  = (v8int32*)(sub_base + (3 * COL));

  v8int32* __restrict out_left  = (v8int32*)(out_base + (0 * COL));
  v8int32* __restrict out_right = (v8int32*)(out_base + (1 * COL));
  v8int32* __restrict out_up    = (v8int32*)(out_base + (2 * COL));
  v8int32* __restrict out_down  = (v8int32*)(out_base + (3 * COL));

  for (unsigned i = 0; i < COL / 8; ++i)
    chess_prepare_for_pipelining
    chess_loop_range(1, ) {
      out_left[i]  = apply_mask((unsigned int)mask_left[i],  sub_left[i]);
      out_right[i] = apply_mask((unsigned int)mask_right[i], sub_right[i]);
      out_up[i]    = apply_mask((unsigned int)mask_up[i],    sub_up[i]);
      out_down[i]  = apply_mask((unsigned int)mask_down[i],  sub_down[i]);
    }
}
