#pragma once

#include <adf.h>
#include "../ProcessUnit/include.h"
#include "../Config.h"
#include "../ProcessUnit/hdiff.h"

using namespace adf;

// Grid: 50 cols x 6 rows = 300 AIE tiles
// dual: 2 rows/unit -> 3 units/col, 150 units total
#define STENCIL_NUM_COLS      50
#define STENCIL_ROWS_PER_COL  6
#define STENCIL_ROWS_PER_UNIT 2
#define STENCIL_UNITS_PER_COL (STENCIL_ROWS_PER_COL / STENCIL_ROWS_PER_UNIT)  // 3
#define STENCIL_NUM_UNITS     (STENCIL_NUM_COLS * STENCIL_UNITS_PER_COL)      // 150
#define STENCIL_INPUTS_PER_UNIT 2
#define STENCIL_TOTAL_CORES   (STENCIL_NUM_UNITS * STENCIL_ROWS_PER_UNIT)     // 300

class StencilCoreGraph : public graph {
public:
    port<input>  in [STENCIL_NUM_UNITS * STENCIL_INPUTS_PER_UNIT];
    port<output> out[STENCIL_NUM_UNITS];

    kernel k_lap [STENCIL_NUM_UNITS];
    kernel k_flux[STENCIL_NUM_UNITS];

    StencilCoreGraph() {
#if defined(__AIESIM__) || defined(__X86SIM__) || defined(__ADF_FRONTEND__)
        for (int i = 0; i < STENCIL_NUM_UNITS; ++i) {
            const int col = 0 + (i / STENCIL_UNITS_PER_COL);                     // 0..49
            const int row = 1 + (i % STENCIL_UNITS_PER_COL) * STENCIL_ROWS_PER_UNIT; // 1..6
            const int in_base = i * STENCIL_INPUTS_PER_UNIT;

            k_lap [i] = kernel::create(hdiff_lap);
            k_flux[i] = kernel::create(hdiff_flux);

            source(k_lap [i]) = "ProcessUnit/hdiff_lap.cc";
            source(k_flux[i]) = "ProcessUnit/hdiff_flux.cc";

            headers(k_lap [i]) = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
            headers(k_flux[i]) = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};

            runtime<ratio>(k_lap [i]) = 0.9;
            runtime<ratio>(k_flux[i]) = 0.9;

            location<kernel>(k_lap [i]) = tile(col, row);
            location<kernel>(k_flux[i]) = tile(col, row + 1);

            auto net_in_lap  = connect(in[in_base + 0], k_lap [i].in[0]);
            auto net_in_flux = connect(in[in_base + 1], k_flux[i].in[0]);

            dimensions(k_lap [i].in[0]) = {hdiff_cfg::kLapInputSampleElems};
            dimensions(k_flux[i].in[0]) = {hdiff_cfg::kFlux1RawInputSampleElems};

            fifo_depth(net_in_lap)  = 2;
            fifo_depth(net_in_flux) = 2;

            auto net_lap_flux = connect(k_lap[i].out[0], k_flux[i].in[1]);
            dimensions(k_lap [i].out[0]) = {hdiff_cfg::kFluxForwardPackElems};
            dimensions(k_flux[i].in[1])  = {hdiff_cfg::kFluxForwardPackElems};
            fifo_depth(net_lap_flux) = 6;

            auto net_out = connect(k_flux[i].out[0], out[i]);
            dimensions(k_flux[i].out[0]) = {COL};
            dimensions(out[i])            = {COL};
            fifo_depth(net_out) = 2;
        }
#endif
    }
};
