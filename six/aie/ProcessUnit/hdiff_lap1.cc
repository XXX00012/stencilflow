// #include <adf.h>
// #include <aie_api/aie.hpp>
// #include <cstdint>
// #include "include.h"
// #include "hdiff.h"

// using namespace adf;

// #define kernel_load 14

// namespace {

// inline void zero_forward(output_buffer<int32_t>& out_buf) {
//   int32_t* out = out_buf.data();
//   for (unsigned i = 0; i < 3 * COL; ++i) {
//     out[i] = 0;
//   }
// }
// //刚传入第一行数据没用，缓存一下预热
// } 

// void hdiff_lap1(
//     lap1_input_buffer& in_window,
//     output_buffer<int32_t>& out_package) {
//   static unsigned warmup_counter = 0;

//   if (warmup_counter < 1) {
//     zero_forward(out_package);
//     ++warmup_counter;
//     return;
//   }//这里是因为lap1读入一行没工作，但是不确定是预热1还是预热128，
//   //不过其实不预热也可以，第一次输出的结果自己知道是错的就行，后期再改

//  alignas(32) int32_t weights[8]      = {-4, -4, -4, -4, -4, -4, -4, -4};
//   alignas(32) int32_t weights_rest[8] = {-1, -1, -1, -1, -1, -1, -1, -1};

//   v8int32 coeffs      = *(v8int32*)weights;
//   v8int32 coeffs_rest = *(v8int32*)weights_rest;

//   int32_t* base_ptr = in_window.data();
//   int32_t* prev_row_base = base_ptr + (0 * COL);
//   int32_t* curr_row_base = base_ptr + (1 * COL);

//   int32_t* pack_base = out_package.data();
//   v8int32* out_mid   = (v8int32*)(pack_base + (0 * COL));
//   v8int32* out_left  = (v8int32*)(pack_base + (1 * COL));
//   v8int32* out_right = (v8int32*)(pack_base + (2 * COL));

//   v8int32* __restrict prev_ptr = (v8int32*)prev_row_base;
//   v8int32* __restrict curr_ptr = (v8int32*)curr_row_base;

//   v16int32 prev_buf = null_v16int32();
//   v16int32 curr_buf = null_v16int32();

//   // 预取两个相邻 8-lane block，形成 16-lane 滑窗
//   prev_buf = upd_w(prev_buf, 0, *prev_ptr++);
//   prev_buf = upd_w(prev_buf, 1, *prev_ptr);

//   curr_buf = upd_w(curr_buf, 0, *curr_ptr++);
//   curr_buf = upd_w(curr_buf, 1, *curr_ptr);

//   for (unsigned i = 0; i < COL / 8; ++i)
//     chess_prepare_for_pipelining
//     chess_loop_range(1, ) {

//       v8acc80 acc_mid   = null_v8acc80();
//       v8acc80 acc_left  = null_v8acc80();
//       v8acc80 acc_right = null_v8acc80();

//       // --------------------------------------------------
//       // partial_mid:
//       // P_i^M = 4*phi_i - phi_{i-1} - phi_{iL} - phi_{iR}
//       //
//       // offset 1 -> L
//       // offset 2 -> M
//       // offset 3 -> R
//       // --------------------------------------------------
//       acc_mid = lmul8(prev_buf, 2, 0x76543210, coeffs_rest, 0, 0x00000000);
//       acc_mid = lmac8(acc_mid, curr_buf, 1, 0x76543210, coeffs_rest, 0, 0x00000000);
//       acc_mid = lmsc8(acc_mid, curr_buf, 2, 0x76543210, coeffs,      0, 0x00000000);
//       acc_mid = lmac8(acc_mid, curr_buf, 3, 0x76543210, coeffs_rest, 0, 0x00000000);

//       // --------------------------------------------------
//       // partial_left:
//       // P_i^L = 4*phi_{iL} - phi_{(i-1)L} - phi_i - phi_{iLL}
//       //
//       // offset 0 -> LL
//       // offset 1 -> L
//       // offset 2 -> M
//       // --------------------------------------------------
//       acc_left = lmul8(prev_buf, 1, 0x76543210, coeffs_rest, 0, 0x00000000);
//       acc_left = lmac8(acc_left, curr_buf, 0, 0x76543210, coeffs_rest, 0, 0x00000000);
//       acc_left = lmsc8(acc_left, curr_buf, 1, 0x76543210, coeffs,      0, 0x00000000);
//       acc_left = lmac8(acc_left, curr_buf, 2, 0x76543210, coeffs_rest, 0, 0x00000000);

//       // --------------------------------------------------
//       // partial_right:
//       // P_i^R = 4*phi_{iR} - phi_{(i-1)R} - phi_i - phi_{iRR}
//       //
//       // offset 2 -> M
//       // offset 3 -> R
//       // offset 4 -> RR
//       // --------------------------------------------------
//       acc_right = lmul8(prev_buf, 3, 0x76543210, coeffs_rest, 0, 0x00000000);
//       acc_right = lmac8(acc_right, curr_buf, 2, 0x76543210, coeffs_rest, 0, 0x00000000);
//       acc_right = lmsc8(acc_right, curr_buf, 3, 0x76543210, coeffs,      0, 0x00000000);
//       acc_right = lmac8(acc_right, curr_buf, 4, 0x76543210, coeffs_rest, 0, 0x00000000);

//       out_mid[i]   = srs(acc_mid, 0);
//       out_left[i]  = srs(acc_left, 0);
//       out_right[i] = srs(acc_right, 0);

//       // 为下一轮迭代更新 16-lane 滑窗
//       prev_ptr = ((v8int32*)prev_row_base) + i + 1;
//       prev_buf = upd_w(prev_buf, 0, *prev_ptr++);
//       prev_buf = upd_w(prev_buf, 1, *prev_ptr);

//       curr_ptr = ((v8int32*)curr_row_base) + i + 1;
//       curr_buf = upd_w(curr_buf, 0, *curr_ptr++);
//       curr_buf = upd_w(curr_buf, 1, *curr_ptr);
//     }
// }