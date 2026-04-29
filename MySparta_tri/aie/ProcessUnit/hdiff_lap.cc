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
  for (unsigned i = 0; i < hdiff_cfg::kFluxForwardPackElems; ++i) {
    out[i] = 0;
  }
}

} 

void hdiff_lap(lap_input_buffer& in_window,
               output_buffer<int32_t>& flux_forward_pack) {
  static unsigned warmup_counter = 0;

  if (warmup_counter < hdiff_cfg::kLapWarmupIterations) {
    zero_forward(flux_forward_pack);
    ++warmup_counter;
    return;
  }

  alignas(32) int32_t weights[8]      = {-4, -4, -4, -4, -4, -4, -4, -4};
  alignas(32) int32_t weights_rest[8] = {-1, -1, -1, -1, -1, -1, -1, -1};

  v8int32 coeffs      = *(v8int32*)weights;
  v8int32 coeffs_rest = *(v8int32*)weights_rest;

  int32_t* pack_base  = flux_forward_pack.data();
  int32_t* out_flux1_base = pack_base + (0 * COL);
  int32_t* out_flux2_base = pack_base + (1 * COL);
  int32_t* out_flux3_base = pack_base + (2 * COL);
  int32_t* out_flux4_base = pack_base + (3 * COL);

  int32_t* base_ptr = in_window.data();
  int32_t* row0_base = base_ptr + (0 * COL);
  int32_t* row1_base = base_ptr + (1 * COL);
  int32_t* row2_base = base_ptr + (2 * COL);
  int32_t* row3_base = base_ptr + (3 * COL);
  int32_t* row4_base = base_ptr + (4 * COL);

  v8int32* ptr_out = nullptr;
  v8int32* __restrict row0_ptr = (v8int32*)row0_base;
  v8int32* __restrict row1_ptr = (v8int32*)row1_base;
  v8int32* __restrict row2_ptr = (v8int32*)row2_base;
  v8int32* __restrict row3_ptr = (v8int32*)row3_base;
  v8int32* __restrict row4_ptr = (v8int32*)row4_base;

  v16int32 data_buf1 = null_v16int32();
  v16int32 data_buf2 = null_v16int32();

  v8acc80 acc_0 = null_v8acc80();
  v8acc80 acc_1 = null_v8acc80();

  v8int32 lap_ij = null_v8int32();
  v8int32 lap_0  = null_v8int32();

  data_buf1 = upd_w(data_buf1, 0, *row3_ptr++);
  data_buf1 = upd_w(data_buf1, 1, *row3_ptr);
  data_buf2 = upd_w(data_buf2, 0, *row1_ptr++);
  data_buf2 = upd_w(data_buf2, 1, *row1_ptr);

  for (unsigned i = 0; i < COL / 8; i++)
    chess_prepare_for_pipelining
    chess_loop_range(1, ) {
      v16int32 flux_sub;

      acc_0 = lmul8(data_buf2, 2, 0x76543210, coeffs_rest, 0, 0x00000000);
      acc_1 = lmul8(data_buf2, 1, 0x76543210, coeffs_rest, 0, 0x00000000);

      acc_0 = lmac8(acc_0, data_buf1, 2, 0x76543210, coeffs_rest, 0, 0x00000000);
      acc_1 = lmac8(acc_1, data_buf1, 1, 0x76543210, coeffs_rest, 0, 0x00000000);

      row2_ptr = ((v8int32*)row2_base) + i;
      data_buf2 = upd_w(data_buf2, 0, *(row2_ptr)++);
      data_buf2 = upd_w(data_buf2, 1, *(row2_ptr));

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
      ptr_out = (v8int32*)out_flux1_base + i;
      *ptr_out = ext_w(flux_sub, 0);

      acc_0 = lmul8(data_buf1, 3, 0x76543210, coeffs_rest, 0, 0x00000000);
      acc_0 = lmsc8(acc_0, data_buf2, 3, 0x76543210, coeffs,      0, 0x00000000);

      row1_ptr = ((v8int32*)row1_base) + i;
      data_buf1 = upd_w(data_buf1, 0, *(row1_ptr)++);
      data_buf1 = upd_w(data_buf1, 1, *(row1_ptr));

      acc_0 = lmac8(acc_0, data_buf2, 2, 0x76543210, coeffs_rest, 0, 0x00000000);
      acc_0 = lmac8(acc_0, data_buf2, 4, 0x76543210, coeffs_rest, 0, 0x00000000);
      acc_0 = lmac8(acc_0, data_buf1, 3, 0x76543210, coeffs_rest, 0, 0x00000000);

      lap_0 = srs(acc_0, 0);

      flux_sub =
          sub16(concat(lap_0,  undef_v8int32()), 0, 0x76543210, 0xFEDCBA98,
                concat(lap_ij, undef_v8int32()), 0, 0x76543210, 0xFEDCBA98);
      ptr_out = (v8int32*)out_flux2_base + i;
      *ptr_out = ext_w(flux_sub, 0);

      acc_1 = lmul8(data_buf2, 2, 0x76543210, coeffs_rest, 0, 0x00000000);
      acc_0 = lmul8(data_buf2, 2, 0x76543210, coeffs_rest, 0, 0x00000000);

      row0_ptr = ((v8int32*)row0_base) + i;
      data_buf2 = upd_w(data_buf2, 0, *(row0_ptr)++);
      data_buf2 = upd_w(data_buf2, 1, *(row0_ptr));

      acc_1 = lmsc8(acc_1, data_buf1, 2, 0x76543210, coeffs,      0, 0x00000000);
      acc_1 = lmac8(acc_1, data_buf1, 1, 0x76543210, coeffs_rest, 0, 0x00000000);
      acc_1 = lmac8(acc_1, data_buf2, 2, 0x76543210, coeffs_rest, 0, 0x00000000);

      row4_ptr = ((v8int32*)row4_base) + i;
      data_buf2 = upd_w(data_buf2, 0, *(row4_ptr)++);
      data_buf2 = upd_w(data_buf2, 1, *(row4_ptr));

      acc_1 = lmac8(acc_1, data_buf1, 3, 0x76543210, coeffs_rest, 0, 0x00000000);
      acc_0 = lmac8(acc_0, data_buf2, 2, 0x76543210, coeffs_rest, 0, 0x00000000);

      lap_0 = srs(acc_1, 0);

      flux_sub =
          sub16(concat(lap_ij, undef_v8int32()), 0, 0x76543210, 0xFEDCBA98,
                concat(lap_0,  undef_v8int32()), 0, 0x76543210, 0xFEDCBA98);
      ptr_out = (v8int32*)out_flux3_base + i;
      *ptr_out = ext_w(flux_sub, 0);

      row3_ptr = ((v8int32*)row3_base) + i;
      data_buf1 = upd_w(data_buf1, 0, *(row3_ptr)++);
      data_buf1 = upd_w(data_buf1, 1, *(row3_ptr));

      acc_0 = lmsc8(acc_0, data_buf1, 2, 0x76543210, coeffs,      0, 0x00000000);
      acc_0 = lmac8(acc_0, data_buf1, 1, 0x76543210, coeffs_rest, 0, 0x00000000);

      row1_ptr = ((v8int32*)row1_base) + i + 1;
      data_buf2 = upd_w(data_buf2, 0, *(row1_ptr)++);
      data_buf2 = upd_w(data_buf2, 1, *(row1_ptr));

      acc_0 = lmac8(acc_0, data_buf1, 3, 0x76543210, coeffs_rest, 0, 0x00000000);

      flux_sub =
          sub16(concat(srs(acc_0, 0), undef_v8int32()), 0, 0x76543210, 0xFEDCBA98,
                concat(lap_ij,        undef_v8int32()), 0, 0x76543210, 0xFEDCBA98);
      ptr_out = (v8int32*)out_flux4_base + i;
      *ptr_out = ext_w(flux_sub, 0);

      row3_ptr = ((v8int32*)row3_base) + i + 1;
      data_buf1 = upd_w(data_buf1, 0, *(row3_ptr)++);
      data_buf1 = upd_w(data_buf1, 1, *(row3_ptr));
    }
}
