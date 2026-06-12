#pragma once

#include <adf.h>
#include "../ProcessUnit/include.h"
#include "../Config.h"
#include "../ProcessUnit/hdiff.h"

using namespace adf;

#define STENCIL_MAIN_COLS 50
#define STENCIL_MAIN_LANES 100
#define STENCIL_TOP_ROWS 2
#define STENCIL_TOP_LANES_PER_ROW 14
#define STENCIL_TOP_LANES 28
#define STENCIL_ROWS_PER_UNIT 3
#define STENCIL_UNITS_PER_COL 2
#define STENCIL_NUM_UNITS 128
#define STENCIL_INPUT_PORTS_PER_UNIT 3
#define STENCIL_TOTAL_CORES 384

class StencilCoreGraph : public graph {
public:
    static constexpr int kNumUnits = STENCIL_NUM_UNITS;
    static constexpr int kInputCount = STENCIL_INPUT_PORTS_PER_UNIT;
    static constexpr int kTotalInputPorts = kNumUnits * kInputCount;

    port<input>  in[kTotalInputPorts];
    port<output> out[kNumUnits];

    kernel k_lap[kNumUnits];
    kernel k_flux1[kNumUnits];
    kernel k_flux2[kNumUnits];

    static constexpr int input_index(int lane, int idx) {
        return lane * kInputCount + idx;
    }

    StencilCoreGraph() {
#if defined(__AIESIM__) || defined(__X86SIM__) || defined(__ADF_FRONTEND__)
        for (int lane = 0; lane < kNumUnits; ++lane) {
            k_lap[lane]   = kernel::create(hdiff_lap);
            k_flux1[lane] = kernel::create(hdiff_flux1);
            k_flux2[lane] = kernel::create(hdiff_flux2);

            source(k_lap[lane])   = "ProcessUnit/hdiff_lap.cc";
            source(k_flux1[lane]) = "ProcessUnit/hdiff_flux1.cc";
            source(k_flux2[lane]) = "ProcessUnit/hdiff_flux2.cc";

            headers(k_lap[lane])   = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
            headers(k_flux1[lane]) = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
            headers(k_flux2[lane]) = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};

            runtime<ratio>(k_lap[lane])   = 0.9;
            runtime<ratio>(k_flux1[lane]) = 0.9;
            runtime<ratio>(k_flux2[lane]) = 0.9;

            location<kernel>(k_lap[lane]) =
                tile(lap_tile_col(lane), lap_tile_row(lane));
            location<kernel>(k_flux1[lane]) =
                tile(flux1_tile_col(lane), flux1_tile_row(lane));
            location<kernel>(k_flux2[lane]) =
                tile(flux2_tile_col(lane), flux2_tile_row(lane));

            auto net_in_lap =
                connect(in[input_index(lane, 0)], k_lap[lane].in[0]);
            auto net_in_flux1 =
                connect(in[input_index(lane, 1)], k_flux1[lane].in[0]);
            auto net_in_flux2 =
                connect(in[input_index(lane, 2)], k_flux2[lane].in[1]);

            dimensions(k_lap[lane].in[0])   = {hdiff_cfg::kLapInputSampleElems};
            dimensions(k_flux1[lane].in[0]) = {hdiff_cfg::kFlux1RawInputSampleElems};
            dimensions(k_flux2[lane].in[1]) = {hdiff_cfg::kFlux2RawInputSampleElems};
            fifo_depth(net_in_lap)           = 8;
            fifo_depth(net_in_flux1)         = 8;
            fifo_depth(net_in_flux2)         = 8;

            auto net_lap_f1 =
                connect(k_lap[lane].out[0], k_flux1[lane].in[1]);

            dimensions(k_lap[lane].out[0])  = {hdiff_cfg::kFluxForwardPackElems};
            dimensions(k_flux1[lane].in[1]) = {hdiff_cfg::kFluxForwardPackElems};
            fifo_depth(net_lap_f1)           = 6;

            auto net_lap_f2 =
                connect(k_lap[lane].out[1], k_flux2[lane].in[2]);

            dimensions(k_lap[lane].out[1])  = {hdiff_cfg::kFluxForwardPackElems};
            dimensions(k_flux2[lane].in[2]) = {hdiff_cfg::kFluxForwardPackElems};
            fifo_depth(net_lap_f2)           = 6;

            auto net_f1_f2 =
                connect(k_flux1[lane].out[0], k_flux2[lane].in[0]);

            dimensions(k_flux1[lane].out[0]) = {hdiff_cfg::kMaskCompletePackElems};
            dimensions(k_flux2[lane].in[0])  = {hdiff_cfg::kMaskCompletePackElems};
            fifo_depth(net_f1_f2)             = 6;

            auto net_out = connect(k_flux2[lane].out[0], out[lane]);

            dimensions(k_flux2[lane].out[0]) = {COL};
            dimensions(out[lane])             = {COL};
            fifo_depth(net_out)               = 2;
        }
#endif
    }

private:
    static constexpr bool is_main_lane(int lane) {
        return lane < STENCIL_MAIN_LANES;
    }

    static constexpr int main_tile_col(int lane) {
        return lane / STENCIL_UNITS_PER_COL;
    }

    static constexpr int main_base_row(int lane) {
        return 2 + (lane % STENCIL_UNITS_PER_COL) * STENCIL_ROWS_PER_UNIT;
    }

    static constexpr int top_lane_index(int lane) {
        return lane - STENCIL_MAIN_LANES;
    }

    static constexpr int top_base_col(int lane) {
        return (top_lane_index(lane) % STENCIL_TOP_LANES_PER_ROW) *
               STENCIL_ROWS_PER_UNIT;
    }

    static constexpr int top_row(int lane) {
        return top_lane_index(lane) / STENCIL_TOP_LANES_PER_ROW;
    }

    static constexpr int lap_tile_col(int lane) {
        return is_main_lane(lane) ? main_tile_col(lane) : top_base_col(lane);
    }

    static constexpr int lap_tile_row(int lane) {
        return is_main_lane(lane) ? main_base_row(lane) : top_row(lane);
    }

    static constexpr int flux1_tile_col(int lane) {
        return is_main_lane(lane) ? main_tile_col(lane) : top_base_col(lane) + 1;
    }

    static constexpr int flux1_tile_row(int lane) {
        return is_main_lane(lane) ? main_base_row(lane) + 1 : top_row(lane);
    }

    static constexpr int flux2_tile_col(int lane) {
        return is_main_lane(lane) ? main_tile_col(lane) : top_base_col(lane) + 2;
    }

    static constexpr int flux2_tile_row(int lane) {
        return is_main_lane(lane) ? main_base_row(lane) + 2 : top_row(lane);
    }

    static_assert(STENCIL_MAIN_LANES +
                      STENCIL_TOP_ROWS * STENCIL_TOP_LANES_PER_ROW ==
                  STENCIL_NUM_UNITS);
    static_assert(STENCIL_NUM_UNITS * STENCIL_INPUT_PORTS_PER_UNIT ==
                  STENCIL_TOTAL_CORES);
};
