#pragma once

#include <adf.h>
#include <cstdint>
#include "include.h"

// four_complete uses the same row-stream style as the six-core complete graph:
// each firing consumes one new row (COL int32 elements), and the complete
// 5-row stencil window is reconstructed by buffer margins.
//
// The graph fuses ms+com and sel+update. The fused kernels still read the raw
// psi stream from external inputs, matching six_complete's dataflow.

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
               adf::output_buffer<int32_t>& out_sub_for_mscom,
               adf::output_buffer<int32_t>& out_sub_for_selupdate);

void hdiff_mscom(ms_complete_input_buffer& in_window,
                 adf::input_buffer<int32_t>& in_sub_package,
                 adf::output_buffer<int32_t>& out_mask_package);

void hdiff_selupdate(adf::input_buffer<int32_t>& in_mask_package,
                     adf::input_buffer<int32_t>& in_sub_package,
                     psi_complete_input_buffer& in_psi_stream,
                     adf::output_buffer<int32_t>& out_row);

}
