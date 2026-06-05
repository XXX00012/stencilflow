#pragma once

#include <adf.h>
#include <cstdint>
#include "Config.h"

using namespace adf;

using lap_input_buffer = input_buffer<
    int32_t,
    adf::extents<adf::inherited_extent>,
    adf::margin<hdiff_cfg::kLapInputMarginElems>>;

using flux1_input_buffer = input_buffer<
    int32_t,
    adf::extents<adf::inherited_extent>,
    adf::margin<hdiff_cfg::kFlux1RawInputMarginElems>>;

using flux2_input_buffer = input_buffer<
    int32_t,
    adf::extents<adf::inherited_extent>,
    adf::margin<hdiff_cfg::kFlux2RawInputMarginElems>>;

extern "C" {

void hdiff_lap(
    lap_input_buffer& in_window,
    output_buffer<int32_t>& sub_pack_for_ms,
    output_buffer<int32_t>& sub_pack_for_sel);

void hdiff_flux1(
    flux1_input_buffer& in_window,
    input_buffer<int32_t>& sub_pack,
    output_buffer<int32_t>& mask_pack);

void hdiff_flux2(
    input_buffer<int32_t>& mask_pack,
    flux2_input_buffer& in_window,
    input_buffer<int32_t>& sub_pack,
    output_buffer<int32_t>& out);

} 
