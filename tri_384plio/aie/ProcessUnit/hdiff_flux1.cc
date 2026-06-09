#include <adf.h>
#include <aie_api/aie.hpp>
#include <cstdint>
#include "include.h"
#include "hdiff.h"

using namespace adf;

#define kernel_load 14

namespace {

inline v8int32 load_row_chunk(int32_t* row_base, unsigned chunk) {
  return ((v8int32*)row_base)[chunk];
}

inline unsigned int make_take_mask(v8int32 product) {
  return gt16(concat(product, undef_v8int32()), 0,
              0x76543210, 0xFEDCBA98,
              null_v16int32(), 0,
              0x76543210, 0xFEDCBA98);
}

} // namespace

void hdiff_flux1(flux1_input_buffer& in_window,
                 input_buffer<int32_t>& sub_pack,
                 output_buffer<int32_t>& mask_pack) {
  int32_t* __restrict base_ptr = in_window.data();

  // data() points at the margin/history region followed by the current row.
  int32_t* row1_base = base_ptr + (0 * COL);
  int32_t* row2_base = base_ptr + (1 * COL);
  int32_t* row3_base = base_ptr + (2 * COL);

  int32_t* __restrict sub_base = sub_pack.data();
  int32_t* __restrict sub_left_base  = sub_base + (0 * COL);
  int32_t* __restrict sub_right_base = sub_base + (1 * COL);
  int32_t* __restrict sub_up_base    = sub_base + (2 * COL);
  int32_t* __restrict sub_down_base  = sub_base + (3 * COL);

  int32_t* __restrict mask_base = mask_pack.data();
  int32_t* __restrict mask_left  = mask_base + (0 * hdiff_cfg::kFluxMaskWordsPerRow);
  int32_t* __restrict mask_right = mask_base + (1 * hdiff_cfg::kFluxMaskWordsPerRow);
  int32_t* __restrict mask_up    = mask_base + (2 * hdiff_cfg::kFluxMaskWordsPerRow);
  int32_t* __restrict mask_down  = mask_base + (3 * hdiff_cfg::kFluxMaskWordsPerRow);

  v16int32 data_buf1 = null_v16int32();
  v16int32 data_buf2 = null_v16int32();

  v8acc80 acc_0 = null_v8acc80();
  v8acc80 acc_1 = null_v8acc80();

  data_buf1 = upd_w(data_buf1, 0, load_row_chunk(row1_base, 0));
  data_buf1 = upd_w(data_buf1, 1, load_row_chunk(row1_base, 1));

  data_buf2 = upd_w(data_buf2, 0, load_row_chunk(row2_base, 0));
  data_buf2 = upd_w(data_buf2, 1, load_row_chunk(row2_base, 1));

  for (unsigned i = 0; i < COL / 8; i++)
    chess_prepare_for_pipelining
    chess_loop_range(1, ) {
      v8int32 flux_sub;
      v8int32 product;

      flux_sub = ((v8int32*)sub_left_base)[i];

      acc_1 = lmul8(data_buf2, 2, 0x76543210, flux_sub, 0, 0x76543210);
      acc_1 = lmsc8(acc_1, data_buf2, 1, 0x76543210, flux_sub, 0, 0x76543210);
      product = srs(acc_1, 0);

      mask_left[i] = (int32_t)make_take_mask(product);

      flux_sub = ((v8int32*)sub_right_base)[i];

      acc_0 = lmul8(data_buf2, 3, 0x76543210, flux_sub, 0, 0x76543210);
      acc_0 = lmsc8(acc_0, data_buf2, 2, 0x76543210, flux_sub, 0, 0x76543210);
      product = srs(acc_0, 0);

      mask_right[i] = (int32_t)make_take_mask(product);

      flux_sub = ((v8int32*)sub_up_base)[i];

      acc_1 = lmul8(data_buf2, 2, 0x76543210, flux_sub, 0, 0x76543210);
      acc_1 = lmsc8(acc_1, data_buf1, 2, 0x76543210, flux_sub, 0, 0x76543210);
      product = srs(acc_1, 0);

      mask_up[i] = (int32_t)make_take_mask(product);

      data_buf1 = upd_w(data_buf1, 0, load_row_chunk(row3_base, i));
      data_buf1 = upd_w(data_buf1, 1, load_row_chunk(row3_base, i + 1));

      flux_sub = ((v8int32*)sub_down_base)[i];

      acc_1 = lmul8(data_buf1, 2, 0x76543210, flux_sub, 0, 0x76543210);
      acc_1 = lmsc8(acc_1, data_buf2, 2, 0x76543210, flux_sub, 0, 0x76543210);
      product = srs(acc_1, 0);

      mask_down[i] = (int32_t)make_take_mask(product);

      data_buf1 = upd_w(data_buf1, 0, load_row_chunk(row1_base, i + 1));
      data_buf1 = upd_w(data_buf1, 1, load_row_chunk(row1_base, i + 2));

      data_buf2 = upd_w(data_buf2, 0, load_row_chunk(row2_base, i + 1));
      data_buf2 = upd_w(data_buf2, 1, load_row_chunk(row2_base, i + 2));
    }
}
