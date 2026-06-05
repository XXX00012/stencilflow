#include <adf.h>
#include <aie_api/aie.hpp>
#include <cstdint>
#include "include.h"
#include "hdiff.h"

using namespace adf;

#define kernel_load 14

namespace {

inline v8int32 sub_vec(v8int32 a, v8int32 b) {
  v16int32 tmp =
      sub16(concat(a, undef_v8int32()), 0, 0x76543210, 0xFEDCBA98,
            concat(b, undef_v8int32()), 0, 0x76543210, 0xFEDCBA98);
  return ext_w(tmp, 0);
}

} // namespace

void hdiff_sub(sub_lap_input_buffer& in_lap_package,
               output_buffer<int32_t>& out_sub_for_ms,
               output_buffer<int32_t>& out_sub_for_sel) {
  // Input from lap:
  //   [lap_left(row r) | lap_right(row r) | lap_down(row r) | lap_mid(row r)]
  //
  // Previous package lap_mid is available through the one-row input margin and
  // becomes the current row's lap_up. Current lap_mid/lap_down are provided by
  // this firing's lap package.
  //
  // Current lap_left/lap_right are used directly, so horizontal sub values are
  // aligned with the current row rather than delayed by one firing.

  // The internal lap->sub margin is empty on the first firing.
  static bool have_prev_lap_mid = false;
  const bool use_prev_lap_mid = have_prev_lap_mid;

  int32_t* __restrict lap_base = in_lap_package.data();
  int32_t* __restrict ms_base  = out_sub_for_ms.data();
  int32_t* __restrict sel_base = out_sub_for_sel.data();

  v8int32* __restrict lap_left  = (v8int32*)(lap_base + (0 * COL));
  v8int32* __restrict lap_right = (v8int32*)(lap_base + (1 * COL));
  v8int32* __restrict lap_down  = (v8int32*)(lap_base + (2 * COL));
  v8int32* __restrict lap_mid   = (v8int32*)(lap_base + (3 * COL));

  v8int32* __restrict ms_left   = (v8int32*)(ms_base + (0 * COL));
  v8int32* __restrict ms_right  = (v8int32*)(ms_base + (1 * COL));
  v8int32* __restrict ms_up     = (v8int32*)(ms_base + (2 * COL));
  v8int32* __restrict ms_down   = (v8int32*)(ms_base + (3 * COL));

  v8int32* __restrict sel_left  = (v8int32*)(sel_base + (0 * COL));
  v8int32* __restrict sel_right = (v8int32*)(sel_base + (1 * COL));
  v8int32* __restrict sel_up    = (v8int32*)(sel_base + (2 * COL));
  v8int32* __restrict sel_down  = (v8int32*)(sel_base + (3 * COL));

  if (use_prev_lap_mid) {
    v8int32* __restrict prev_lap_mid = (v8int32*)(lap_base - COL);

    for (unsigned i = 0; i < COL / 8; ++i)
      chess_prepare_for_pipelining
      chess_loop_range(1, ) {
        v8int32 up_lap   = prev_lap_mid[i];
        v8int32 mid_lap  = lap_mid[i];
        v8int32 down_lap = lap_down[i];

        v8int32 left_v  = sub_vec(mid_lap,      lap_left[i]);
        v8int32 right_v = sub_vec(lap_right[i], mid_lap);
        v8int32 up_v    = sub_vec(mid_lap,      up_lap);
        v8int32 down_v  = sub_vec(down_lap,     mid_lap);

        ms_left[i]   = left_v;
        ms_right[i]  = right_v;
        ms_up[i]     = up_v;
        ms_down[i]   = down_v;

        sel_left[i]  = left_v;
        sel_right[i] = right_v;
        sel_up[i]    = up_v;
        sel_down[i]  = down_v;
      }
  } else {
    for (unsigned i = 0; i < COL / 8; ++i)
      chess_prepare_for_pipelining
      chess_loop_range(1, ) {
        v8int32 mid_lap  = lap_mid[i];
        v8int32 down_lap = lap_down[i];

        v8int32 left_v  = sub_vec(mid_lap,      lap_left[i]);
        v8int32 right_v = sub_vec(lap_right[i], mid_lap);
        v8int32 up_v    = sub_vec(mid_lap,      mid_lap);
        v8int32 down_v  = sub_vec(down_lap,     mid_lap);

        ms_left[i]   = left_v;
        ms_right[i]  = right_v;
        ms_up[i]     = up_v;
        ms_down[i]   = down_v;

        sel_left[i]  = left_v;
        sel_right[i] = right_v;
        sel_up[i]    = up_v;
        sel_down[i]  = down_v;
      }
  }

  have_prev_lap_mid = true;
}
