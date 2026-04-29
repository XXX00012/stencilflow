#include <adf.h>
#include <aie_api/aie.hpp>
#include <cstdint>
#include "include.h"
#include "hdiff.h"

using namespace adf;

#define kernel_load 14

namespace {

// 若乘积 > 0，则输出 0；否则输出对应 sub 差值
inline v8int32 select_nonpos(v8int32 product, v8int32 diff) {
  unsigned int take_zero_mask =
      gt16(concat(product, undef_v8int32()), 0, 0x76543210, 0xFEDCBA98,
           null_v16int32(), 0, 0x76543210, 0xFEDCBA98);

  v16int32 selected =
      select16(take_zero_mask,
               concat(diff, undef_v8int32()),  // product <= 0
               null_v16int32());               // product > 0

  return ext_w(selected, 0);
}

} // namespace

void hdiff_comsel(
    input_buffer<int32_t>& in_ms_package,
    input_buffer<int32_t>& in_sub_package,
    output_buffer<int32_t>& out_package) {

  // --------------------------------------------------
  // in_ms_package 新布局（必须与 ms 输出顺序严格对齐）:
  //   [ ms_vert_shared | ms_left_cur | ms_right_cur ]
  //
  // 其中:
  //   ms_vert_shared 对应共享 vertical 接口
  //   这一路既可看作上一循环的 Gr+1/2，
  //   也可看作本循环的 Gr-1/2
  //
  // in_sub_package 布局（保持 sub 原顺序）:
  //   [ sub_down_prev | sub_left_cur | sub_right_cur | sub_up_cur ]
  //
  // 这里约定：
  //   共享接口统一使用 sub_down_prev 这一路来输出，
  //   不再单独输出 sub_up_cur，对齐 ms 的三行口径。
  //
  // out_package 新布局（与 ms 保持同顺序）:
  //   [ fg_vert_shared | fg_left_cur | fg_right_cur ]
  //
  // 公式:
  //   fg_vert_shared = (ms_vert_shared  <= 0) ? sub_down_prev : 0
  //   fg_left_cur    = (ms_left_cur     <= 0) ? sub_left_cur  : 0
  //   fg_right_cur   = (ms_right_cur    <= 0) ? sub_right_cur : 0
  // --------------------------------------------------

  int32_t* ms_base  = in_ms_package.data();
  int32_t* sub_base = in_sub_package.data();
  int32_t* out_base = out_package.data();

  // ms: 3 行，顺序必须与 hdiff_ms 完全一致

  v8int32* __restrict in_ms_left  = (v8int32*)(ms_base + (0 * COL));
  v8int32* __restrict in_ms_right = (v8int32*)(ms_base + (1 * COL));
  v8int32* __restrict in_ms_vert  = (v8int32*)(ms_base + (2 * COL));

  // sub: 仍然保持 4 行
  v8int32* __restrict in_sub_down  = (v8int32*)(sub_base + (0 * COL));
  v8int32* __restrict in_sub_left  = (v8int32*)(sub_base + (1 * COL));
  v8int32* __restrict in_sub_right = (v8int32*)(sub_base + (2 * COL));
  v8int32* __restrict in_sub_up    = (v8int32*)(sub_base + (3 * COL)); // 不再单独使用

  // out: 3 行，顺序也与 ms 一样
  v8int32* __restrict out_predown  = (v8int32*)(out_base + (0 * COL));
  v8int32* __restrict out_left  = (v8int32*)(out_base + (1 * COL));
  v8int32* __restrict out_right = (v8int32*)(out_base + (2 * COL));
  v8int32* __restrict out_curup = (v8int32*)(out_base + (3 * COL)); 

  for (unsigned i = 0; i < COL / 8; ++i)
    chess_prepare_for_pipelining
    chess_loop_range(1, ) {

      v8int32 ms_vert  = in_ms_vert[i];
      v8int32 ms_left  = in_ms_left[i];
      v8int32 ms_right = in_ms_right[i];

      v8int32 sub_down  = in_sub_down[i];
      v8int32 sub_left  = in_sub_left[i];
      v8int32 sub_right = in_sub_right[i];
      v8int32 sub_up    = in_sub_up[i];

      out_predown[i]  = select_nonpos(ms_vert,  sub_down);
      out_left[i]  = select_nonpos(ms_left,  sub_left);
      out_right[i] = select_nonpos(ms_right, sub_right);
      out_curup[i] = select_nonpos(ms_vert,  sub_up); 
    }
}