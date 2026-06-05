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

extern "C" {

void hdiff_lap(
    lap_input_buffer& in_window,
    output_buffer<int32_t>& out_flux1,
    output_buffer<int32_t>& out_flux2,
    output_buffer<int32_t>& out_flux3,
    output_buffer<int32_t>& out_flux4);

void hdiff_flux1(
    flux1_input_buffer& in_window,
    input_buffer<int32_t>& flux_forward1,
    input_buffer<int32_t>& flux_forward2,
    input_buffer<int32_t>& flux_forward3,
    input_buffer<int32_t>& flux_forward4,
    output_buffer<int32_t>& flux_inter1,
    output_buffer<int32_t>& flux_inter2,
    output_buffer<int32_t>& flux_inter3,
    output_buffer<int32_t>& flux_inter4,
    output_buffer<int32_t>& flux_inter5);

void hdiff_flux2(
    input_buffer<int32_t>& flux_inter1,
    input_buffer<int32_t>& flux_inter2,
    input_buffer<int32_t>& flux_inter3,
    input_buffer<int32_t>& flux_inter4,
    input_buffer<int32_t>& flux_inter5,
    output_buffer<int32_t>& out);

} 
