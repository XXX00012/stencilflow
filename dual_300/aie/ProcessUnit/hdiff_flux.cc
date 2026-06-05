#include <adf.h>
#include <aie_api/aie.hpp>
#include <cstdint>

#include "include.h"
#include "hdiff.h"

using namespace adf;

#define kernel_load 14

void hdiff_flux(flux_input_buffer& in_window,
                input_buffer<int32_t>& flux_forward_pack,
                output_buffer<int32_t>& out) {
  alignas(32) int32_t weights1[8] = {1, 1, 1, 1, 1, 1, 1, 1};
  alignas(32) int32_t flux_out_arr[8] = {-7, -7, -7, -7, -7, -7, -7, -7};

  v8int32 coeffs1 = *(v8int32*)weights1;
  v8int32 flux_out_coeff = *(v8int32*)flux_out_arr;

  int32_t* __restrict base_ptr = in_window.data();

  int32_t* __restrict row1_base = base_ptr + (0 * COL);
  int32_t* __restrict row2_base = base_ptr + (1 * COL);
  int32_t* __restrict row3_base = base_ptr + (2 * COL);

  int32_t* __restrict forward_pack_base = flux_forward_pack.data();
  int32_t* __restrict flux_forward1 = forward_pack_base + (0 * COL);
  int32_t* __restrict flux_forward2 = forward_pack_base + (1 * COL);
  int32_t* __restrict flux_forward3 = forward_pack_base + (2 * COL);
  int32_t* __restrict flux_forward4 = forward_pack_base + (3 * COL);
  int32_t* __restrict out_base = out.data();

  v8int32* __restrict row1_ptr = (v8int32*)row1_base;
  v8int32* __restrict row2_ptr = (v8int32*)row2_base;
  v8int32* __restrict row3_ptr = (v8int32*)row3_base;
  v8int32* __restrict ptr_forward = nullptr;
  v8int32* __restrict ptr_out = (v8int32*)out_base;

  v16int32 data_buf1 = null_v16int32();
  v16int32 data_buf2 = null_v16int32();

  v8acc80 acc_0 = null_v8acc80();
  v8acc80 acc_1 = null_v8acc80();

  data_buf1 = upd_w(data_buf1, 0, *row1_ptr++);
  data_buf1 = upd_w(data_buf1, 1, *row1_ptr);

  data_buf2 = upd_w(data_buf2, 0, *row2_ptr++);
  data_buf2 = upd_w(data_buf2, 1, *row2_ptr);

  for (unsigned i = 0; i < COL / 8; ++i)
    chess_prepare_for_pipelining
    chess_loop_range(1, ) {
      v8int32 flux_sub;

      ptr_forward = (v8int32*)flux_forward1 + i;
      flux_sub = *ptr_forward;
      acc_1 = lmul8(data_buf2, 2, 0x76543210, flux_sub, 0, 0x00000000);
      acc_1 = lmsc8(acc_1, data_buf2, 1, 0x76543210, flux_sub, 0, 0x00000000);

      unsigned int flx_compare_imj =
          gt16(concat(srs(acc_1, 0), undef_v8int32()), 0, 0x76543210,
               0xFEDCBA98, null_v16int32(), 0, 0x76543210, 0xFEDCBA98);
      v16int32 out_flx_inter1 =
          select16(flx_compare_imj, concat(flux_sub, undef_v8int32()),
                   null_v16int32());

      ptr_forward = (v8int32*)flux_forward2 + i;
      flux_sub = *ptr_forward;
      acc_0 = lmul8(data_buf2, 3, 0x76543210, flux_sub, 0, 0x00000000);
      acc_0 = lmsc8(acc_0, data_buf2, 2, 0x76543210, flux_sub, 0, 0x00000000);

      unsigned int flx_compare_ij =
          gt16(concat(srs(acc_0, 0), undef_v8int32()), 0, 0x76543210,
               0xFEDCBA98, null_v16int32(), 0, 0x76543210, 0xFEDCBA98);
      v16int32 out_flx_inter2 =
          select16(flx_compare_ij, concat(flux_sub, undef_v8int32()),
                   null_v16int32());
      v16int32 flx_out2 = sub16(out_flx_inter2, out_flx_inter1);

      ptr_forward = (v8int32*)flux_forward3 + i;
      flux_sub = *ptr_forward;
      acc_1 = lmul8(data_buf2, 2, 0x76543210, flux_sub, 0, 0x00000000);
      acc_1 = lmsc8(acc_1, data_buf1, 2, 0x76543210, flux_sub, 0, 0x00000000);

      row3_ptr = ((v8int32*)row3_base) + i;
      data_buf1 = upd_w(data_buf1, 0, *row3_ptr++);
      data_buf1 = upd_w(data_buf1, 1, *row3_ptr);

      unsigned int fly_compare_ijm =
          gt16(concat(srs(acc_1, 0), undef_v8int32()), 0, 0x76543210,
               0xFEDCBA98, null_v16int32(), 0, 0x76543210, 0xFEDCBA98);
      v16int32 out_flx_inter3 =
          select16(fly_compare_ijm, concat(flux_sub, undef_v8int32()),
                   null_v16int32());
      v16int32 flx_out3 = sub16(flx_out2, out_flx_inter3);

      ptr_forward = (v8int32*)flux_forward4 + i;
      flux_sub = *ptr_forward;
      acc_1 = lmul8(data_buf1, 2, 0x76543210, flux_sub, 0, 0x00000000);
      acc_1 = lmsc8(acc_1, data_buf2, 2, 0x76543210, flux_sub, 0, 0x00000000);

      row1_ptr = ((v8int32*)row1_base) + i + 1;
      data_buf1 = upd_w(data_buf1, 0, *row1_ptr++);
      data_buf1 = upd_w(data_buf1, 1, *row1_ptr);

      unsigned int fly_compare_ij =
          gt16(concat(srs(acc_1, 0), undef_v8int32()), 0, 0x76543210,
               0xFEDCBA98, null_v16int32(), 0, 0x76543210, 0xFEDCBA98);
      v16int32 out_flx_inter4 =
          select16(fly_compare_ij, concat(flux_sub, undef_v8int32()),
                   null_v16int32());
      v16int32 flx_out4 = add16(flx_out3, out_flx_inter4);

      v8acc80 final_output =
          lmul8(flx_out4, 0, 0x76543210, flux_out_coeff, 0, 0x00000000);
      final_output =
          lmac8(final_output, data_buf2, 2, 0x76543210,
                concat(coeffs1, undef_v8int32()), 0, 0x76543210);

      row2_ptr = ((v8int32*)row2_base) + i + 1;
      data_buf2 = upd_w(data_buf2, 0, *row2_ptr++);
      data_buf2 = upd_w(data_buf2, 1, *row2_ptr);

      *ptr_out++ = srs(final_output, 0);
    }
}
