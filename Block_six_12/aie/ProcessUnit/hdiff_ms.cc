#include <adf.h>
#include <aie_api/aie.hpp>
#include <cstdint>
#include "include.h"
#include "hdiff.h"

using namespace adf;

#define kernel_load 14

inline void zero_forward(output_buffer<int32_t>& out_buf) {
  int32_t* out = out_buf.data();
  for (unsigned i = 0; i < 3 * COL; ++i) {
    out[i] = 0;
  }
} // ms 现在一次输出三行，前两拍预热

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

void hdiff_ms(
    ms_input_buffer& in_window,
    input_buffer<int32_t>& in_sub_package,
    output_buffer<int32_t>& out_package) {

  // --------------------------------------------------
  // in_window:
  //   margin 行 + 当前行
  //   prev_row_base = 上一行原始 phi
  //   curr_row_base = 当前行原始 phi
  //
  // in_sub_package 布局（与 sub 对齐）:
  //   [ sub_down_prev | sub_left_cur | sub_right_cur | sub_up_cur ]
  //
  // out_package 新布局（3 行）:
  //   [ ms_vert_shared | ms_left_cur | ms_right_cur ]
  //
  // 其中:
  //   ms_vert_shared = ms_down_prev = ms_up_cur
  //                  = sub_down_prev * (phi_t - phi_{t-1})
  //                  = sub_up_cur    * (phi_{t-1} - phi_t)
  //
  //   ms_left_cur  = sub_left_cur  * (phi_tL - phi_t)
  //   ms_right_cur = sub_right_cur * (phi_tR - phi_t)
  //
  // 预热:
  //   只有输入为 2/3 行数据时才开始工作，因此前两拍输出全 0。
  // --------------------------------------------------

  // static unsigned warmup_counter = 0;

  // if (warmup_counter < 2) {
  //   zero_forward(out_package);
  //   ++warmup_counter;
  //   return;
  // }

  int32_t* base_ptr      = in_window.data();
  int32_t* prev_row_base = base_ptr + (0 * COL);
  int32_t* curr_row_base = base_ptr + (1 * COL);

  int32_t* sub_base = in_sub_package.data();
  int32_t* out_base = out_package.data();

  v8int32* in_sub_down  = (v8int32*)(sub_base + (0 * COL));
  v8int32* in_sub_left  = (v8int32*)(sub_base + (1 * COL));
  v8int32* in_sub_right = (v8int32*)(sub_base + (2 * COL));
  // sub_up 逻辑上等价，但这里不再单独输出
  // v8int32* in_sub_up    = (v8int32*)(sub_base + (3 * COL));

  v8int32* out_vert  = (v8int32*)(out_base + (0 * COL));
  v8int32* out_left  = (v8int32*)(out_base + (1 * COL));
  v8int32* out_right = (v8int32*)(out_base + (2 * COL));

  v8int32* __restrict prev_ptr = (v8int32*)prev_row_base;
  v8int32* __restrict curr_ptr = (v8int32*)curr_row_base;

  v16int32 prev_buf = null_v16int32();
  v16int32 curr_buf = null_v16int32();

  prev_buf = upd_w(prev_buf, 0, *prev_ptr++);
  prev_buf = upd_w(prev_buf, 1, *prev_ptr);

  curr_buf = upd_w(curr_buf, 0, *curr_ptr++);
  curr_buf = upd_w(curr_buf, 1, *curr_ptr);

  for (unsigned i = 0; i < COL / 8; ++i)
    chess_prepare_for_pipelining
    chess_loop_range(1, ) {

      v8int32 sub_down  = in_sub_down[i];
      v8int32 sub_left  = in_sub_left[i];
      v8int32 sub_right = in_sub_right[i];

      // 共享的 vertical 乘积:
      // 既是上一循环 down，也是本次循环 up
      out_vert[i] =
          mul_sub_vec(sub_down,
                      curr_buf, 2,
                      prev_buf, 2);

      // 本次循环 left:
      // sub_left_cur * (phi_tL - phi_t)
      out_left[i] =
          mul_sub_vec(sub_left,
                      curr_buf, 1,
                      curr_buf, 2);

      // 本次循环 right:
      // sub_right_cur * (phi_tR - phi_t)
      out_right[i] =
          mul_sub_vec(sub_right,
                      curr_buf, 3,
                      curr_buf, 2);

      prev_ptr = ((v8int32*)prev_row_base) + i + 1;
      prev_buf = upd_w(prev_buf, 0, *prev_ptr++);
      prev_buf = upd_w(prev_buf, 1, *prev_ptr);

      curr_ptr = ((v8int32*)curr_row_base) + i + 1;
      curr_buf = upd_w(curr_buf, 0, *curr_ptr++);
      curr_buf = upd_w(curr_buf, 1, *curr_ptr);
    }
}