#pragma once
#include <adf.h>
#include <cstdint>
#include "Config.h"

using namespace adf;
using namespace hdiff_cfg;

using hdiff_input_buffer = input_buffer<
    int32_t,
    adf::extents<adf::inherited_extent>,
    adf::margin<4*256>>;



void vec_hdiff(hdiff_input_buffer&  in_window,
               output_buffer<int32_t>& out);

