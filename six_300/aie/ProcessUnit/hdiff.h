#pragma once
#include <adf.h>
#include <cstdint>
#include "Config.h"

using namespace adf;
using namespace hdiff_cfg;

using lap2_input_buffer = input_buffer<
    int32_t,
    adf::extents<adf::inherited_extent>,
    adf::margin<256>>;



using ms_input_buffer = input_buffer<
    int32_t,
    adf::extents<adf::inherited_extent>,
    adf::margin<256>>;

using sub_input_buffer = input_buffer<
    int32_t,
    adf::extents<adf::inherited_extent>,
    adf::margin<256>>;

using update_input_buffer = input_buffer<
        int32_t,
        adf::extents<adf::inherited_extent>,
        adf::margin<3*COL>>;





void hdiff_lap2(
    lap2_input_buffer& in_window,
    output_buffer<int32_t>& out_package
);

void hdiff_sub(
    sub_input_buffer& in_lap_package,
    output_buffer<int32_t>& out_package
);

void hdiff_ms(
    ms_input_buffer& in_window,
    input_buffer<int32_t>& in_sub_package,
    output_buffer<int32_t>& out_package
);

void hdiff_com(
    input_buffer<int32_t>& in_ms_package,
    output_buffer<int32_t>& out_mask_package
);

void hdiff_sel(
    input_buffer<int32_t>& in_mask_package,
    input_buffer<int32_t>& in_sub_package,
    output_buffer<int32_t>& out_package
);

void hdiff_update(
    update_input_buffer& in_comsel_package,
    input_buffer<int32_t>& in_psi_row,
    output_buffer<int32_t>& out_row);

