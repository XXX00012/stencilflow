#include <adf.h>
#include <aie_api/aie.hpp>
#include <cstdint>
#include "include.h"
#include "hdiff.h"

using namespace adf;

#define kernel_load 14

namespace {

constexpr int32_t kC = -7;

inline void zero_forward(output_buffer<int32_t>& out_buf) {
  int32_t* out = out_buf.data();
  for (unsigned i = 0; i < COL; ++i) {
    out[i] = 0;
  }
}

} // namespace

void hdiff_update(
    update_input_buffer& in_comsel_package,
    input_buffer<int32_t>& in_psi_row,
    output_buffer<int32_t>& out_row) {

  // --------------------------------------------------
  // 当前实现按你最新确认的口径：
  //
  // comsel 正文每拍输出 4 行：
  //   [ cur_down | cur_left | cur_right | cur_up ]
  //
  // update 输入带 margin<3*COL>，因此当前拍可见 7 行：
  //   row0 = prev_left
  //   row1 = prev_right
  //   row2 = prev_up
  //   row3 = cur_down
  //   row4 = cur_left
  //   row5 = cur_right
  //   row6 = cur_up
  //
  // 本拍只用前 4 行：
  //   [ prev_left | prev_right | prev_up | cur_down ]
  //
  // 也就是：
  //   上一循环的 left/right/up
  //   + 当前循环补来的 down
  //
  // in_psi_row:
  //   按你当前口径，直接作为本拍要更新的那一行原始数据
  //
  // 符号口径沿用旧版 update 的 4 项符号关系：
  //   out = psi
  //       - C * down
  //       + C * up
  //       - C * left
  //       - C * right
  //
  // 其中 C = -7
  //
  // 第一拍没有有效 margin，直接输出 0
  // --------------------------------------------------

  // static bool warmup_done = false;

  // if (!warmup_done) {
  //   zero_forward(out_row);
  //   warmup_done = true;
  //   return;
  // }

  alignas(32) int32_t ones_arr[8] = {1, 1, 1, 1, 1, 1, 1, 1};
  alignas(32) int32_t c_arr[8]    = {kC, kC, kC, kC, kC, kC, kC, kC};

  v8int32 ones_vec = *(v8int32*)ones_arr;
  v8int32 c_vec    = *(v8int32*)c_arr;

  int32_t* comsel_base = in_comsel_package.data();
  int32_t* psi_base    = in_psi_row.data();
  int32_t* out_base    = out_row.data();

  // 可见 7 行窗口
  v8int32* __restrict prev_left  = (v8int32*)(comsel_base + (0 * COL));
  v8int32* __restrict prev_right = (v8int32*)(comsel_base + (1 * COL));
  v8int32* __restrict prev_up    = (v8int32*)(comsel_base + (2 * COL));
  v8int32* __restrict cur_down   = (v8int32*)(comsel_base + (3 * COL));

  v8int32* __restrict psi_cur = (v8int32*)psi_base; 
  v8int32* __restrict out     = (v8int32*)out_base;

  for (unsigned i = 0; i < COL / 8; ++i)
    chess_prepare_for_pipelining
    chess_loop_range(1, ) {

      v8int32 psi_v   = psi_cur[i];
      v8int32 left_v  = prev_left[i];
      v8int32 right_v = prev_right[i];
      v8int32 up_v    = prev_up[i];
      v8int32 down_v  = cur_down[i];

      v8acc80 acc = null_v8acc80();

      // out = psi
      acc = lmul8(concat(psi_v, undef_v8int32()), 0, 0x76543210,
                  ones_vec, 0, 0x00000000);

      // out -= C * down
      acc = lmsc8(acc,
                  concat(down_v, undef_v8int32()), 0, 0x76543210,
                  c_vec, 0, 0x00000000);

      // out += C * up
      acc = lmac8(acc,
                  concat(up_v, undef_v8int32()), 0, 0x76543210,
                  c_vec, 0, 0x00000000);

      // out -= C * left
      acc = lmsc8(acc,
                  concat(left_v, undef_v8int32()), 0, 0x76543210,
                  c_vec, 0, 0x00000000);

      // out -= C * right
      acc = lmsc8(acc,
                  concat(right_v, undef_v8int32()), 0, 0x76543210,
                  c_vec, 0, 0x00000000);

      out[i] = srs(acc, 0);
    }
}