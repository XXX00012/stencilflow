#include <adf.h>
#include <aie_api/aie.hpp>
#include <cstdint>

#include "include.h"
#include "hdiff.h"

using namespace adf;

namespace {

inline void hdiff_flux2_core(std::int32_t* __restrict flux_inter_pack,
                             std::int32_t* __restrict out) {
  alignas(32) std::int32_t weights1[8] = {1, 1, 1, 1, 1, 1, 1, 1};
  alignas(32) std::int32_t flux_out_arr[8] = {-7, -7, -7, -7, -7, -7, -7, -7};

  std::int32_t* flux_inter1 = flux_inter_pack + (0 * hdiff_cfg::kFluxInterElems);
  std::int32_t* flux_inter2 = flux_inter_pack + (1 * hdiff_cfg::kFluxInterElems);
  std::int32_t* flux_inter3 = flux_inter_pack + (2 * hdiff_cfg::kFluxInterElems);
  std::int32_t* flux_inter4 = flux_inter_pack + (3 * hdiff_cfg::kFluxInterElems);
  std::int32_t* flux_inter5 = flux_inter_pack + (4 * hdiff_cfg::kFluxInterElems);

  v8int32 coeffs1 = *(v8int32*)weights1;
  v8int32 flux_out_coeff = *(v8int32*)flux_out_arr;

  v8int32* ptr_in = nullptr;
  v8int32* ptr_out = nullptr;

  for (unsigned i = 0; i < COL / 8; ++i)
    chess_prepare_for_pipelining
    chess_loop_range(1, ) {
      v8int32 flux_sub;
      v8int32 flux_interm_sub;

      ptr_in = (v8int32*)flux_inter1 + (i * 2);
      flux_sub = *ptr_in++;
      flux_interm_sub = *ptr_in;

      unsigned int flx_compare_imj =
          gt16(concat(flux_interm_sub, undef_v8int32()), 0, 0x76543210,
               0xFEDCBA98, null_v16int32(), 0, 0x76543210, 0xFEDCBA98);

      v16int32 out_flx_inter1 =
          select16(flx_compare_imj, concat(flux_sub, undef_v8int32()),
                   null_v16int32());

      ptr_in = (v8int32*)flux_inter2 + (i * 2);
      flux_sub = *ptr_in++;
      flux_interm_sub = *ptr_in;

      unsigned int flx_compare_ij =
          gt16(concat(flux_interm_sub, undef_v8int32()), 0, 0x76543210,
               0xFEDCBA98, null_v16int32(), 0, 0x76543210, 0xFEDCBA98);

      v16int32 out_flx_inter2 =
          select16(flx_compare_ij, concat(flux_sub, undef_v8int32()),
                   null_v16int32());

      v16int32 flx_out2 = sub16(out_flx_inter2, out_flx_inter1);

      ptr_in = (v8int32*)flux_inter3 + (i * 2);
      flux_sub = *ptr_in++;
      flux_interm_sub = *ptr_in;

      unsigned int fly_compare_ijm =
          gt16(concat(flux_interm_sub, undef_v8int32()), 0, 0x76543210,
               0xFEDCBA98, null_v16int32(), 0, 0x76543210, 0xFEDCBA98);

      v16int32 out_flx_inter3 =
          select16(fly_compare_ijm, concat(flux_sub, undef_v8int32()),
                   null_v16int32());

      v16int32 flx_out3 = sub16(flx_out2, out_flx_inter3);

      ptr_in = (v8int32*)flux_inter4 + (i * 2);
      flux_sub = *ptr_in++;
      flux_interm_sub = *ptr_in;

      unsigned int fly_compare_ij =
          gt16(concat(flux_interm_sub, undef_v8int32()), 0, 0x76543210,
               0xFEDCBA98, null_v16int32(), 0, 0x76543210, 0xFEDCBA98);

      v16int32 out_flx_inter4 =
          select16(fly_compare_ij, concat(flux_sub, undef_v8int32()),
                   null_v16int32());

      v16int32 flx_out4 = add16(flx_out3, out_flx_inter4);

      ptr_in = (v8int32*)flux_inter5 + (i * 2);
      v8int32 tmp1 = *ptr_in++;
      v8int32 tmp2 = *ptr_in;
      v16int32 data_buf2 = concat(tmp2, tmp1);

      v8acc80 final_output =
          lmul8(flx_out4, 0, 0x76543210, flux_out_coeff, 0, 0x00000000);
      final_output =
          lmac8(final_output, data_buf2, 2, 0x76543210,
                concat(coeffs1, undef_v8int32()), 0, 0x76543210);

      ptr_out = (v8int32*)out + i;
      *ptr_out = srs(final_output, 0);
    }
}

} // namespace

void hdiff_flux2(input_buffer<std::int32_t>& flux_inter_pack,
                 output_buffer<std::int32_t>& out) {
  hdiff_flux2_core(flux_inter_pack.data(), out.data());
}
