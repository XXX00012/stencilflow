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

void hdiff_sub(
    sub_input_buffer& in_lap_package,
    output_buffer<int32_t>& out_package) {

  // --------------------------------------------------
  // lap2 输出布局（你现在已经改成）:
  //   [ new_left | new_right | new_mid ]
  //
  // sub 输入看到的逻辑布局:
  //   [ old_mid (margin) | new_left | new_right | new_mid ]
  //
  // 输出布局:
  //   [ sub_down_prev | sub_left_cur | sub_right_cur | sub_up_cur ]
  //
  // 公式:
  //   down_prev = new_mid   - old_mid
  //   left_cur  = new_left  - new_mid
  //   right_cur = new_right - new_mid
  //   up_cur    = old_mid   - new_mid
  // --------------------------------------------------

  int32_t* cur_base = in_lap_package.data();
  int32_t* out_base = out_package.data();

  // 注意：
  // data() 指向当前 block 的正文起点；margin 在它前面
  v8int32* old_mid   = (v8int32*)(cur_base + 0 * COL);
  v8int32* new_left  = (v8int32*)(cur_base + 1 * COL);
  v8int32* new_right = (v8int32*)(cur_base + 2 * COL);
  v8int32* new_mid   = (v8int32*)(cur_base + 3 * COL);

  v8int32* out_down  = (v8int32*)(out_base + (0 * COL));
  v8int32* out_left  = (v8int32*)(out_base + (1 * COL));
  v8int32* out_right = (v8int32*)(out_base + (2 * COL));
  v8int32* out_up    = (v8int32*)(out_base + (3 * COL));

  for (unsigned i = 0; i < COL / 8; ++i)
    chess_prepare_for_pipelining
    chess_loop_range(1, ) {

      v8int32 lap_old_mid = old_mid[i];
      v8int32 lap_left    = new_left[i];
      v8int32 lap_right   = new_right[i];
      v8int32 lap_mid     = new_mid[i];

      // 上一拍的下差值: S_{t-1}^{down} = L_t_mid - R_{t-1}
      out_down[i] = sub_vec(lap_mid, lap_old_mid);

      // 本拍左差值: S_t^{left} = L_t_left - L_t_mid
      out_left[i] = sub_vec(lap_left, lap_mid);

      // 本拍右差值: S_t^{right} = L_t_right - L_t_mid
      out_right[i] = sub_vec(lap_right, lap_mid);

      // 本拍上差值: S_t^{up} = R_{t-1} - L_t_mid
      out_up[i] = sub_vec(lap_old_mid, lap_mid);
    }
}