#include <adf.h>
#include <aie_api/aie.hpp>
#include <cstdint>
#include "include.h"
#include "hdiff.h"

using namespace adf;

#define kernel_load 14

namespace {

inline v8int32 select_flux(v8int32 product, v8int32 diff) {
  unsigned int take_zero_mask =
      gt16(concat(product, undef_v8int32()), 0,
           0x76543210, 0xFEDCBA98,
           null_v16int32(), 0,
           0x76543210, 0xFEDCBA98);

  v16int32 selected =
      select16(take_zero_mask,
               concat(diff, undef_v8int32()),
               null_v16int32());
  return ext_w(selected, 0);
}

} // namespace

void hdiff_comsel(input_buffer<int32_t>& in_ms_package,
                  input_buffer<int32_t>& in_sub_package,
                  output_buffer<int32_t>& out_flux_package) {
  // Fused complete-window com + sel.
  //
  // Product layout:
  //   [ms_left | ms_right | ms_up | ms_down]
  // Sub layout:
  //   [sub_left | sub_right | sub_up | sub_down]
  // Output flux layout:
  //   [flux_left | flux_right | flux_up | flux_down]

  int32_t* __restrict ms_base  = in_ms_package.data();
  int32_t* __restrict sub_base = in_sub_package.data();
  int32_t* __restrict out_base = out_flux_package.data();

  v8int32* __restrict ms_left  = (v8int32*)(ms_base + (0 * COL));
  v8int32* __restrict ms_right = (v8int32*)(ms_base + (1 * COL));
  v8int32* __restrict ms_up    = (v8int32*)(ms_base + (2 * COL));
  v8int32* __restrict ms_down  = (v8int32*)(ms_base + (3 * COL));

  v8int32* __restrict sub_left  = (v8int32*)(sub_base + (0 * COL));
  v8int32* __restrict sub_right = (v8int32*)(sub_base + (1 * COL));
  v8int32* __restrict sub_up    = (v8int32*)(sub_base + (2 * COL));
  v8int32* __restrict sub_down  = (v8int32*)(sub_base + (3 * COL));

  v8int32* __restrict out_left  = (v8int32*)(out_base + (0 * COL));
  v8int32* __restrict out_right = (v8int32*)(out_base + (1 * COL));
  v8int32* __restrict out_up    = (v8int32*)(out_base + (2 * COL));
  v8int32* __restrict out_down  = (v8int32*)(out_base + (3 * COL));

  for (unsigned i = 0; i < COL / 8; ++i)
    chess_prepare_for_pipelining
    chess_loop_range(1, ) {
      out_left[i]  = select_flux(ms_left[i],  sub_left[i]);
      out_right[i] = select_flux(ms_right[i], sub_right[i]);
      out_up[i]    = select_flux(ms_up[i],    sub_up[i]);
      out_down[i]  = select_flux(ms_down[i],  sub_down[i]);
    }
}
