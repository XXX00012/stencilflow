#include <adf.h>
#include <aie_api/aie.hpp>
#include <cstdint>
#include "include.h"
#include "hdiff.h"

using namespace adf;

#define kernel_load 14

namespace {

constexpr int32_t kC = -7;

} // namespace

void hdiff_update(input_buffer<int32_t>& in_flux_package,
                  psi_complete_input_buffer& in_psi_stream,
                  output_buffer<int32_t>& out_row) {
  // Complete version: update consumes the four selected flux rows belonging
  // to the same logical output row. It no longer takes left/right/up from the
  // previous firing and down from the current firing.
  //
  // Flux layout:
  //   [flux_left | flux_right | flux_up | flux_down]
  //
  // Same algebra as your three-core flux2 stage:
  //   out = psi + C * (right - left - up + down), C = -7
  //
  // The raw psi row is the center of the five-row window; if base_ptr is the
  // newest row at this firing, psi_center = base_ptr - 2*COL.

  alignas(32) int32_t ones_arr[8] = {1, 1, 1, 1, 1, 1, 1, 1};
  alignas(32) int32_t c_arr[8]    = {kC, kC, kC, kC, kC, kC, kC, kC};

  v8int32 ones_vec = *(v8int32*)ones_arr;
  v8int32 c_vec    = *(v8int32*)c_arr;

  int32_t* __restrict flux_base = in_flux_package.data();
  int32_t* __restrict psi_base  = in_psi_stream.data() - (2 * COL);
  int32_t* __restrict out_base  = out_row.data();

  v8int32* __restrict flux_left  = (v8int32*)(flux_base + (0 * COL));
  v8int32* __restrict flux_right = (v8int32*)(flux_base + (1 * COL));
  v8int32* __restrict flux_up    = (v8int32*)(flux_base + (2 * COL));
  v8int32* __restrict flux_down  = (v8int32*)(flux_base + (3 * COL));

  v8int32* __restrict psi_center = (v8int32*)psi_base;
  v8int32* __restrict out        = (v8int32*)out_base;

  for (unsigned i = 0; i < COL / 8; ++i)
    chess_prepare_for_pipelining
    chess_loop_range(1, ) {
      v8int32 psi_v   = psi_center[i];
      v8int32 left_v  = flux_left[i];
      v8int32 right_v = flux_right[i];
      v8int32 up_v    = flux_up[i];
      v8int32 down_v  = flux_down[i];

      v8acc80 acc = null_v8acc80();

      // acc = psi
      acc = lmul8(concat(psi_v, undef_v8int32()), 0, 0x76543210,
                  ones_vec, 0, 0x00000000);

      // acc += C * right
      acc = lmac8(acc,
                  concat(right_v, undef_v8int32()), 0, 0x76543210,
                  c_vec, 0, 0x00000000);

      // acc -= C * left
      acc = lmsc8(acc,
                  concat(left_v, undef_v8int32()), 0, 0x76543210,
                  c_vec, 0, 0x00000000);

      // acc -= C * up
      acc = lmsc8(acc,
                  concat(up_v, undef_v8int32()), 0, 0x76543210,
                  c_vec, 0, 0x00000000);

      // acc += C * down
      acc = lmac8(acc,
                  concat(down_v, undef_v8int32()), 0, 0x76543210,
                  c_vec, 0, 0x00000000);

      out[i] = srs(acc, 0);
    }
}
