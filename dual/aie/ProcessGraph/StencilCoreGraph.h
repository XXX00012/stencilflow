#pragma once

#include <adf.h>
#include "../ProcessUnit/include.h"
#include "../Config.h"
#include "../ProcessUnit/hdiff.h"

using namespace adf;

class StencilCoreGraph : public graph {
public:
    static constexpr int kInputCount  = 2;
    static constexpr int kOutputCount = 1;

    port<input>  in[kInputCount];
    port<output> out;

    kernel k_lap;
    kernel k_flux;

    StencilCoreGraph(int base_col = hdiff_cfg::kTileCol,
                     int base_row = hdiff_cfg::kLapTileRow)
        : base_col_(base_col), base_row_(base_row) {
#if defined(__AIESIM__) || defined(__X86SIM__) || defined(__ADF_FRONTEND__)
        k_lap  = kernel::create(hdiff_lap);
        k_flux = kernel::create(hdiff_flux);

        source(k_lap)  = "ProcessUnit/hdiff_lap.cc";
        source(k_flux) = "ProcessUnit/hdiff_flux.cc";

        headers(k_lap)  = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
        headers(k_flux) = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};

        runtime<ratio>(k_lap)  = 0.9;
        runtime<ratio>(k_flux) = 0.9;

        location<kernel>(k_lap)  = tile(base_col_, base_row_);
        location<kernel>(k_flux) = tile(base_col_, base_row_ + 1);

        auto net_in_lap  = connect(in[0], k_lap.in[0]);
        auto net_in_flux = connect(in[1], k_flux.in[0]);

        dimensions(k_lap.in[0])  = {hdiff_cfg::kLapInputSampleElems};
        dimensions(k_flux.in[0]) = {hdiff_cfg::kFlux1RawInputSampleElems};
        fifo_depth(net_in_lap)   = 2;
        fifo_depth(net_in_flux)  = 2;

        auto net_lap_flux = connect(k_lap.out[0], k_flux.in[1]);
        dimensions(k_lap.out[0])  = {hdiff_cfg::kFluxForwardPackElems};
        dimensions(k_flux.in[1])  = {hdiff_cfg::kFluxForwardPackElems};
        fifo_depth(net_lap_flux)  = 6;

        auto net_out = connect(k_flux.out[0], out);
        dimensions(k_flux.out[0]) = {COL};
        dimensions(out)           = {COL};
        fifo_depth(net_out)       = 2;
#endif
    }

private:
    int base_col_;
    int base_row_;
};
