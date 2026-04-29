#pragma once

#include <adf.h>
#include <cstdint>
#include "Config.h"
using namespace adf;
using namespace hdiff_cfg

void hdiff_lap(input_buffer<int32_t>&  in_window,  
               output_buffer<int32_t>& flux_forward_pack);

void hdiff_flux1(input_buffer<int32_t>& in_window,
                 input_buffer<int32_t>& flux_forward_pack,
                 output_buffer<int32_t>& flux_inter_pack);

void hdiff_flux2(adf::input_buffer<int32_t>& flux_inter_pack,
                 adf::output_buffer<int32_t>& out);

void hdiff_flux2_gather(adf::input_buffer<int32_t>& flux_inter_pack,
                        adf::input_buffer<int32_t>& row_from_lane0,
                        adf::input_buffer<int32_t>& row_from_lane2,
                        adf::input_buffer<int32_t>& row_from_lane3,
                        adf::output_buffer<int32_t>& block_out);