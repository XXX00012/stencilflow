
#include <adf.h>
#include <aie_api/aie.hpp>
#include <cstdint>
#include "include.h"
#include "hdiff.h"

using namespace adf;

#define kernel_load 14
void hdiff_lap(input_buffer<int32_t>&  in_window,  output_buffer<int32_t>& flux_forward_pack) {

  alignas(32) int32_t weights[8] = {-4, -4, -4, -4, -4, -4, -4, -4};
  alignas(32) int32_t weights1[8] = {1, 1, 1, 1, 1, 1, 1, 1};
  alignas(32) int32_t weights_rest[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
  alignas(32) int32_t flux_out[8] = {-7, -7, -7, -7, -7, -7, -7, -7};

  v8int32 coeffs = *(v8int32 *)weights;           //  8 x int32 = 256b W vector     //  8 x int32 = 256b W vector
  v8int32 coeffs_rest = *(v8int32 *)weights_rest; //  8 x int32 = 256b W vector

  int32_t* pack_base  = flux_forward_pack.data();
  int32_t* out_flux1 = pack_base + (0 * COL);
  int32_t* out_flux2 = pack_base + (1 * COL);
  int32_t* out_flux3 = pack_base + (2 * COL);
  int32_t* out_flux4 = pack_base + (3 * COL);

  int32_t* base_ptr = in_window.data();
  int32_t* row0 = base_ptr + (0 * COL);
  int32_t* row1 = base_ptr + (1 * COL);
  int32_t* row2 = base_ptr + (2 * COL);
  int32_t* row3 = base_ptr + (3 * COL);
  int32_t* row4 = base_ptr + (4 * COL);

  v8int32 *ptr_out = (v8int32 *)out_flux1;
  v8int32 *restrict row0_ptr = (v8int32 *)row0;
  v8int32 *restrict row1_ptr = (v8int32 *)row1;
  v8int32 *restrict row2_ptr = (v8int32 *)row2;
  v8int32 *restrict row3_ptr = (v8int32 *)row3;
  v8int32 *restrict row4_ptr = (v8int32 *)row4;

  v8int32 *restrict r1;

  v16int32 data_buf1 = null_v16int32();
  v16int32 data_buf2 = null_v16int32();

  v8acc80 acc_0 = null_v8acc80();
  v8acc80 acc_1 = null_v8acc80();

  v8int32 lap_ij = null_v8int32(); //  8 x int32 = 256b W vector
  v8int32 lap_0 = null_v8int32();  //  8 x int32 = 256b W vector

  data_buf1 = upd_w(data_buf1, 0, *row3_ptr++);
  data_buf1 = upd_w(data_buf1, 1, *row3_ptr);
  data_buf2 = upd_w(data_buf2, 0, *row1_ptr++);
  data_buf2 = upd_w(data_buf2, 1, *row1_ptr);

  for (unsigned i = 0; i < COL / 8; i++)
    chess_prepare_for_pipelining chess_loop_range(1, ) {
      v16int32 flux_sub;

      acc_0 = lmul8(data_buf2, 2, 0x76543210, coeffs_rest, 0, 0x00000000); // c
      acc_1 = lmul8(data_buf2, 1, 0x76543210, coeffs_rest, 0, 0x00000000); // b

      acc_0 = lmac8(acc_0, data_buf1, 2, 0x76543210, coeffs_rest, 0,
                    0x00000000); // c,k
      acc_1 = lmac8(acc_1, data_buf1, 1, 0x76543210, coeffs_rest, 0,
                    0x00000000); // b,j

      row2_ptr = ((v8int32 *)(row2)) + i;
      data_buf2 = upd_w(data_buf2, 0, *(row2_ptr)++);
      data_buf2 = upd_w(data_buf2, 1, *(row2_ptr));

      acc_0 = lmac8(acc_0, data_buf2, 1, 0x76543210, coeffs_rest, 0,
                    0x00000000); // c,k,f
      acc_0 = lmsc8(acc_0, data_buf2, 2, 0x76543210, coeffs, 0,
                    0x00000000); // c,k,f,4*g
      acc_0 = lmac8(acc_0, data_buf2, 3, 0x76543210, coeffs_rest, 0,
                    0x00000000); // c,k,f,4*g,h

      lap_ij = srs(acc_0, 0); // store lap_ij

      acc_1 = lmac8(acc_1, data_buf2, 0, 0x76543210, coeffs_rest, 0,
                    0x00000000); // b,j,e
      acc_1 = lmsc8(acc_1, data_buf2, 1, 0x76543210, coeffs, 0,
                    0x00000000); // b,j,e,4*f
      acc_1 = lmac8(acc_1, data_buf2, 2, 0x76543210, coeffs_rest, 0,
                    0x00000000); // b,j,e,4*f,g

      // lap_ijm
      lap_0 = srs(acc_1, 0);

      // Calculate  lap_ij - lap_ijm
      flux_sub =
          sub16(concat(lap_ij, undef_v8int32()), 0, 0x76543210, 0xFEDCBA98,
                concat(lap_0, undef_v8int32()), 0, 0x76543210, 0xFEDCBA98);
      ptr_out = (v8int32 *)out_flux1 + i;
      *ptr_out = ext_w(flux_sub, 0);
      //

      acc_0 = lmul8(data_buf1, 3, 0x76543210, coeffs_rest, 0, 0x00000000); // l
      acc_0 = lmsc8(acc_0, data_buf2, 3, 0x76543210, coeffs, 0,
                    0x00000000); // l, 4*h

      row1_ptr = ((v8int32 *)(row1)) + i;
      data_buf1 = upd_w(data_buf1, 0, *(row1_ptr)++);
      data_buf1 = upd_w(data_buf1, 1, *(row1_ptr));

      acc_0 = lmac8(acc_0, data_buf2, 2, 0x76543210, coeffs_rest, 0,
                    0x00000000); // l, 4*h, g
      acc_0 = lmac8(acc_0, data_buf2, 4, 0x76543210, coeffs_rest, 0,
                    0x00000000); // l, 4*h, g, i

      acc_0 = lmac8(acc_0, data_buf1, 3, 0x76543210, coeffs_rest, 0,
                    0x00000000); // l, 4*h, g, i, d

      // Calculates lap_ijp
      lap_0 = srs(acc_0, 0);

      // Calculates lap_ijp - lap_ij
      flux_sub =
          sub16(concat(lap_0, undef_v8int32()), 0, 0x76543210, 0xFEDCBA98,
                concat(lap_ij, undef_v8int32()), 0, 0x76543210, 0xFEDCBA98);
      ptr_out = (v8int32 *)out_flux2 + i;
      *ptr_out = ext_w(flux_sub, 0);

      //***********************************************************************STARTING
      // X
      // FLUX*****************************************************************************************************************************************************

      acc_1 = lmul8(data_buf2, 2, 0x76543210, coeffs_rest, 0, 0x00000000); // g
      acc_0 = lmul8(data_buf2, 2, 0x76543210, coeffs_rest, 0, 0x00000000); // g

      // r2 = ptr_in + 0*COL/8 + i ;
      row0_ptr = ((v8int32 *)(row0)) + i;
      data_buf2 = upd_w(data_buf2, 0, *(row0_ptr)++);
      data_buf2 = upd_w(data_buf2, 1, *(row0_ptr));

      acc_1 = lmsc8(acc_1, data_buf1, 2, 0x76543210, coeffs, 0,
                    0x00000000); // g, 4*c
      acc_1 = lmac8(acc_1, data_buf1, 1, 0x76543210, coeffs_rest, 0,
                    0x00000000); // g, 4*c, b
      acc_1 = lmac8(acc_1, data_buf2, 2, 0x76543210, coeffs_rest, 0,
                    0x00000000); // g, 4*c, b, a

      // r2 = ptr_in + 4*COL/8 + i ;
      row4_ptr = ((v8int32 *)(row4)) + i;
      data_buf2 = upd_w(data_buf2, 0, *(row4_ptr)++);
      data_buf2 = upd_w(data_buf2, 1, *(row4_ptr));

      acc_1 = lmac8(acc_1, data_buf1, 3, 0x76543210, coeffs_rest, 0,
                    0x00000000); // g, 4*c, b, a, d
      acc_0 = lmac8(acc_0, data_buf2, 2, 0x76543210, coeffs_rest, 0,
                    0x00000000); // g, m

      // Calculates lap_imj
      lap_0 = srs(acc_1, 0);

      flux_sub =
          sub16(concat(lap_ij, undef_v8int32()), 0, 0x76543210, 0xFEDCBA98,
                concat(lap_0, undef_v8int32()), 0, 0x76543210, 0xFEDCBA98);
      ptr_out = (v8int32 *)out_flux3 + i;
      *ptr_out = ext_w(flux_sub, 0);

      row3_ptr = ((v8int32 *)(row3)) + i;
      data_buf1 = upd_w(data_buf1, 0, *(row3_ptr)++);
      data_buf1 = upd_w(data_buf1, 1, *(row3_ptr));

      acc_0 = lmsc8(acc_0, data_buf1, 2, 0x76543210, coeffs, 0,
                    0x00000000); // g, m , k * 4


      acc_0 = lmac8(acc_0, data_buf1, 1, 0x76543210, coeffs_rest, 0,
                    0x00000000); // g, m , k * 4, j

      // LOAD DATA FOR NEXT ITERATION
      row1_ptr = ((v8int32 *)(row1)) + i + 1;
      data_buf2 = upd_w(data_buf2, 0, *(row1_ptr)++);
      data_buf2 = upd_w(data_buf2, 1, *(row1_ptr));

      acc_0 = lmac8(acc_0, data_buf1, 3, 0x76543210, coeffs_rest, 0,
                    0x00000000); // g, m , k * 4, j, l

      flux_sub = sub16(concat(srs(acc_0, 0), undef_v8int32()), 0, 0x76543210,
                       0xFEDCBA98, concat(lap_ij, undef_v8int32()), 0,
                       0x76543210, 0xFEDCBA98);
      ptr_out = (v8int32 *)out_flux4 + i;
      *ptr_out = ext_w(flux_sub, 0);

      // LOAD DATA FOR NEXT ITERATION
      row3_ptr = ((v8int32 *)(row3)) + i + 1;
      data_buf1 = upd_w(data_buf1, 0, *(row3_ptr)++);
      data_buf1 = upd_w(data_buf1, 1, *(row3_ptr));

    }
}
