#pragma once

#include <adf.h>
#include <cstdint>
#include "include.h"

// six_complete uses the same row-stream style as your partial six-core graph:
// each firing consumes one new row (COL int32 elements), and the complete
// 5-row stencil window is reconstructed by buffer margins.
//
// When the 5th row has arrived, the complete chain computes one full hdiff
// output for the center row in this window. lap/sub use a one-row package
// margin so the previous package's trailing lap_mid becomes the current lap_up.

using lap_complete_input_buffer = adf::input_buffer<
    int32_t,
    adf::extents<adf::inherited_extent>,
    adf::margin<4 * COL>>;

using sub_lap_input_buffer = adf::input_buffer<
    int32_t,
    adf::extents<adf::inherited_extent>,
    adf::margin<COL>>;

using ms_complete_input_buffer = adf::input_buffer<
    int32_t,
    adf::extents<adf::inherited_extent>,
    adf::margin<4 * COL>>;

using psi_complete_input_buffer = adf::input_buffer<
    int32_t,
    adf::extents<adf::inherited_extent>,
    adf::margin<2 * COL>>;

extern "C" {

void hdiff_lap(lap_complete_input_buffer& in_window,
               adf::output_buffer<int32_t>& out_lap_package);

void hdiff_sub(sub_lap_input_buffer& in_lap_package,
               adf::output_buffer<int32_t>& out_sub_for_ms,
               adf::output_buffer<int32_t>& out_sub_for_sel);

void hdiff_ms(ms_complete_input_buffer& in_window,
              adf::input_buffer<int32_t>& in_sub_package,
              adf::output_buffer<int32_t>& out_ms_package);

void hdiff_com(adf::input_buffer<int32_t>& in_ms_package,
               adf::output_buffer<int32_t>& out_mask_package);

void hdiff_sel(adf::input_buffer<int32_t>& in_mask_package,
               adf::input_buffer<int32_t>& in_sub_package,
               adf::output_buffer<int32_t>& out_flux_package);

void hdiff_update(adf::input_buffer<int32_t>& in_flux_package,
                  psi_complete_input_buffer& in_psi_stream,
                  adf::output_buffer<int32_t>& out_row);

}
