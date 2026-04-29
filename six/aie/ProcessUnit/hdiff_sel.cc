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

void hdiff_sel(
    input_buffer<int32_t>& in_mask_package,
    input_buffer<int32_t>& in_sub_package,
    output_buffer<int32_t>& out_package) {

  int32_t* mask_base = in_mask_package.data();
  int32_t* sub_base  = in_sub_package.data();
  int32_t* out_base  = out_package.data();

  const int32_t* in_mask_vert  = mask_base + (0 * kMaskWordsPerRow);
  const int32_t* in_mask_left  = mask_base + (1 * kMaskWordsPerRow);
  const int32_t* in_mask_right = mask_base + (2 * kMaskWordsPerRow);

  v8int32* __restrict in_sub_down  = (v8int32*)(sub_base + (0 * COL));
  v8int32* __restrict in_sub_left  = (v8int32*)(sub_base + (1 * COL));
  v8int32* __restrict in_sub_right = (v8int32*)(sub_base + (2 * COL));
  v8int32* __restrict in_sub_up    = (v8int32*)(sub_base + (3 * COL));

  v8int32* __restrict out_predown = (v8int32*)(out_base + (0 * COL));
  v8int32* __restrict out_left    = (v8int32*)(out_base + (1 * COL));
  v8int32* __restrict out_right   = (v8int32*)(out_base + (2 * COL));
  v8int32* __restrict out_curup   = (v8int32*)(out_base + (3 * COL));

  for (unsigned i = 0; i < COL / 8; ++i)
    chess_prepare_for_pipelining
    chess_loop_range(1, ) {

      unsigned int mask_vert  = (unsigned int)in_mask_vert[i];
      unsigned int mask_left  = (unsigned int)in_mask_left[i];
      unsigned int mask_right = (unsigned int)in_mask_right[i];

      v8int32 sub_down  = in_sub_down[i];
      v8int32 sub_left  = in_sub_left[i];
      v8int32 sub_right = in_sub_right[i];
      v8int32 sub_up    = in_sub_up[i];

      out_predown[i] = apply_mask(mask_vert,  sub_down);
      out_left[i]    = apply_mask(mask_left,  sub_left);
      out_right[i]   = apply_mask(mask_right, sub_right);
      out_curup[i]   = apply_mask(mask_vert,  sub_up);
    }
}
