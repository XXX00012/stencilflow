#pragma once

#include <adf.h>
#include <cstdint>
#include "include.h"

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
               adf::output_buffer<int32_t>& out_sub_for_comsel);

void hdiff_ms(ms_complete_input_buffer& in_window,
              adf::input_buffer<int32_t>& in_sub_package,
              adf::output_buffer<int32_t>& out_ms_package);

void hdiff_comsel(adf::input_buffer<int32_t>& in_ms_package,
                  adf::input_buffer<int32_t>& in_sub_package,
                  adf::output_buffer<int32_t>& out_flux_package);

void hdiff_update(adf::input_buffer<int32_t>& in_flux_package,
                  psi_complete_input_buffer& in_psi_stream,
                  adf::output_buffer<int32_t>& out_row);

}
