#include <adf.h>
#include <aie_api/aie.hpp>
#include <cstdint>

#include "include.h"
#include "hdiff.h"

using namespace adf;

namespace {

inline v8int32 load_row_chunk(std::int32_t* row_base, unsigned chunk) {
  return ((v8int32*)row_base)[chunk];
}

inline v16int32 select_flux(unsigned int take_mask, v8int32 diff_vec) {
  return select16(take_mask,
                  concat(diff_vec, undef_v8int32()),
                  null_v16int32());
}

} // namespace

void hdiff_flux2(input_buffer<std::int32_t>& mask_pack,
                 flux2_input_buffer& in_window,
                 input_buffer<std::int32_t>& sub_pack,
                 output_buffer<std::int32_t>& out) {
  alignas(32) std::int32_t weights1[8] = {1, 1, 1, 1, 1, 1, 1, 1};
  alignas(32) std::int32_t flux_out_arr[8] = {-7, -7, -7, -7, -7, -7, -7, -7};

  std::int32_t* __restrict mask_base = mask_pack.data();
  std::int32_t* __restrict raw_base  = in_window.data();
  std::int32_t* __restrict sub_base  = sub_pack.data();
  std::int32_t* __restrict out_base  = out.data();

  v8int32 coeffs1 = *(v8int32*)weights1;
  v8int32 flux_out_coeff = *(v8int32*)flux_out_arr;

  for (unsigned r = 0; r < hdiff_cfg::kRowsPerCall; ++r) {
    const unsigned in_off   = r * COL;
    const unsigned sub_off  = r * hdiff_cfg::kFluxForwardPackRows * COL;
    const unsigned mask_off = r * 4 * hdiff_cfg::kFluxMaskWordsPerRow;
    const unsigned out_off  = r * COL;

    std::int32_t* __restrict sub_left  = sub_base + sub_off + (0 * COL);
    std::int32_t* __restrict sub_right = sub_base + sub_off + (1 * COL);
    std::int32_t* __restrict sub_up    = sub_base + sub_off + (2 * COL);
    std::int32_t* __restrict sub_down  = sub_base + sub_off + (3 * COL);
    std::int32_t* __restrict mask_left  =
        mask_base + mask_off + (0 * hdiff_cfg::kFluxMaskWordsPerRow);
    std::int32_t* __restrict mask_right =
        mask_base + mask_off + (1 * hdiff_cfg::kFluxMaskWordsPerRow);
    std::int32_t* __restrict mask_up =
        mask_base + mask_off + (2 * hdiff_cfg::kFluxMaskWordsPerRow);
    std::int32_t* __restrict mask_down =
        mask_base + mask_off + (3 * hdiff_cfg::kFluxMaskWordsPerRow);
    // data() points at the margin/history region followed by the current row.
    std::int32_t* __restrict row2_base = raw_base + in_off + (1 * COL);
    std::int32_t* __restrict out_row   = out_base + out_off;

    v8int32* __restrict ptr_out = nullptr;
    v16int32 data_buf2 = null_v16int32();

    data_buf2 = upd_w(data_buf2, 0, load_row_chunk(row2_base, 0));
    data_buf2 = upd_w(data_buf2, 1, load_row_chunk(row2_base, 1));

    for (unsigned i = 0; i < COL / 8; ++i)
    chess_prepare_for_pipelining
    chess_loop_range(1, ) {
      v8int32 flux_sub;

      flux_sub = ((v8int32*)sub_left)[i];
      v16int32 out_flx_inter1 =
          select_flux((unsigned int)mask_left[i], flux_sub);

      flux_sub = ((v8int32*)sub_right)[i];
      v16int32 out_flx_inter2 =
          select_flux((unsigned int)mask_right[i], flux_sub);

      v16int32 flx_out2 = sub16(out_flx_inter2, out_flx_inter1);

      flux_sub = ((v8int32*)sub_up)[i];
      v16int32 out_flx_inter3 =
          select_flux((unsigned int)mask_up[i], flux_sub);

      v16int32 flx_out3 = sub16(flx_out2, out_flx_inter3);

      flux_sub = ((v8int32*)sub_down)[i];
      v16int32 out_flx_inter4 =
          select_flux((unsigned int)mask_down[i], flux_sub);

      v16int32 flx_out4 = add16(flx_out3, out_flx_inter4);

      v8acc80 final_output =
          lmul8(flx_out4, 0, 0x76543210, flux_out_coeff, 0, 0x00000000);
      final_output =
          lmac8(final_output, data_buf2, 2, 0x76543210,
                concat(coeffs1, undef_v8int32()), 0, 0x76543210);

      ptr_out = (v8int32*)out_row + i;
      *ptr_out = srs(final_output, 0);

      data_buf2 = upd_w(data_buf2, 0, load_row_chunk(row2_base, i + 1));
      data_buf2 = upd_w(data_buf2, 1, load_row_chunk(row2_base, i + 2));
    }
  }
}
