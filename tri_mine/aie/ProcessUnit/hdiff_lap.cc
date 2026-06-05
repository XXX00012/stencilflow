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

} // namespace

void hdiff_lap(lap_input_buffer& in_window,
               output_buffer<int32_t>& sub_pack_for_ms,
               output_buffer<int32_t>& sub_pack_for_sel) {
  alignas(32) int32_t weights[8]      = {-4, -4, -4, -4, -4, -4, -4, -4};
  alignas(32) int32_t weights_rest[8] = {-1, -1, -1, -1, -1, -1, -1, -1};

  v8int32 coeffs      = *(v8int32*)weights;
  v8int32 coeffs_rest = *(v8int32*)weights_rest;

  int32_t* __restrict ms_base  = sub_pack_for_ms.data();
  int32_t* __restrict sel_base = sub_pack_for_sel.data();

  int32_t* __restrict out_ms_flux1_base = ms_base + (0 * COL);
  int32_t* __restrict out_ms_flux2_base = ms_base + (1 * COL);
  int32_t* __restrict out_ms_flux3_base = ms_base + (2 * COL);
  int32_t* __restrict out_ms_flux4_base = ms_base + (3 * COL);

  int32_t* __restrict out_sel_flux1_base = sel_base + (0 * COL);
  int32_t* __restrict out_sel_flux2_base = sel_base + (1 * COL);
  int32_t* __restrict out_sel_flux3_base = sel_base + (2 * COL);
  int32_t* __restrict out_sel_flux4_base = sel_base + (3 * COL);

  int32_t* __restrict base_ptr = in_window.data();
  // data() points at the margin/history region followed by the current row.
  int32_t* row0_base = base_ptr + (0 * COL);
  int32_t* row1_base = base_ptr + (1 * COL);
  int32_t* row2_base = base_ptr + (2 * COL);
  int32_t* row3_base = base_ptr + (3 * COL);
  int32_t* row4_base = base_ptr + (4 * COL);

  v16int32 data_buf1 = null_v16int32();
  v16int32 data_buf2 = null_v16int32();

  v8acc80 acc_0 = null_v8acc80();
  v8acc80 acc_1 = null_v8acc80();

  v8int32 lap_ij = null_v8int32();
  v8int32 lap_0  = null_v8int32();

  data_buf1 = upd_w(data_buf1, 0, load_row_chunk(row3_base, 0));
  data_buf1 = upd_w(data_buf1, 1, load_row_chunk(row3_base, 1));
  data_buf2 = upd_w(data_buf2, 0, load_row_chunk(row1_base, 0));
  data_buf2 = upd_w(data_buf2, 1, load_row_chunk(row1_base, 1));

  for (unsigned i = 0; i < COL / 8; i++)
    chess_prepare_for_pipelining
    chess_loop_range(1, ) {
      v16int32 flux_sub;

      acc_0 = lmul8(data_buf2, 2, 0x76543210, coeffs_rest, 0, 0x00000000);
      acc_1 = lmul8(data_buf2, 1, 0x76543210, coeffs_rest, 0, 0x00000000);

      acc_0 = lmac8(acc_0, data_buf1, 2, 0x76543210, coeffs_rest, 0, 0x00000000);
      acc_1 = lmac8(acc_1, data_buf1, 1, 0x76543210, coeffs_rest, 0, 0x00000000);

      data_buf2 = upd_w(data_buf2, 0, load_row_chunk(row2_base, i));
      data_buf2 = upd_w(data_buf2, 1, load_row_chunk(row2_base, i + 1));

      acc_0 = lmac8(acc_0, data_buf2, 1, 0x76543210, coeffs_rest, 0, 0x00000000);
      acc_0 = lmsc8(acc_0, data_buf2, 2, 0x76543210, coeffs,      0, 0x00000000);
      acc_0 = lmac8(acc_0, data_buf2, 3, 0x76543210, coeffs_rest, 0, 0x00000000);

      lap_ij = srs(acc_0, 0);

      acc_1 = lmac8(acc_1, data_buf2, 0, 0x76543210, coeffs_rest, 0, 0x00000000);
      acc_1 = lmsc8(acc_1, data_buf2, 1, 0x76543210, coeffs,      0, 0x00000000);
      acc_1 = lmac8(acc_1, data_buf2, 2, 0x76543210, coeffs_rest, 0, 0x00000000);

      lap_0 = srs(acc_1, 0);

      flux_sub =
          sub16(concat(lap_ij, undef_v8int32()), 0, 0x76543210, 0xFEDCBA98,
                concat(lap_0,  undef_v8int32()), 0, 0x76543210, 0xFEDCBA98);
      v8int32 sub_v = ext_w(flux_sub, 0);
      ((v8int32*)out_ms_flux1_base)[i]  = sub_v;
      ((v8int32*)out_sel_flux1_base)[i] = sub_v;

      acc_0 = lmul8(data_buf1, 3, 0x76543210, coeffs_rest, 0, 0x00000000);
      acc_0 = lmsc8(acc_0, data_buf2, 3, 0x76543210, coeffs,      0, 0x00000000);

      data_buf1 = upd_w(data_buf1, 0, load_row_chunk(row1_base, i));
      data_buf1 = upd_w(data_buf1, 1, load_row_chunk(row1_base, i + 1));

      acc_0 = lmac8(acc_0, data_buf2, 2, 0x76543210, coeffs_rest, 0, 0x00000000);
      acc_0 = lmac8(acc_0, data_buf2, 4, 0x76543210, coeffs_rest, 0, 0x00000000);
      acc_0 = lmac8(acc_0, data_buf1, 3, 0x76543210, coeffs_rest, 0, 0x00000000);

      lap_0 = srs(acc_0, 0);

      flux_sub =
          sub16(concat(lap_0,  undef_v8int32()), 0, 0x76543210, 0xFEDCBA98,
                concat(lap_ij, undef_v8int32()), 0, 0x76543210, 0xFEDCBA98);
      sub_v = ext_w(flux_sub, 0);
      ((v8int32*)out_ms_flux2_base)[i]  = sub_v;
      ((v8int32*)out_sel_flux2_base)[i] = sub_v;

      acc_1 = lmul8(data_buf2, 2, 0x76543210, coeffs_rest, 0, 0x00000000);
      acc_0 = lmul8(data_buf2, 2, 0x76543210, coeffs_rest, 0, 0x00000000);

      data_buf2 = upd_w(data_buf2, 0, load_row_chunk(row0_base, i));
      data_buf2 = upd_w(data_buf2, 1, load_row_chunk(row0_base, i + 1));

      acc_1 = lmsc8(acc_1, data_buf1, 2, 0x76543210, coeffs,      0, 0x00000000);
      acc_1 = lmac8(acc_1, data_buf1, 1, 0x76543210, coeffs_rest, 0, 0x00000000);
      acc_1 = lmac8(acc_1, data_buf2, 2, 0x76543210, coeffs_rest, 0, 0x00000000);

      data_buf2 = upd_w(data_buf2, 0, load_row_chunk(row4_base, i));
      data_buf2 = upd_w(data_buf2, 1, load_row_chunk(row4_base, i + 1));

      acc_1 = lmac8(acc_1, data_buf1, 3, 0x76543210, coeffs_rest, 0, 0x00000000);
      acc_0 = lmac8(acc_0, data_buf2, 2, 0x76543210, coeffs_rest, 0, 0x00000000);

      lap_0 = srs(acc_1, 0);

      flux_sub =
          sub16(concat(lap_ij, undef_v8int32()), 0, 0x76543210, 0xFEDCBA98,
                concat(lap_0,  undef_v8int32()), 0, 0x76543210, 0xFEDCBA98);
      sub_v = ext_w(flux_sub, 0);
      ((v8int32*)out_ms_flux3_base)[i]  = sub_v;
      ((v8int32*)out_sel_flux3_base)[i] = sub_v;

      data_buf1 = upd_w(data_buf1, 0, load_row_chunk(row3_base, i));
      data_buf1 = upd_w(data_buf1, 1, load_row_chunk(row3_base, i + 1));

      acc_0 = lmsc8(acc_0, data_buf1, 2, 0x76543210, coeffs,      0, 0x00000000);
      acc_0 = lmac8(acc_0, data_buf1, 1, 0x76543210, coeffs_rest, 0, 0x00000000);

      data_buf2 = upd_w(data_buf2, 0, load_row_chunk(row1_base, i + 1));
      data_buf2 = upd_w(data_buf2, 1, load_row_chunk(row1_base, i + 2));

      acc_0 = lmac8(acc_0, data_buf1, 3, 0x76543210, coeffs_rest, 0, 0x00000000);

      flux_sub =
          sub16(concat(srs(acc_0, 0), undef_v8int32()), 0, 0x76543210, 0xFEDCBA98,
                concat(lap_ij,        undef_v8int32()), 0, 0x76543210, 0xFEDCBA98);
      sub_v = ext_w(flux_sub, 0);
      ((v8int32*)out_ms_flux4_base)[i]  = sub_v;
      ((v8int32*)out_sel_flux4_base)[i] = sub_v;

      data_buf1 = upd_w(data_buf1, 0, load_row_chunk(row3_base, i + 1));
      data_buf1 = upd_w(data_buf1, 1, load_row_chunk(row3_base, i + 2));
    }
}
