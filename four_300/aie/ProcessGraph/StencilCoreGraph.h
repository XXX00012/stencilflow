#pragma once

#include <adf.h>
#include "../ProcessUnit/include.h"
#include "../Config.h"
#include "../ProcessUnit/hdiff.h"

using namespace adf;

// 296-core layout in the same 50 x 6 tile region used by *_300 designs:
//   - lanes 0..49: one vertical 4-kernel lane per column, rows 1..4
//   - lanes 50..73: twenty-four horizontal 4-kernel lanes, rows 5..6
//     row 5: lanes 50..61 use cols 0..47
//     row 6: lanes 62..73 use cols 0..47
#define STENCIL_NUM_COLS                 50
#define STENCIL_ROWS_PER_COL             6
#define STENCIL_KERNELS_PER_LANE         4
#define STENCIL_VERTICAL_LANES           STENCIL_NUM_COLS
#define STENCIL_HORIZONTAL_LANES_PER_ROW 12
#define STENCIL_HORIZONTAL_ROWS          2
#define STENCIL_HORIZONTAL_LANES         (STENCIL_HORIZONTAL_LANES_PER_ROW * STENCIL_HORIZONTAL_ROWS)
#define STENCIL_NUM_LANES                (STENCIL_VERTICAL_LANES + STENCIL_HORIZONTAL_LANES)
#define STENCIL_INPUTS_PER_LANE          3
#define STENCIL_TOTAL_CORES              (STENCIL_NUM_LANES * STENCIL_KERNELS_PER_LANE)

class StencilCoreGraph : public graph {
public:
    port<input>  in [STENCIL_NUM_LANES * STENCIL_INPUTS_PER_LANE];
    port<output> out[STENCIL_NUM_LANES];

    kernel k_lap      [STENCIL_NUM_LANES];
    kernel k_sub      [STENCIL_NUM_LANES];
    kernel k_mscom    [STENCIL_NUM_LANES];
    kernel k_selupdate[STENCIL_NUM_LANES];

    StencilCoreGraph() {
#if defined(__AIESIM__) || defined(__X86SIM__) || defined(__ADF_FRONTEND__)
        for (int i = 0; i < STENCIL_NUM_LANES; ++i) {
            const int in_base = i * STENCIL_INPUTS_PER_LANE;

            k_lap      [i] = kernel::create(hdiff_lap);
            k_sub      [i] = kernel::create(hdiff_sub);
            k_mscom    [i] = kernel::create(hdiff_mscom);
            k_selupdate[i] = kernel::create(hdiff_selupdate);

            source(k_lap      [i]) = "ProcessUnit/hdiff_lap.cc";
            source(k_sub      [i]) = "ProcessUnit/hdiff_sub.cc";
            source(k_mscom    [i]) = "ProcessUnit/hdiff_mscom.cc";
            source(k_selupdate[i]) = "ProcessUnit/hdiff_selupdate.cc";

            headers(k_lap      [i]) = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
            headers(k_sub      [i]) = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
            headers(k_mscom    [i]) = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
            headers(k_selupdate[i]) = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};

            runtime<ratio>(k_lap      [i]) = 0.9;
            runtime<ratio>(k_sub      [i]) = 0.9;
            runtime<ratio>(k_mscom    [i]) = 0.9;
            runtime<ratio>(k_selupdate[i]) = 0.9;

            place_lane(i);

            auto net_in_lap       = connect(in[in_base + 0], k_lap      [i].in[0]);
            auto net_in_mscom     = connect(in[in_base + 1], k_mscom    [i].in[0]);
            auto net_in_selupdate = connect(in[in_base + 2], k_selupdate[i].in[2]);

            dimensions(k_lap      [i].in[0]) = {COL};
            dimensions(k_mscom    [i].in[0]) = {COL};
            dimensions(k_selupdate[i].in[2]) = {COL};
            fifo_depth(net_in_lap)            = 4;
            fifo_depth(net_in_mscom)          = 4;
            fifo_depth(net_in_selupdate)      = 4;

            auto net_lap_sub = connect(k_lap[i].out[0], k_sub[i].in[0]);
            dimensions(k_lap[i].out[0]) = {hdiff_cfg::kLapReusePackElems};
            dimensions(k_sub[i].in[0])  = {hdiff_cfg::kLapReusePackElems};
            fifo_depth(net_lap_sub)     = 2;

            auto net_sub_mscom = connect(k_sub[i].out[0], k_mscom[i].in[1]);
            dimensions(k_sub  [i].out[0]) = {hdiff_cfg::kSubCompletePackElems};
            dimensions(k_mscom[i].in[1])  = {hdiff_cfg::kSubCompletePackElems};
            fifo_depth(net_sub_mscom)      = 2;

            auto net_sub_selupdate = connect(k_sub[i].out[1], k_selupdate[i].in[1]);
            dimensions(k_sub      [i].out[1]) = {hdiff_cfg::kSubCompletePackElems};
            dimensions(k_selupdate[i].in[1])  = {hdiff_cfg::kSubCompletePackElems};
            fifo_depth(net_sub_selupdate)      = 4;

            auto net_mscom_selupdate = connect(k_mscom[i].out[0], k_selupdate[i].in[0]);
            dimensions(k_mscom    [i].out[0]) = {hdiff_cfg::kMaskCompletePackElems};
            dimensions(k_selupdate[i].in[0])  = {hdiff_cfg::kMaskCompletePackElems};
            fifo_depth(net_mscom_selupdate)    = 2;

            auto net_out = connect(k_selupdate[i].out[0], out[i]);
            dimensions(k_selupdate[i].out[0]) = {COL};
            dimensions(out[i])                = {COL};
            fifo_depth(net_out)               = 2;
        }
#endif
    }

private:
    void place_lane(int i) {
        if (i < STENCIL_VERTICAL_LANES) {
            const int col = i;

            location<kernel>(k_lap      [i]) = tile(col, 1);
            location<kernel>(k_sub      [i]) = tile(col, 2);
            location<kernel>(k_mscom    [i]) = tile(col, 3);
            location<kernel>(k_selupdate[i]) = tile(col, 4);
        } else {
            const int horizontal_id = i - STENCIL_VERTICAL_LANES;
            const int row = 5 + (horizontal_id / STENCIL_HORIZONTAL_LANES_PER_ROW);
            const int slot = horizontal_id % STENCIL_HORIZONTAL_LANES_PER_ROW;
            const int col = slot * STENCIL_KERNELS_PER_LANE;

            location<kernel>(k_lap      [i]) = tile(col + 0, row);
            location<kernel>(k_sub      [i]) = tile(col + 1, row);
            location<kernel>(k_mscom    [i]) = tile(col + 2, row);
            location<kernel>(k_selupdate[i]) = tile(col + 3, row);
        }
    }
};
