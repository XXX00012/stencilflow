#pragma once

#include <adf.h>
#include <cstdint>
#include "Config.h"

using namespace adf;
using namespace hdiff_cfg;

using lap_input_buffer_t = input_buffer<
    int32_t,
    adf::extents<adf::inherited_extent>,
    adf::margin<kLapInputMarginElems>>;

using flux_raw_input_buffer_t = input_buffer<
    int32_t,
    adf::extents<adf::inherited_extent>,
    adf::margin<kFlux1RawInputMarginElems>>;

using lap_window_buf_t = lap_input_buffer_t;
using flux_window_buf_t = flux_raw_input_buffer_t;



void hdiff_lap(
    lap_input_buffer_t& in_window,
    output_buffer<int32_t>& flux_forward_pack);

void hdiff_flux(
    flux_raw_input_buffer_t& in_window,
    input_buffer<int32_t>& flux_forward_pack,
    output_buffer<int32_t>& out);


