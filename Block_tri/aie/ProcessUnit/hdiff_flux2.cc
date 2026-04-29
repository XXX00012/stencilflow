
#include <adf.h>
#include <aie_api/aie.hpp>
#include <cstdint>

#include "include.h"
#include "hdiff.h"

#define kernel_load 14

using namespace adf;

static void hdiff_flux2_body(input_buffer<int32_t>& flux_inter_pack,
                             int32_t* out_ptr) {
  alignas(32) int32_t weights1[8] = {1, 1, 1, 1, 1, 1, 1, 1};
  alignas(32) int32_t flux_out[8] = {-7, -7, -7, -7, -7, -7, -7, -7};

  v8int32 coeffs1 = *(v8int32*)weights1;
  v8int32 flux_out_coeff = *(v8int32*)flux_out;

  int32_t* inter_pack_base = flux_inter_pack.data();
  int32_t* flux_inter1 = inter_pack_base + (0 * hdiff_cfg::kFluxInterElems);
  int32_t* flux_inter2 = inter_pack_base + (1 * hdiff_cfg::kFluxInterElems);
  int32_t* flux_inter3 = inter_pack_base + (2 * hdiff_cfg::kFluxInterElems);
  int32_t* flux_inter4 = inter_pack_base + (3 * hdiff_cfg::kFluxInterElems);
  int32_t* flux_inter5 = inter_pack_base + (4 * hdiff_cfg::kFluxInterElems);

  v8int32* ptr_forward = nullptr;
  v8int32* ptr_out = (v8int32*)out_ptr;

  v16int32 data_buf2 = null_v16int32();

  v8acc80 acc_0 = null_v8acc80();
  v8acc80 acc_1 = null_v8acc80();

  for (unsigned i = 0; i < COL / 8; i++)
    chess_prepare_for_pipelining
    chess_loop_range(1, ) {
      v8int32 flux_sub;
      v8int32 flux_interm_sub;

      ptr_forward = (v8int32*)flux_inter1 + 2 * i;
      flux_sub = *ptr_forward++;
      flux_interm_sub = *ptr_forward;

      unsigned int flx_compare_imj =
          gt16(concat(flux_interm_sub, undef_v8int32()), 0, 0x76543210,
               0xFEDCBA98, null_v16int32(), 0, 0x76543210, 0xFEDCBA98);

      v16int32 out_flx_inter1 =
          select16(flx_compare_imj, concat(flux_sub, undef_v8int32()),
                   null_v16int32());

      ptr_forward = (v8int32*)flux_inter2 + 2 * i;
      flux_sub = *ptr_forward++;
      flux_interm_sub = *ptr_forward;

      unsigned int flx_compare_ij =
          gt16(concat(flux_interm_sub, undef_v8int32()), 0, 0x76543210,
               0xFEDCBA98, null_v16int32(), 0, 0x76543210, 0xFEDCBA98);

      v16int32 out_flx_inter2 =
          select16(flx_compare_ij, concat(flux_sub, undef_v8int32()),
                   null_v16int32());

      v16int32 flx_out2 = sub16(out_flx_inter2, out_flx_inter1);

      ptr_forward = (v8int32*)flux_inter3 + 2 * i;
      flux_sub = *ptr_forward++;
      flux_interm_sub = *ptr_forward;

      unsigned int fly_compare_ijm =
          gt16(concat(flux_interm_sub, undef_v8int32()), 0, 0x76543210,
               0xFEDCBA98, null_v16int32(), 0, 0x76543210, 0xFEDCBA98);

      v16int32 out_flx_inter3 =
          select16(fly_compare_ijm, concat(flux_sub, undef_v8int32()),
                   null_v16int32());

      v16int32 flx_out3 = sub16(flx_out2, out_flx_inter3);

      ptr_forward = (v8int32*)flux_inter4 + 2 * i;
      flux_sub = *ptr_forward++;
      flux_interm_sub = *ptr_forward;

      unsigned int fly_compare_ij =
          gt16(concat(flux_interm_sub, undef_v8int32()), 0, 0x76543210,
               0xFEDCBA98, null_v16int32(), 0, 0x76543210, 0xFEDCBA98);

      v16int32 out_flx_inter4 =
          select16(fly_compare_ij, concat(flux_sub, undef_v8int32()),
                   null_v16int32());

      v16int32 flx_out4 = add16(flx_out3, out_flx_inter4);

      ptr_forward = (v8int32*)flux_inter5 + 2 * i;
      v8int32 tmp1 = *ptr_forward++;
      v8int32 tmp2 = *ptr_forward;
      data_buf2 = concat(tmp2, tmp1);

      v8acc80 final_output =
          lmul8(flx_out4, 0, 0x76543210, flux_out_coeff, 0, 0x00000000);
      final_output =
          lmac8(final_output, data_buf2, 2, 0x76543210,
                concat(coeffs1, undef_v8int32()), 0, 0x76543210);

      *ptr_out++ = srs(final_output, 0);
    }
}

void hdiff_flux2(input_buffer<int32_t>& flux_inter_pack,
                 output_buffer<int32_t>& out) {
  hdiff_flux2_body(flux_inter_pack, out.data());
}
void hdiff_flux2_gather(input_buffer<int32_t>& flux_inter_pack,
                        input_buffer<int32_t>& row_from_lane0,
                        input_buffer<int32_t>& row_from_lane2,
                        input_buffer<int32_t>& row_from_lane3,
                        output_buffer<int32_t>& block_out) {
  int32_t* dst = block_out.data();
  const int32_t* src0 = row_from_lane0.data();
  const int32_t* src2 = row_from_lane2.data();
  const int32_t* src3 = row_from_lane3.data();

  // 直接把本核结果写到 block_out 的第 2 行
  hdiff_flux2_body(flux_inter_pack, dst + 1 * COL);

  for (int i = 0; i < COL; ++i) {
    dst[0 * COL + i] = src0[i];
    dst[2 * COL + i] = src2[i];
    dst[3 * COL + i] = src3[i];
  }
}