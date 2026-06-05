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

inline v8int32 mul_sub_vec(v8int32 factor,
                           v16int32 pos_buf, int pos_offset,
                           v16int32 neg_buf, int neg_offset) {
  v8acc80 acc =
      lmul8(pos_buf, pos_offset, 0x76543210,
            factor, 0, 0x76543210);
  acc =
      lmsc8(acc, neg_buf, neg_offset, 0x76543210,
            factor, 0, 0x76543210);
  return srs(acc, 0);
}

inline v8int32 select_flux(v8int32 product, v8int32 diff) {
  unsigned int take_mask =
      gt16(concat(product, undef_v8int32()), 0,
           0x76543210, 0xFEDCBA98,
           null_v16int32(), 0,
           0x76543210, 0xFEDCBA98);

  v16int32 selected =
      select16(take_mask,
               concat(diff, undef_v8int32()),
               null_v16int32());
  return ext_w(selected, 0);
}

} // namespace

void hdiff_flux_x(lap_package_input_buffer& in_lap_package,
                  psi_complete_input_buffer& in_psi_stream,
                  output_buffer<int32_t>& out_dx) {
  int32_t* __restrict lap_base = in_lap_package.data();
  v8int32* __restrict lap_left  = (v8int32*)(lap_base + (0 * COL));
  v8int32* __restrict lap_right = (v8int32*)(lap_base + (1 * COL));
  v8int32* __restrict lap_mid   = (v8int32*)(lap_base + (3 * COL));

  int32_t* __restrict psi_base     = in_psi_stream.data();
  int32_t* __restrict row_mid_base = psi_base - (2 * COL);

  v8int32* __restrict out = (v8int32*)out_dx.data();

  v8int32* __restrict mid_ptr = (v8int32*)row_mid_base;
  v16int32 mid_buf = null_v16int32();

  mid_buf = upd_w(mid_buf, 0, *mid_ptr++);
  mid_buf = upd_w(mid_buf, 1, *mid_ptr);

  for (unsigned i = 0; i < COL / 8; ++i)
    chess_prepare_for_pipelining
    chess_loop_range(1, ) {
      v8int32 mid_lap_v = lap_mid[i];

      v8int32 delta_left  = sub_vec(mid_lap_v, lap_left[i]);
      v8int32 delta_right = sub_vec(lap_right[i], mid_lap_v);

      v8int32 product_left =
          mul_sub_vec(delta_left,
                      mid_buf, 2,
                      mid_buf, 1);
      v8int32 product_right =
          mul_sub_vec(delta_right,
                      mid_buf, 3,
                      mid_buf, 2);

      v8int32 flux_left  = select_flux(product_left, delta_left);
      v8int32 flux_right = select_flux(product_right, delta_right);

      out[i] = sub_vec(flux_right, flux_left);

      mid_ptr = ((v8int32*)row_mid_base) + i + 1;
      mid_buf = upd_w(mid_buf, 0, *mid_ptr++);
      mid_buf = upd_w(mid_buf, 1, *mid_ptr);
    }
}
