#include <adf.h>
#include <aie_api/aie.hpp>
#include <cstdint>
#include "include.h"
#include "hdiff.h"

using namespace adf;

#define kernel_load 14

namespace {

inline v8int32 lap_center(v16int32 up_buf,
                          v16int32 mid_buf,
                          v16int32 down_buf,
                          v8int32 coeffs,
                          v8int32 coeffs_rest) {
  v8acc80 acc = null_v8acc80();
  acc = lmul8(up_buf,    2, 0x76543210, coeffs_rest, 0, 0x00000000);
  acc = lmac8(acc, mid_buf, 1, 0x76543210, coeffs_rest, 0, 0x00000000);
  acc = lmsc8(acc, mid_buf, 2, 0x76543210, coeffs,      0, 0x00000000);
  acc = lmac8(acc, mid_buf, 3, 0x76543210, coeffs_rest, 0, 0x00000000);
  acc = lmac8(acc, down_buf,2, 0x76543210, coeffs_rest, 0, 0x00000000);
  return srs(acc, 0);
}

inline v8int32 lap_left(v16int32 up_buf,
                        v16int32 mid_buf,
                        v16int32 down_buf,
                        v8int32 coeffs,
                        v8int32 coeffs_rest) {
  v8acc80 acc = null_v8acc80();
  acc = lmul8(up_buf,    1, 0x76543210, coeffs_rest, 0, 0x00000000);
  acc = lmac8(acc, mid_buf, 0, 0x76543210, coeffs_rest, 0, 0x00000000);
  acc = lmsc8(acc, mid_buf, 1, 0x76543210, coeffs,      0, 0x00000000);
  acc = lmac8(acc, mid_buf, 2, 0x76543210, coeffs_rest, 0, 0x00000000);
  acc = lmac8(acc, down_buf,1, 0x76543210, coeffs_rest, 0, 0x00000000);
  return srs(acc, 0);
}

inline v8int32 lap_right(v16int32 up_buf,
                         v16int32 mid_buf,
                         v16int32 down_buf,
                         v8int32 coeffs,
                         v8int32 coeffs_rest) {
  v8acc80 acc = null_v8acc80();
  acc = lmul8(up_buf,    3, 0x76543210, coeffs_rest, 0, 0x00000000);
  acc = lmac8(acc, mid_buf, 2, 0x76543210, coeffs_rest, 0, 0x00000000);
  acc = lmsc8(acc, mid_buf, 3, 0x76543210, coeffs,      0, 0x00000000);
  acc = lmac8(acc, mid_buf, 4, 0x76543210, coeffs_rest, 0, 0x00000000);
  acc = lmac8(acc, down_buf,3, 0x76543210, coeffs_rest, 0, 0x00000000);
  return srs(acc, 0);
}

} // namespace

void hdiff_lap(lap_complete_input_buffer& in_window,
               output_buffer<int32_t>& lap_to_x,
               output_buffer<int32_t>& lap_to_y) {
  alignas(32) int32_t weights[8]      = {-4, -4, -4, -4, -4, -4, -4, -4};
  alignas(32) int32_t weights_rest[8] = {-1, -1, -1, -1, -1, -1, -1, -1};

  v8int32 coeffs      = *(v8int32*)weights;
  v8int32 coeffs_rest = *(v8int32*)weights_rest;

  int32_t* __restrict base_ptr  = in_window.data();
  int32_t* __restrict row1_base = base_ptr - (3 * COL);
  int32_t* __restrict row2_base = base_ptr - (2 * COL);
  int32_t* __restrict row3_base = base_ptr - (1 * COL);
  int32_t* __restrict row4_base = base_ptr;

  int32_t* __restrict out_x_base = lap_to_x.data();
  int32_t* __restrict out_y_base = lap_to_y.data();

  v8int32* __restrict x_lap_left  = (v8int32*)(out_x_base + (0 * COL));
  v8int32* __restrict x_lap_right = (v8int32*)(out_x_base + (1 * COL));
  v8int32* __restrict x_lap_down  = (v8int32*)(out_x_base + (2 * COL));
  v8int32* __restrict x_lap_mid   = (v8int32*)(out_x_base + (3 * COL));

  v8int32* __restrict y_lap_left  = (v8int32*)(out_y_base + (0 * COL));
  v8int32* __restrict y_lap_right = (v8int32*)(out_y_base + (1 * COL));
  v8int32* __restrict y_lap_down  = (v8int32*)(out_y_base + (2 * COL));
  v8int32* __restrict y_lap_mid   = (v8int32*)(out_y_base + (3 * COL));

  v16int32 row1_buf = null_v16int32();
  v16int32 row2_buf = null_v16int32();
  v16int32 row3_buf = null_v16int32();
  v16int32 row4_buf = null_v16int32();

  v8int32* __restrict row1_ptr = (v8int32*)row1_base;
  v8int32* __restrict row2_ptr = (v8int32*)row2_base;
  v8int32* __restrict row3_ptr = (v8int32*)row3_base;
  v8int32* __restrict row4_ptr = (v8int32*)row4_base;

  row1_buf = upd_w(row1_buf, 0, *row1_ptr++);
  row1_buf = upd_w(row1_buf, 1, *row1_ptr);
  row2_buf = upd_w(row2_buf, 0, *row2_ptr++);
  row2_buf = upd_w(row2_buf, 1, *row2_ptr);
  row3_buf = upd_w(row3_buf, 0, *row3_ptr++);
  row3_buf = upd_w(row3_buf, 1, *row3_ptr);
  row4_buf = upd_w(row4_buf, 0, *row4_ptr++);
  row4_buf = upd_w(row4_buf, 1, *row4_ptr);

  for (unsigned i = 0; i < COL / 8; ++i)
    chess_prepare_for_pipelining
    chess_loop_range(1, ) {
      v8int32 lap_l = lap_left(row1_buf, row2_buf, row3_buf, coeffs, coeffs_rest);
      v8int32 lap_r = lap_right(row1_buf, row2_buf, row3_buf, coeffs, coeffs_rest);
      v8int32 lap_d = lap_center(row2_buf, row3_buf, row4_buf, coeffs, coeffs_rest);
      v8int32 lap_m = lap_center(row1_buf, row2_buf, row3_buf, coeffs, coeffs_rest);

      x_lap_left[i]  = lap_l;
      x_lap_right[i] = lap_r;
      x_lap_down[i]  = lap_d;
      x_lap_mid[i]   = lap_m;

      y_lap_left[i]  = lap_l;
      y_lap_right[i] = lap_r;
      y_lap_down[i]  = lap_d;
      y_lap_mid[i]   = lap_m;

      row1_ptr = ((v8int32*)row1_base) + i + 1;
      row2_ptr = ((v8int32*)row2_base) + i + 1;
      row3_ptr = ((v8int32*)row3_base) + i + 1;
      row4_ptr = ((v8int32*)row4_base) + i + 1;

      row1_buf = upd_w(row1_buf, 0, *row1_ptr++);
      row1_buf = upd_w(row1_buf, 1, *row1_ptr);
      row2_buf = upd_w(row2_buf, 0, *row2_ptr++);
      row2_buf = upd_w(row2_buf, 1, *row2_ptr);
      row3_buf = upd_w(row3_buf, 0, *row3_ptr++);
      row3_buf = upd_w(row3_buf, 1, *row3_ptr);
      row4_buf = upd_w(row4_buf, 0, *row4_ptr++);
      row4_buf = upd_w(row4_buf, 1, *row4_ptr);
    }
}
