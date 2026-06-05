#include <adf.h>
#include <aie_api/aie.hpp>
#include <cstdint>
#include "include.h"
#include "hdiff.h"

using namespace adf;

#define kernel_load 14

namespace {

// factor * pos - factor * neg
inline v8int32 mul_sub_vec(v8int32 factor,
                           v16int32 pos_buf, int pos_offset,
                           v16int32 neg_buf, int neg_offset) {
  v8acc80 acc =
      lmul8(pos_buf, pos_offset, 0x76543210,
            factor, 0, 0x76543210);

  acc =
      lmsc8(acc, neg_buf, neg_offset, 0x76543210,
            factor, 0, 0x76543210);

  return srs(acc, 0);
}

} // namespace

void hdiff_ms(ms_complete_input_buffer& in_window,
              input_buffer<int32_t>& in_sub_package,
              output_buffer<int32_t>& out_ms_package) {
  // Complete version: no vertical product is shared across firings.
  //
  // The same 5-row time as lap_complete is used. When base_ptr is the newest
  // row, the current output row is base - 2*COL. We only need raw rows:
  //   row_up   = base - 3*COL
  //   row_mid  = base - 2*COL
  //   row_down = base - 1*COL
  //
  // Input sub layout:
  //   [sub_left | sub_right | sub_up | sub_down]
  //
  // Output product layout:
  //   [ms_left | ms_right | ms_up | ms_down]

  int32_t* __restrict base_ptr      = in_window.data();
  int32_t* __restrict row_up_base   = base_ptr - (3 * COL);
  int32_t* __restrict row_mid_base  = base_ptr - (2 * COL);
  int32_t* __restrict row_down_base = base_ptr - (1 * COL);

  int32_t* __restrict sub_base = in_sub_package.data();
  int32_t* __restrict out_base = out_ms_package.data();

  v8int32* __restrict sub_left  = (v8int32*)(sub_base + (0 * COL));
  v8int32* __restrict sub_right = (v8int32*)(sub_base + (1 * COL));
  v8int32* __restrict sub_up    = (v8int32*)(sub_base + (2 * COL));
  v8int32* __restrict sub_down  = (v8int32*)(sub_base + (3 * COL));

  v8int32* __restrict ms_left   = (v8int32*)(out_base + (0 * COL));
  v8int32* __restrict ms_right  = (v8int32*)(out_base + (1 * COL));
  v8int32* __restrict ms_up     = (v8int32*)(out_base + (2 * COL));
  v8int32* __restrict ms_down   = (v8int32*)(out_base + (3 * COL));

  v8int32* __restrict up_ptr   = (v8int32*)row_up_base;
  v8int32* __restrict mid_ptr  = (v8int32*)row_mid_base;
  v8int32* __restrict down_ptr = (v8int32*)row_down_base;

  v16int32 up_buf   = null_v16int32();
  v16int32 mid_buf  = null_v16int32();
  v16int32 down_buf = null_v16int32();

  up_buf   = upd_w(up_buf,   0, *up_ptr++);
  up_buf   = upd_w(up_buf,   1, *up_ptr);
  mid_buf  = upd_w(mid_buf,  0, *mid_ptr++);
  mid_buf  = upd_w(mid_buf,  1, *mid_ptr);
  down_buf = upd_w(down_buf, 0, *down_ptr++);
  down_buf = upd_w(down_buf, 1, *down_ptr);

  for (unsigned i = 0; i < COL / 8; ++i)
    chess_prepare_for_pipelining
    chess_loop_range(1, ) {
      // left:  sub_left  * (psi_mid - psi_left)
      ms_left[i] =
          mul_sub_vec(sub_left[i],
                      mid_buf, 2,
                      mid_buf, 1);

      // right: sub_right * (psi_right - psi_mid)
      ms_right[i] =
          mul_sub_vec(sub_right[i],
                      mid_buf, 3,
                      mid_buf, 2);

      // up:    sub_up    * (psi_mid - psi_up)
      ms_up[i] =
          mul_sub_vec(sub_up[i],
                      mid_buf, 2,
                      up_buf, 2);

      // down:  sub_down  * (psi_down - psi_mid)
      ms_down[i] =
          mul_sub_vec(sub_down[i],
                      down_buf, 2,
                      mid_buf, 2);

      up_ptr   = ((v8int32*)row_up_base)   + i + 1;
      mid_ptr  = ((v8int32*)row_mid_base)  + i + 1;
      down_ptr = ((v8int32*)row_down_base) + i + 1;

      up_buf   = upd_w(up_buf,   0, *up_ptr++);
      up_buf   = upd_w(up_buf,   1, *up_ptr);
      mid_buf  = upd_w(mid_buf,  0, *mid_ptr++);
      mid_buf  = upd_w(mid_buf,  1, *mid_ptr);
      down_buf = upd_w(down_buf, 0, *down_ptr++);
      down_buf = upd_w(down_buf, 1, *down_ptr);
    }
}
