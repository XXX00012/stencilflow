#pragma once

#include <adf.h>
#include <cstdint>
#include "include.h"

using lap_complete_input_buffer = adf::input_buffer<
    int32_t,
    adf::extents<adf::inherited_extent>,
    adf::margin<4 * COL>>;

using lap_package_input_buffer = adf::input_buffer<
    int32_t,
    adf::extents<adf::inherited_extent>,
    adf::margin<COL>>;

using psi_complete_input_buffer = adf::input_buffer<
    int32_t,
    adf::extents<adf::inherited_extent>,
    adf::margin<4 * COL>>;

using psi_center_input_buffer = adf::input_buffer<
    int32_t,
    adf::extents<adf::inherited_extent>,
    adf::margin<2 * COL>>;

extern "C" {

void hdiff_lap(lap_complete_input_buffer& in_window,
               adf::output_buffer<int32_t>& lap_to_x,
               adf::output_buffer<int32_t>& lap_to_y);

void hdiff_flux_x(lap_package_input_buffer& in_lap_package,
                  psi_complete_input_buffer& in_psi_stream,
                  adf::output_buffer<int32_t>& out_dx);

void hdiff_flux_y(lap_package_input_buffer& in_lap_package,
                  psi_complete_input_buffer& in_psi_stream,
                  adf::output_buffer<int32_t>& out_dy);

void hdiff_update(adf::input_buffer<int32_t>& in_dx,
                  adf::input_buffer<int32_t>& in_dy,
                  psi_center_input_buffer& in_psi_stream,
                  adf::output_buffer<int32_t>& out_row);

}
