#include <adf.h>
#include <aie_api/aie.hpp>
#include <cstdint>
#include "include.h"
#include "hdiff.h"

using namespace adf;

#define kernel_load 14

namespace {

constexpr unsigned kMaskWordsPerRow = COL / 8;

inline unsigned int gt_zero_mask(v8int32 v) {
  return gt16(concat(v, undef_v8int32()), 0,
              0x76543210, 0xFEDCBA98,
              null_v16int32(), 0,
              0x76543210, 0xFEDCBA98);
}

inline unsigned int lt_zero_mask(v8int32 v) {
  return gt16(null_v16int32(), 0,
              0x76543210, 0xFEDCBA98,
              concat(v, undef_v8int32()), 0,
              0x76543210, 0xFEDCBA98);
}

inline unsigned int gt_diff_mask(v16int32 pos_buf, int pos_offset,
                                 v16int32 neg_buf, int neg_offset) {
  return gt16(pos_buf, pos_offset,
              0x76543210, 0xFEDCBA98,
              neg_buf, neg_offset,
              0x76543210, 0xFEDCBA98);
}

inline unsigned int same_sign_diff_mask(v8int32 factor,
                                        v16int32 pos_buf, int pos_offset,
                                        v16int32 neg_buf, int neg_offset) {
  unsigned int factor_pos = gt_zero_mask(factor);
  unsigned int factor_neg = lt_zero_mask(factor);
  unsigned int diff_pos = gt_diff_mask(pos_buf, pos_offset, neg_buf, neg_offset);
  unsigned int diff_neg = gt_diff_mask(neg_buf, neg_offset, pos_buf, pos_offset);
  return (factor_pos & diff_pos) | (factor_neg & diff_neg);
}

} // namespace

void hdiff_mscom(ms_complete_input_buffer& in_window,
                 input_buffer<int32_t>& in_sub_package,
                 output_buffer<int32_t>& out_mask_package) {
  // Fused ms+com.
  //
  // It keeps six_complete's external raw-psi input for ms:
  //   in_window -> psi rows used by the product stage
  //   in_sub_package -> [sub_left | sub_right | sub_up | sub_down]
  //
  // Instead of materializing the four product rows, it immediately emits the
  // compact masks consumed by selupdate:
  //   [mask_left | mask_right | mask_up | mask_down]

  int32_t* __restrict base_ptr      = in_window.data();
  int32_t* __restrict row_up_base   = base_ptr - (3 * COL);
  int32_t* __restrict row_mid_base  = base_ptr - (2 * COL);
  int32_t* __restrict row_down_base = base_ptr - (1 * COL);

  int32_t* __restrict sub_base  = in_sub_package.data();
  int32_t* __restrict mask_base = out_mask_package.data();

  v8int32* __restrict sub_left  = (v8int32*)(sub_base + (0 * COL));
  v8int32* __restrict sub_right = (v8int32*)(sub_base + (1 * COL));
  v8int32* __restrict sub_up    = (v8int32*)(sub_base + (2 * COL));
  v8int32* __restrict sub_down  = (v8int32*)(sub_base + (3 * COL));

  int32_t* __restrict mask_left  = mask_base + (0 * kMaskWordsPerRow);
  int32_t* __restrict mask_right = mask_base + (1 * kMaskWordsPerRow);
  int32_t* __restrict mask_up    = mask_base + (2 * kMaskWordsPerRow);
  int32_t* __restrict mask_down  = mask_base + (3 * kMaskWordsPerRow);

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
      mask_left[i] =
          (int32_t)same_sign_diff_mask(sub_left[i],
                                       mid_buf, 2,
                                       mid_buf, 1);
      mask_right[i] =
          (int32_t)same_sign_diff_mask(sub_right[i],
                                       mid_buf, 3,
                                       mid_buf, 2);
      mask_up[i] =
          (int32_t)same_sign_diff_mask(sub_up[i],
                                       mid_buf, 2,
                                       up_buf, 2);
      mask_down[i] =
          (int32_t)same_sign_diff_mask(sub_down[i],
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
