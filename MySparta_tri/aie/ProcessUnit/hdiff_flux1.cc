#include <adf.h>
#include <aie_api/aie.hpp>
#include <cstdint>
#include "include.h"
#include "hdiff.h"

using namespace adf;

#define kernel_load 14



void hdiff_flux1(flux1_input_buffer& in_window,
                 input_buffer<int32_t>& flux_forward_pack,
                 output_buffer<int32_t>& flux_inter_pack) {

int32_t* base_ptr = in_window.data();


int32_t* row1_base = base_ptr + (0 * COL);  
int32_t* row2_base = base_ptr + (1 * COL);  
int32_t* row3_base = base_ptr + (2 * COL); 

  int32_t* forward_pack_base = flux_forward_pack.data();
  int32_t* flux_forward1 = forward_pack_base + (0 * COL);
  int32_t* flux_forward2 = forward_pack_base + (1 * COL);
  int32_t* flux_forward3 = forward_pack_base + (2 * COL);
  int32_t* flux_forward4 = forward_pack_base + (3 * COL);

  int32_t* inter_pack_base = flux_inter_pack.data();
  int32_t* flux_inter1 = inter_pack_base + (0 * hdiff_cfg::kFluxInterElems);
  int32_t* flux_inter2 = inter_pack_base + (1 * hdiff_cfg::kFluxInterElems);
  int32_t* flux_inter3 = inter_pack_base + (2 * hdiff_cfg::kFluxInterElems);
  int32_t* flux_inter4 = inter_pack_base + (3 * hdiff_cfg::kFluxInterElems);
  int32_t* flux_inter5 = inter_pack_base + (4 * hdiff_cfg::kFluxInterElems);

  v8int32* __restrict row1_ptr = (v8int32*)row1_base;
  v8int32* __restrict row2_ptr = (v8int32*)row2_base;
  v8int32* __restrict row3_ptr = (v8int32*)row3_base;

  v8int32* ptr_forward = nullptr;
  v8int32* ptr_out = nullptr;

  v16int32 data_buf1 = null_v16int32();
  v16int32 data_buf2 = null_v16int32();

  v8acc80 acc_0 = null_v8acc80();
  v8acc80 acc_1 = null_v8acc80();

  data_buf1 = upd_w(data_buf1, 0, *row1_ptr++);
  data_buf1 = upd_w(data_buf1, 1, *row1_ptr);

  data_buf2 = upd_w(data_buf2, 0, *row2_ptr++);
  data_buf2 = upd_w(data_buf2, 1, *row2_ptr);

  for (unsigned i = 0; i < COL / 8; i++)
    chess_prepare_for_pipelining
    chess_loop_range(1, ) {
      v8int32 flux_sub;

      ptr_forward = (v8int32*)flux_forward1 + i;
      flux_sub = *ptr_forward;

      acc_1 = lmul8(data_buf2, 2, 0x76543210, flux_sub, 0, 0x00000000);
      acc_1 = lmsc8(acc_1, data_buf2, 1, 0x76543210, flux_sub, 0, 0x00000000);

      ptr_out = (v8int32*)flux_inter1 + (i * 2);
      *ptr_out++ = flux_sub;
      *ptr_out = srs(acc_1, 0);

      ptr_forward = (v8int32*)flux_forward2 + i;
      flux_sub = *ptr_forward;

      acc_0 = lmul8(data_buf2, 3, 0x76543210, flux_sub, 0, 0x00000000);
      acc_0 = lmsc8(acc_0, data_buf2, 2, 0x76543210, flux_sub, 0, 0x00000000);

      ptr_out = (v8int32*)flux_inter2 + (i * 2);
      *ptr_out++ = flux_sub;
      *ptr_out = srs(acc_0, 0);

      ptr_forward = (v8int32*)flux_forward3 + i;
      flux_sub = *ptr_forward;

      acc_1 = lmul8(data_buf2, 2, 0x76543210, flux_sub, 0, 0x00000000);
      acc_1 = lmsc8(acc_1, data_buf1, 2, 0x76543210, flux_sub, 0, 0x00000000);

      ptr_out = (v8int32*)flux_inter3 + (i * 2);
      *ptr_out++ = flux_sub;
      *ptr_out = srs(acc_1, 0);

      row3_ptr = ((v8int32*)row3_base) + i;
      data_buf1 = upd_w(data_buf1, 0, *row3_ptr++);
      data_buf1 = upd_w(data_buf1, 1, *row3_ptr);

      ptr_forward = (v8int32*)flux_forward4 + i;
      flux_sub = *ptr_forward;

      acc_1 = lmul8(data_buf1, 2, 0x76543210, flux_sub, 0, 0x00000000);
      acc_1 = lmsc8(acc_1, data_buf2, 2, 0x76543210, flux_sub, 0, 0x00000000);

      ptr_out = (v8int32*)flux_inter4 + (i * 2);
      *ptr_out++ = flux_sub;
      *ptr_out = srs(acc_1, 0);

      row1_ptr = ((v8int32*)row1_base) + i + 1;
      data_buf1 = upd_w(data_buf1, 0, *row1_ptr++);
      data_buf1 = upd_w(data_buf1, 1, *row1_ptr);

      ptr_out = (v8int32*)flux_inter5 + (i * 2);
      *ptr_out++ = ext_w(data_buf2, 1);
      *ptr_out = ext_w(data_buf2, 0);

      row2_ptr = ((v8int32*)row2_base) + i + 1;
      data_buf2 = upd_w(data_buf2, 0, *row2_ptr++);
      data_buf2 = upd_w(data_buf2, 1, *row2_ptr);
    }


}
