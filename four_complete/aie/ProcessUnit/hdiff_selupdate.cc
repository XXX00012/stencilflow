#include <adf.h>
#include <aie_api/aie.hpp>
#include <cstdint>
#include "include.h"
#include "hdiff.h"

using namespace adf;

#define kernel_load 14

namespace {

constexpr int32_t kC = -7;
constexpr unsigned kMaskWordsPerRow = COL / 8;

inline v16int32 select_flux(unsigned int take_mask, v8int32 diff) {
  return select16(take_mask,
                  concat(diff, undef_v8int32()),
                  null_v16int32());
}

} // namespace

void hdiff_selupdate(input_buffer<int32_t>& in_mask_package,
                     input_buffer<int32_t>& in_sub_package,
                     psi_complete_input_buffer& in_psi_stream,
                     output_buffer<int32_t>& out_row) {
  // Fused sel+update.
  //
  // sel uses:
  //   mask layout [mask_left | mask_right | mask_up | mask_down]
  //   sub layout  [sub_left  | sub_right  | sub_up  | sub_down]
  //
  // update still reads the raw center psi row from an external stream, matching
  // six_complete's update kernel:
  //   out = psi + C * (right - left - up + down), C = -7

  alignas(32) int32_t ones_arr[8] = {1, 1, 1, 1, 1, 1, 1, 1};
  alignas(32) int32_t c_arr[8]    = {kC, kC, kC, kC, kC, kC, kC, kC};

  v8int32 ones_vec = *(v8int32*)ones_arr;
  v8int32 c_vec    = *(v8int32*)c_arr;

  int32_t* __restrict mask_base = in_mask_package.data();
  int32_t* __restrict sub_base  = in_sub_package.data();
  int32_t* __restrict psi_base  = in_psi_stream.data() - (2 * COL);
  int32_t* __restrict out_base  = out_row.data();

  const int32_t* __restrict mask_left  = mask_base + (0 * kMaskWordsPerRow);
  const int32_t* __restrict mask_right = mask_base + (1 * kMaskWordsPerRow);
  const int32_t* __restrict mask_up    = mask_base + (2 * kMaskWordsPerRow);
  const int32_t* __restrict mask_down  = mask_base + (3 * kMaskWordsPerRow);

  v8int32* __restrict sub_left  = (v8int32*)(sub_base + (0 * COL));
  v8int32* __restrict sub_right = (v8int32*)(sub_base + (1 * COL));
  v8int32* __restrict sub_up    = (v8int32*)(sub_base + (2 * COL));
  v8int32* __restrict sub_down  = (v8int32*)(sub_base + (3 * COL));

  v8int32* __restrict psi_center = (v8int32*)psi_base;
  v8int32* __restrict out        = (v8int32*)out_base;

  for (unsigned i = 0; i < COL / 8; ++i)
    chess_prepare_for_pipelining
    chess_loop_range(1, ) {
      v16int32 left_v =
          select_flux((unsigned int)mask_left[i], sub_left[i]);
      v16int32 right_v =
          select_flux((unsigned int)mask_right[i], sub_right[i]);
      v16int32 up_v =
          select_flux((unsigned int)mask_up[i], sub_up[i]);
      v16int32 down_v =
          select_flux((unsigned int)mask_down[i], sub_down[i]);

      v8acc80 acc = null_v8acc80();

      acc = lmul8(concat(psi_center[i], undef_v8int32()), 0, 0x76543210,
                  ones_vec, 0, 0x00000000);

      acc = lmac8(acc,
                  right_v, 0, 0x76543210,
                  c_vec, 0, 0x00000000);

      acc = lmsc8(acc,
                  left_v, 0, 0x76543210,
                  c_vec, 0, 0x00000000);

      acc = lmsc8(acc,
                  up_v, 0, 0x76543210,
                  c_vec, 0, 0x00000000);

      acc = lmac8(acc,
                  down_v, 0, 0x76543210,
                  c_vec, 0, 0x00000000);

      out[i] = srs(acc, 0);
    }
}
