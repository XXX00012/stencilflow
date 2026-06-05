#pragma once

#include <adf.h>
#include "../ProcessUnit/include.h"
#include "../Config.h"
#include "../ProcessUnit/hdiff.h"

using namespace adf;

class StencilCoreGraph : public graph {
public:
    port<input>  in[4];
    port<output> out;

    kernel k_lap;
    kernel k_flux_x;
    kernel k_flux_y;
    kernel k_update;

    StencilCoreGraph() {
#if defined(__AIESIM__) || defined(__X86SIM__) || defined(__ADF_FRONTEND__)
        k_lap    = kernel::create(hdiff_lap);
        k_flux_x = kernel::create(hdiff_flux_x);
        k_flux_y = kernel::create(hdiff_flux_y);
        k_update = kernel::create(hdiff_update);

        source(k_lap)    = "ProcessUnit/hdiff_lap.cc";
        source(k_flux_x) = "ProcessUnit/hdiff_flux_x.cc";
        source(k_flux_y) = "ProcessUnit/hdiff_flux_y.cc";
        source(k_update) = "ProcessUnit/hdiff_update.cc";

        headers(k_lap)    = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
        headers(k_flux_x) = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
        headers(k_flux_y) = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
        headers(k_update) = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};

        runtime<ratio>(k_lap)    = 0.9;
        runtime<ratio>(k_flux_x) = 0.9;
        runtime<ratio>(k_flux_y) = 0.9;
        runtime<ratio>(k_update) = 0.9;

        location<kernel>(k_lap)    = tile(7, 1);
        location<kernel>(k_flux_x) = tile(7, 2);
        location<kernel>(k_flux_y) = tile(7, 3);
        location<kernel>(k_update) = tile(7, 4);

        auto net_in_lap    = connect(in[0], k_lap.in[0]);
        auto net_in_fx_psi = connect(in[1], k_flux_x.in[1]);
        auto net_in_fy_psi = connect(in[2], k_flux_y.in[1]);
        auto net_in_update = connect(in[3], k_update.in[2]);

        dimensions(k_lap.in[0])    = {COL};
        dimensions(k_flux_x.in[1]) = {COL};
        dimensions(k_flux_y.in[1]) = {COL};
        dimensions(k_update.in[2]) = {COL};
        fifo_depth(net_in_lap)      = 4;
        fifo_depth(net_in_fx_psi)   = 4;
        fifo_depth(net_in_fy_psi)   = 4;
        fifo_depth(net_in_update)   = 4;

        auto net_lap_fx = connect(k_lap.out[0], k_flux_x.in[0]);
        dimensions(k_lap.out[0])    = {hdiff_cfg::kLapReusePackElems};
        dimensions(k_flux_x.in[0])  = {hdiff_cfg::kLapReusePackElems};
        fifo_depth(net_lap_fx)      = 2;

        auto net_lap_fy = connect(k_lap.out[1], k_flux_y.in[0]);
        dimensions(k_lap.out[1])    = {hdiff_cfg::kLapReusePackElems};
        dimensions(k_flux_y.in[0])  = {hdiff_cfg::kLapReusePackElems};
        fifo_depth(net_lap_fy)      = 2;

        auto net_fx_update = connect(k_flux_x.out[0], k_update.in[0]);
        dimensions(k_flux_x.out[0]) = {hdiff_cfg::kFluxDivergenceElems};
        dimensions(k_update.in[0])  = {hdiff_cfg::kFluxDivergenceElems};
        fifo_depth(net_fx_update)   = 2;

        auto net_fy_update = connect(k_flux_y.out[0], k_update.in[1]);
        dimensions(k_flux_y.out[0]) = {hdiff_cfg::kFluxDivergenceElems};
        dimensions(k_update.in[1])  = {hdiff_cfg::kFluxDivergenceElems};
        fifo_depth(net_fy_update)   = 2;

        auto net_out = connect(k_update.out[0], out);
        dimensions(k_update.out[0]) = {COL};
        dimensions(out)             = {COL};
        fifo_depth(net_out)         = 2;
#endif
    }
};
