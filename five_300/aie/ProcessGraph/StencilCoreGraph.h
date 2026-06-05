#pragma once

#include <adf.h>
#include "../ProcessUnit/include.h"
#include "../Config.h"
#include "../ProcessUnit/hdiff.h"

using namespace adf;

// 300-core layout in the same 50 x 6 tile region used by *_300 designs:
//   - lanes 0..49: one vertical 5-kernel lane per column, rows 1..5
//   - lanes 50..59: ten horizontal 5-kernel lanes on row 6
#define STENCIL_NUM_COLS          50
#define STENCIL_ROWS_PER_COL      6
#define STENCIL_KERNELS_PER_LANE  5
#define STENCIL_VERTICAL_LANES    STENCIL_NUM_COLS
#define STENCIL_HORIZONTAL_LANES  (STENCIL_NUM_COLS / STENCIL_KERNELS_PER_LANE)
#define STENCIL_NUM_LANES         (STENCIL_VERTICAL_LANES + STENCIL_HORIZONTAL_LANES)
#define STENCIL_INPUTS_PER_LANE   3
#define STENCIL_TOTAL_CORES       (STENCIL_NUM_LANES * STENCIL_KERNELS_PER_LANE)

class StencilCoreGraph : public graph {
public:
    port<input>  in [STENCIL_NUM_LANES * STENCIL_INPUTS_PER_LANE];
    port<output> out[STENCIL_NUM_LANES];

    kernel k_lap   [STENCIL_NUM_LANES];
    kernel k_sub   [STENCIL_NUM_LANES];
    kernel k_ms    [STENCIL_NUM_LANES];
    kernel k_comsel[STENCIL_NUM_LANES];
    kernel k_update[STENCIL_NUM_LANES];

    StencilCoreGraph() {
#if defined(__AIESIM__) || defined(__X86SIM__) || defined(__ADF_FRONTEND__)
        for (int i = 0; i < STENCIL_NUM_LANES; ++i) {
            const int in_base = i * STENCIL_INPUTS_PER_LANE;

            k_lap   [i] = kernel::create(hdiff_lap);
            k_sub   [i] = kernel::create(hdiff_sub);
            k_ms    [i] = kernel::create(hdiff_ms);
            k_comsel[i] = kernel::create(hdiff_comsel);
            k_update[i] = kernel::create(hdiff_update);

            source(k_lap   [i]) = "aie/ProcessUnit/hdiff_lap.cc";
            source(k_sub   [i]) = "aie/ProcessUnit/hdiff_sub.cc";
            source(k_ms    [i]) = "aie/ProcessUnit/hdiff_ms.cc";
            source(k_comsel[i]) = "aie/ProcessUnit/hdiff_comsel.cc";
            source(k_update[i]) = "aie/ProcessUnit/hdiff_update.cc";

            headers(k_lap   [i]) = {"aie/ProcessUnit/hdiff.h", "aie/ProcessUnit/include.h", "aie/Config.h"};
            headers(k_sub   [i]) = {"aie/ProcessUnit/hdiff.h", "aie/ProcessUnit/include.h", "aie/Config.h"};
            headers(k_ms    [i]) = {"aie/ProcessUnit/hdiff.h", "aie/ProcessUnit/include.h", "aie/Config.h"};
            headers(k_comsel[i]) = {"aie/ProcessUnit/hdiff.h", "aie/ProcessUnit/include.h", "aie/Config.h"};
            headers(k_update[i]) = {"aie/ProcessUnit/hdiff.h", "aie/ProcessUnit/include.h", "aie/Config.h"};

            runtime<ratio>(k_lap   [i]) = 0.9;
            runtime<ratio>(k_sub   [i]) = 0.9;
            runtime<ratio>(k_ms    [i]) = 0.9;
            runtime<ratio>(k_comsel[i]) = 0.9;
            runtime<ratio>(k_update[i]) = 0.9;

            place_lane(i);

            connect<>(in[in_base + 0], k_lap   [i].in[0]);
            connect<>(in[in_base + 1], k_ms    [i].in[0]);
            connect<>(in[in_base + 2], k_update[i].in[1]);

            dimensions(k_lap   [i].in[0]) = {COL};
            dimensions(k_ms    [i].in[0]) = {COL};
            dimensions(k_update[i].in[1]) = {COL};

            auto net_lap_sub = connect(k_lap[i].out[0], k_sub[i].in[0]);
            dimensions(k_lap[i].out[0]) = {hdiff_cfg::kLapReusePackElems};
            dimensions(k_sub[i].in[0])  = {hdiff_cfg::kLapReusePackElems};
            fifo_depth(net_lap_sub) = 2;

            auto net_sub_ms = connect(k_sub[i].out[0], k_ms[i].in[1]);
            dimensions(k_sub[i].out[0]) = {hdiff_cfg::kSubCompletePackElems};
            dimensions(k_ms [i].in[1])  = {hdiff_cfg::kSubCompletePackElems};
            fifo_depth(net_sub_ms) = 2;

            auto net_sub_comsel = connect(k_sub[i].out[1], k_comsel[i].in[1]);
            dimensions(k_sub   [i].out[1]) = {hdiff_cfg::kSubCompletePackElems};
            dimensions(k_comsel[i].in[1])  = {hdiff_cfg::kSubCompletePackElems};
            fifo_depth(net_sub_comsel) = 4;

            auto net_ms_comsel = connect(k_ms[i].out[0], k_comsel[i].in[0]);
            dimensions(k_ms    [i].out[0]) = {hdiff_cfg::kMsCompletePackElems};
            dimensions(k_comsel[i].in[0])  = {hdiff_cfg::kMsCompletePackElems};
            fifo_depth(net_ms_comsel) = 2;

            auto net_comsel_update = connect(k_comsel[i].out[0], k_update[i].in[0]);
            dimensions(k_comsel[i].out[0]) = {hdiff_cfg::kSelCompletePackElems};
            dimensions(k_update[i].in[0])  = {hdiff_cfg::kSelCompletePackElems};
            fifo_depth(net_comsel_update) = 2;

            auto net_out = connect(k_update[i].out[0], out[i]);
            dimensions(k_update[i].out[0]) = {COL};
            dimensions(out[i])             = {COL};
            fifo_depth(net_out) = 2;
        }
#endif
    }

private:
    void place_lane(int i) {
        if (i < STENCIL_VERTICAL_LANES) {
            const int col = i;

            location<kernel>(k_lap   [i]) = tile(col, 1);
            location<kernel>(k_sub   [i]) = tile(col, 2);
            location<kernel>(k_ms    [i]) = tile(col, 3);
            location<kernel>(k_comsel[i]) = tile(col, 4);
            location<kernel>(k_update[i]) = tile(col, 5);
        } else {
            const int slot = i - STENCIL_VERTICAL_LANES;
            const int col = slot * STENCIL_KERNELS_PER_LANE;

            location<kernel>(k_lap   [i]) = tile(col + 0, 6);
            location<kernel>(k_sub   [i]) = tile(col + 1, 6);
            location<kernel>(k_ms    [i]) = tile(col + 2, 6);
            location<kernel>(k_comsel[i]) = tile(col + 3, 6);
            location<kernel>(k_update[i]) = tile(col + 4, 6);
        }
    }
};
