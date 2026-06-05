#include <adf.h>
#include <aie_api/aie.hpp>
#include <cstdint>
#include "include.h"
#include "hdiff.h"

using namespace adf;

#define kernel_load 14

void hdiff_update(input_buffer<int32_t>& in_dx,
                  input_buffer<int32_t>& in_dy,
                  psi_center_input_buffer& in_psi_stream,
                  output_buffer<int32_t>& out_row) {
  alignas(32) int32_t ones_arr[8] = {1, 1, 1, 1, 1, 1, 1, 1};
  alignas(32) int32_t c_arr[8] = {
      hdiff_cfg::kUpdateCoeff, hdiff_cfg::kUpdateCoeff,
      hdiff_cfg::kUpdateCoeff, hdiff_cfg::kUpdateCoeff,
      hdiff_cfg::kUpdateCoeff, hdiff_cfg::kUpdateCoeff,
      hdiff_cfg::kUpdateCoeff, hdiff_cfg::kUpdateCoeff};

  v8int32 ones_vec = *(v8int32*)ones_arr;
  v8int32 c_vec    = *(v8int32*)c_arr;

  v8int32* __restrict dx = (v8int32*)in_dx.data();
  v8int32* __restrict dy = (v8int32*)in_dy.data();

  int32_t* __restrict psi_base = in_psi_stream.data() - (2 * COL);
  v8int32* __restrict psi      = (v8int32*)psi_base;
  v8int32* __restrict out      = (v8int32*)out_row.data();

  for (unsigned i = 0; i < COL / 8; ++i)
    chess_prepare_for_pipelining
    chess_loop_range(1, ) {
      v16int32 sum = add16(concat(dx[i], undef_v8int32()),
                           concat(dy[i], undef_v8int32()));

      v8acc80 acc =
          lmul8(concat(psi[i], undef_v8int32()), 0, 0x76543210,
                ones_vec, 0, 0x00000000);

      acc =
          lmsc8(acc,
                sum, 0, 0x76543210,
                c_vec, 0, 0x00000000);

      out[i] = srs(acc, 0);
    }
}
