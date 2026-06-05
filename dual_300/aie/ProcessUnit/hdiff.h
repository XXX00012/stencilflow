#pragma once

#include <adf.h>
#include <cstdint>
#include "Config.h"

using namespace adf;

using lap_input_buffer = input_buffer<
    int32_t,
    adf::extents<adf::inherited_extent>,
    adf::margin<hdiff_cfg::kLapInputMarginElems>>;

using flux_input_buffer = input_buffer<
    int32_t,
    adf::extents<adf::inherited_extent>,
    adf::margin<hdiff_cfg::kFlux1RawInputMarginElems>>;

extern "C" {

void hdiff_lap(
    lap_input_buffer& in_window,
    output_buffer<int32_t>& flux_forward_pack);

void hdiff_flux(
    flux_input_buffer& in_window,
    input_buffer<int32_t>& flux_forward_pack,
    output_buffer<int32_t>& out);

}
