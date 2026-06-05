#pragma once

#include <adf.h>
#include "../ProcessUnit/include.h"
#include "../Config.h"
#include "../ProcessUnit/hdiff.h"

using namespace adf;

// Grid: 50 cols x 6 rows = 300 AIE tiles.
// six: 6 rows/unit -> 1 unit/col, 50 units total.
#define STENCIL_NUM_COLS        50
#define STENCIL_ROWS_PER_COL    6
#define STENCIL_ROWS_PER_UNIT   6
#define STENCIL_UNITS_PER_COL   (STENCIL_ROWS_PER_COL / STENCIL_ROWS_PER_UNIT)
#define STENCIL_NUM_UNITS       (STENCIL_NUM_COLS * STENCIL_UNITS_PER_COL)
#define STENCIL_INPUTS_PER_UNIT 3
#define STENCIL_TOTAL_CORES     (STENCIL_NUM_UNITS * STENCIL_ROWS_PER_UNIT)

class StencilCoreGraph : public graph {
public:
    port<input>  in [STENCIL_NUM_UNITS * STENCIL_INPUTS_PER_UNIT];
    port<output> out[STENCIL_NUM_UNITS];

    kernel k_lap2  [STENCIL_NUM_UNITS];
    kernel k_sub   [STENCIL_NUM_UNITS];
    kernel k_ms    [STENCIL_NUM_UNITS];
    kernel k_com   [STENCIL_NUM_UNITS];
    kernel k_sel   [STENCIL_NUM_UNITS];
    kernel k_update[STENCIL_NUM_UNITS];

    StencilCoreGraph() {
#if defined(__AIESIM__) || defined(__X86SIM__) || defined(__ADF_FRONTEND__)
        for (int i = 0; i < STENCIL_NUM_UNITS; ++i) {
            const int col = i / STENCIL_UNITS_PER_COL;
            const int row = 1 + (i % STENCIL_UNITS_PER_COL) * STENCIL_ROWS_PER_UNIT;
            const int in_base = i * STENCIL_INPUTS_PER_UNIT;

            k_lap2  [i] = kernel::create(hdiff_lap2);
            k_sub   [i] = kernel::create(hdiff_sub);
            k_ms    [i] = kernel::create(hdiff_ms);
            k_com   [i] = kernel::create(hdiff_com);
            k_sel   [i] = kernel::create(hdiff_sel);
            k_update[i] = kernel::create(hdiff_update);

            source(k_lap2  [i]) = "ProcessUnit/hdiff_lap2.cc";
            source(k_sub   [i]) = "ProcessUnit/hdiff_sub.cc";
            source(k_ms    [i]) = "ProcessUnit/hdiff_ms.cc";
            source(k_com   [i]) = "ProcessUnit/hdiff_com.cc";
            source(k_sel   [i]) = "ProcessUnit/hdiff_sel.cc";
            source(k_update[i]) = "ProcessUnit/hdiff_update.cc";

            headers(k_lap2  [i]) = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
            headers(k_sub   [i]) = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
            headers(k_ms    [i]) = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
            headers(k_com   [i]) = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
            headers(k_sel   [i]) = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
            headers(k_update[i]) = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};

            runtime<ratio>(k_lap2  [i]) = 0.9;
            runtime<ratio>(k_sub   [i]) = 0.9;
            runtime<ratio>(k_ms    [i]) = 0.9;
            runtime<ratio>(k_com   [i]) = 0.9;
            runtime<ratio>(k_sel   [i]) = 0.9;
            runtime<ratio>(k_update[i]) = 0.9;

            location<kernel>(k_lap2  [i]) = tile(col, row);
            location<kernel>(k_sub   [i]) = tile(col, row + 1);
            location<kernel>(k_ms    [i]) = tile(col, row + 2);
            location<kernel>(k_com   [i]) = tile(col, row + 3);
            location<kernel>(k_sel   [i]) = tile(col, row + 4);
            location<kernel>(k_update[i]) = tile(col, row + 5);

            auto net_in_lap2   = connect(in[in_base + 0], k_lap2  [i].in[0]);
            auto net_in_ms     = connect(in[in_base + 1], k_ms    [i].in[0]);
            auto net_in_update = connect(in[in_base + 2], k_update[i].in[1]);

            dimensions(k_lap2  [i].in[0]) = {COL};
            dimensions(k_ms    [i].in[0]) = {COL};
            dimensions(k_update[i].in[1]) = {COL};

            fifo_depth(net_in_lap2)   = 2;
            fifo_depth(net_in_ms)     = 2;
            fifo_depth(net_in_update) = 2;

            auto net_lap2_sub = connect(k_lap2[i].out[0], k_sub[i].in[0]);
            dimensions(k_lap2[i].out[0]) = {3 * COL};
            dimensions(k_sub [i].in[0])  = {3 * COL};
            fifo_depth(net_lap2_sub) = 2;

            auto net_sub_ms = connect(k_sub[i].out[0], k_ms[i].in[1]);
            dimensions(k_sub[i].out[0]) = {4 * COL};
            dimensions(k_ms [i].in[1])  = {4 * COL};
            fifo_depth(net_sub_ms) = 2;

            auto net_sub_sel = connect(k_sub[i].out[0], k_sel[i].in[1]);
            dimensions(k_sel[i].in[1]) = {4 * COL};
            fifo_depth(net_sub_sel) = 2;

            auto net_ms_com = connect(k_ms[i].out[0], k_com[i].in[0]);
            dimensions(k_ms [i].out[0]) = {3 * COL};
            dimensions(k_com[i].in[0])  = {3 * COL};
            fifo_depth(net_ms_com) = 2;

            auto net_com_sel = connect(k_com[i].out[0], k_sel[i].in[0]);
            dimensions(k_com[i].out[0]) = {3 * (COL / 8)};
            dimensions(k_sel[i].in[0])  = {3 * (COL / 8)};
            fifo_depth(net_com_sel) = 2;

            auto net_sel_update = connect(k_sel[i].out[0], k_update[i].in[0]);
            dimensions(k_sel   [i].out[0]) = {4 * COL};
            dimensions(k_update[i].in[0])  = {4 * COL};
            fifo_depth(net_sel_update) = 2;

            auto net_out = connect(k_update[i].out[0], out[i]);
            dimensions(k_update[i].out[0]) = {COL};
            dimensions(out[i])             = {COL};
            fifo_depth(net_out) = 2;
        }
#endif
    }
};
