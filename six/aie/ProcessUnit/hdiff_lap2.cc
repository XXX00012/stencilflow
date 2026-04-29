#include <adf.h>
#include <aie_api/aie.hpp>
#include <cstdint>
#include "include.h"
#include "hdiff.h"

using namespace adf;

#define kernel_load 14

namespace {

inline void zero_forward(output_buffer<int32_t>& out_buf) {
  int32_t* out = out_buf.data();
  for (unsigned i = 0; i < 3 * COL; ++i) {
    out[i] = 0;
  }
}

} // namespace

void hdiff_lap2(
    lap2_input_buffer& in_window,
    output_buffer<int32_t>& out_package) {

  // 可见窗口定义：
  //   row_up   = 上上行（margin 第 1 行）
  //   row_mid  = 上一行（margin 第 2 行） <- 本拍实际输出对应这一行
  //   row_down = 当前正文行
  //
  // 输出顺序保持与原 lap2 完全一致：
  //   [ lap_left | lap_right | lap_mid ]

  // static unsigned warmup_counter = 0;
  // if (warmup_counter < 2) {
  //   zero_forward(out_package);
  //   ++warmup_counter;
  //   return;
  // }

  alignas(32) int32_t weights[8]      = {-4, -4, -4, -4, -4, -4, -4, -4};
  alignas(32) int32_t weights_rest[8] = {-1, -1, -1, -1, -1, -1, -1, -1};

  v8int32 coeffs      = *(v8int32*)weights;
  v8int32 coeffs_rest = *(v8int32*)weights_rest;

  int32_t* base_ptr      = in_window.data();
  int32_t* row_up_base   = base_ptr - (2 * COL);
  int32_t* row_mid_base  = base_ptr - (1 * COL);
  int32_t* row_down_base = base_ptr;

  int32_t* out_base = out_package.data();

  // 输出顺序不能改，sub 直接依赖这个顺序
  v8int32* __restrict out_left  = (v8int32*)(out_base + (0 * COL));
  v8int32* __restrict out_right = (v8int32*)(out_base + (1 * COL));
  v8int32* __restrict out_mid   = (v8int32*)(out_base + (2 * COL));

  v8int32* __restrict up_ptr   = (v8int32*)row_up_base;
  v8int32* __restrict mid_ptr  = (v8int32*)row_mid_base;
  v8int32* __restrict down_ptr = (v8int32*)row_down_base;

  v16int32 up_buf   = null_v16int32();
  v16int32 mid_buf  = null_v16int32();
  v16int32 down_buf = null_v16int32();

  // 预取两个相邻 8-lane block，形成 16-lane 滑窗
  up_buf   = upd_w(up_buf,   0, *up_ptr++);
  up_buf   = upd_w(up_buf,   1, *up_ptr);

  mid_buf  = upd_w(mid_buf,  0, *mid_ptr++);
  mid_buf  = upd_w(mid_buf,  1, *mid_ptr);

  down_buf = upd_w(down_buf, 0, *down_ptr++);
  down_buf = upd_w(down_buf, 1, *down_ptr);

  for (unsigned i = 0; i < COL / 8; ++i)
    chess_prepare_for_pipelining
    chess_loop_range(1, ) {

      v8acc80 acc_mid   = null_v8acc80();
      v8acc80 acc_left  = null_v8acc80();
      v8acc80 acc_right = null_v8acc80();

      // ------------------------------------------
      // lap_mid(row_mid)
      // = 4*mid - up - left - right - down
      // offset:
      //   mid row: 1(L), 2(M), 3(R)
      //   up/down: 2(M)
      // ------------------------------------------
      acc_mid = lmul8(up_buf,   2, 0x76543210, coeffs_rest, 0, 0x00000000);
      acc_mid = lmac8(acc_mid,  mid_buf, 1, 0x76543210, coeffs_rest, 0, 0x00000000);
      acc_mid = lmsc8(acc_mid,  mid_buf, 2, 0x76543210, coeffs,      0, 0x00000000);
      acc_mid = lmac8(acc_mid,  mid_buf, 3, 0x76543210, coeffs_rest, 0, 0x00000000);
      acc_mid = lmac8(acc_mid, down_buf, 2, 0x76543210, coeffs_rest, 0, 0x00000000);

      // ------------------------------------------
      // lap_left(row_mid)
      // = 4*mid_L - up_L - mid_LL - mid_M - down_L
      // offset:
      //   up/down: 1(L)
      //   mid: 0(LL), 1(L), 2(M)
      // ------------------------------------------
      acc_left = lmul8(up_buf,   1, 0x76543210, coeffs_rest, 0, 0x00000000);
      acc_left = lmac8(acc_left, mid_buf, 0, 0x76543210, coeffs_rest, 0, 0x00000000);
      acc_left = lmsc8(acc_left, mid_buf, 1, 0x76543210, coeffs,      0, 0x00000000);
      acc_left = lmac8(acc_left, mid_buf, 2, 0x76543210, coeffs_rest, 0, 0x00000000);
      acc_left = lmac8(acc_left, down_buf,1, 0x76543210, coeffs_rest, 0, 0x00000000);

      // ------------------------------------------
      // lap_right(row_mid)
      // = 4*mid_R - up_R - mid_M - mid_RR - down_R
      // offset:
      //   up/down: 3(R)
      //   mid: 2(M), 3(R), 4(RR)
      // ------------------------------------------
      acc_right = lmul8(up_buf,   3, 0x76543210, coeffs_rest, 0, 0x00000000);
      acc_right = lmac8(acc_right,mid_buf, 2, 0x76543210, coeffs_rest, 0, 0x00000000);
      acc_right = lmsc8(acc_right,mid_buf, 3, 0x76543210, coeffs,      0, 0x00000000);
      acc_right = lmac8(acc_right,mid_buf, 4, 0x76543210, coeffs_rest, 0, 0x00000000);
      acc_right = lmac8(acc_right,down_buf,3, 0x76543210, coeffs_rest, 0, 0x00000000);

      out_left[i]  = srs(acc_left, 0);
      out_right[i] = srs(acc_right, 0);
      out_mid[i]   = srs(acc_mid, 0);

      // 更新三行滑窗
      up_ptr = ((v8int32*)row_up_base) + i + 1;
      up_buf = upd_w(up_buf, 0, *up_ptr++);
      up_buf = upd_w(up_buf, 1, *up_ptr);

      mid_ptr = ((v8int32*)row_mid_base) + i + 1;
      mid_buf = upd_w(mid_buf, 0, *mid_ptr++);
      mid_buf = upd_w(mid_buf, 1, *mid_ptr);

      down_ptr = ((v8int32*)row_down_base) + i + 1;
      down_buf = upd_w(down_buf, 0, *down_ptr++);
      down_buf = upd_w(down_buf, 1, *down_ptr);
    }
}