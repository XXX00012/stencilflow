#include <adf.h>
#include <aie_api/aie.hpp>
#include <cstdint>
#include "include.h"
#include "hdiff.h"

using namespace adf;

#define kernel_load 14

namespace {

constexpr unsigned kMaskWordsPerRow = COL / 8;

inline unsigned int make_take_zero_mask(v8int32 product) {
  return gt16(concat(product, undef_v8int32()), 0,
              0x76543210, 0xFEDCBA98,
              null_v16int32(), 0,
              0x76543210, 0xFEDCBA98);
}

} // namespace

void hdiff_com(
    input_buffer<int32_t>& in_ms_package,
    output_buffer<int32_t>& out_mask_package) {

  // 说明：
  // 这里刻意保持“当前 hdiff_comsel.cc 的实际读取顺序”，
  // 不按注释里声称的 [vert | left | right] 去改，
  // 以避免在拆核时顺手改掉你现有数值逻辑。
  // 也就是说，这里仍保持：
  //   row0 -> ms_left  （对应当前代码实际读法）
  //   row1 -> ms_right （对应当前代码实际读法）
  //   row2 -> ms_vert  （对应当前代码实际读法）

  int32_t* ms_base   = in_ms_package.data();
  int32_t* mask_base = out_mask_package.data();

  v8int32* __restrict in_ms_vert  = (v8int32*)(ms_base + (0 * COL));
  v8int32* __restrict in_ms_left  = (v8int32*)(ms_base + (1 * COL));
  v8int32* __restrict in_ms_right = (v8int32*)(ms_base + (2 * COL));


  int32_t* __restrict out_mask_vert  = mask_base + (0 * kMaskWordsPerRow);
  int32_t* __restrict out_mask_left  = mask_base + (1 * kMaskWordsPerRow);
  int32_t* __restrict out_mask_right = mask_base + (2 * kMaskWordsPerRow);

  for (unsigned i = 0; i < COL / 8; ++i)
    chess_prepare_for_pipelining
    chess_loop_range(1, ) {

      v8int32 ms_vert  = in_ms_vert[i];
      v8int32 ms_left  = in_ms_left[i];
      v8int32 ms_right = in_ms_right[i];

      out_mask_vert[i]  = (int32_t)make_take_zero_mask(ms_vert);
      out_mask_left[i]  = (int32_t)make_take_zero_mask(ms_left);
      out_mask_right[i] = (int32_t)make_take_zero_mask(ms_right);
    }
}
