#pragma once

#include <adf.h>
#include "../ProcessUnit/include.h"
#include "../Config.h"
#include "../ProcessUnit/hdiff.h"

using namespace adf;

class StencilCoreGraph : public graph {
public:
    // in[0], in[1], in[2] normally connect to the same row stream file/source.
    // They are split externally to avoid depending on one external stream fanout.
    //   in[0] -> lap_complete  : reuse-oriented 4-row Laplacian package
    //   in[1] -> ms_complete   : raw psi differences for compare product
    //   in[2] -> update        : center psi row for final update
    port<input>  in[3];
    port<output> out;

    kernel k_lap;
    kernel k_sub;
    kernel k_ms;
    kernel k_com;
    kernel k_sel;
    kernel k_update;

    StencilCoreGraph() {
#if defined(__AIESIM__) || defined(__X86SIM__) || defined(__ADF_FRONTEND__)
        k_lap    = kernel::create(hdiff_lap);
        k_sub    = kernel::create(hdiff_sub);
        k_ms     = kernel::create(hdiff_ms);
        k_com    = kernel::create(hdiff_com);
        k_sel    = kernel::create(hdiff_sel);
        k_update = kernel::create(hdiff_update);

        source(k_lap)    = "ProcessUnit/hdiff_lap.cc";
        source(k_sub)    = "ProcessUnit/hdiff_sub.cc";
        source(k_ms)     = "ProcessUnit/hdiff_ms.cc";
        source(k_com)    = "ProcessUnit/hdiff_com.cc";
        source(k_sel)    = "ProcessUnit/hdiff_sel.cc";
        source(k_update) = "ProcessUnit/hdiff_update.cc";

        headers(k_lap)    = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
        headers(k_sub)    = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
        headers(k_ms)     = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
        headers(k_com)    = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
        headers(k_sel)    = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
        headers(k_update) = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};

        runtime<ratio>(k_lap)    = 0.9;
        runtime<ratio>(k_sub)    = 0.9;
        runtime<ratio>(k_ms)     = 0.9;
        runtime<ratio>(k_com)    = 0.9;
        runtime<ratio>(k_sel)    = 0.9;
        runtime<ratio>(k_update) = 0.9;

        location<kernel>(k_lap)    = tile(7, 0);
        location<kernel>(k_sub)    = tile(7, 1);
        location<kernel>(k_ms)     = tile(7, 2);
        location<kernel>(k_com)    = tile(7, 3);
        location<kernel>(k_sel)    = tile(7, 4);
        location<kernel>(k_update) = tile(7, 5);

        // ------------------------------------------------------------
        // External row streams. Each firing consumes one new raw row.
        // The 5-row/center-row alignment is handled by buffer margins
        // and by the 4-firing warmup in lap/ms/update.
        // ------------------------------------------------------------
        auto net_in_lap    = connect(in[0], k_lap.in[0]);
        auto net_in_ms     = connect(in[1], k_ms.in[0]);
        auto net_in_update = connect(in[2], k_update.in[1]);

        dimensions(k_lap.in[0])    = {COL};
        dimensions(k_ms.in[0])     = {COL};
        dimensions(k_update.in[1]) = {COL};
        fifo_depth(net_in_lap)      = 4;
        fifo_depth(net_in_ms)       = 4;
        fifo_depth(net_in_update)   = 4;

        // lap_complete -> sub_complete: 4-row reuse package
        auto net_lap_sub = connect(k_lap.out[0], k_sub.in[0]);
        dimensions(k_lap.out[0]) = {hdiff_cfg::kLapReusePackElems};
        dimensions(k_sub.in[0])  = {hdiff_cfg::kLapReusePackElems};
        fifo_depth(net_lap_sub)  = 2;

        // sub_complete branches to ms and sel: 4 complete flux differences
        auto net_sub_ms = connect(k_sub.out[0], k_ms.in[1]);
        dimensions(k_sub.out[0]) = {hdiff_cfg::kSubCompletePackElems};
        dimensions(k_ms.in[1])   = {hdiff_cfg::kSubCompletePackElems};
        fifo_depth(net_sub_ms)   = 2;

        auto net_sub_sel = connect(k_sub.out[1], k_sel.in[1]);
        dimensions(k_sub.out[1]) = {hdiff_cfg::kSubCompletePackElems};
        dimensions(k_sel.in[1])  = {hdiff_cfg::kSubCompletePackElems};
        fifo_depth(net_sub_sel)  = 4;

        // ms_complete -> com_complete: 4 complete compare products
        auto net_ms_com = connect(k_ms.out[0], k_com.in[0]);
        dimensions(k_ms.out[0])  = {hdiff_cfg::kMsCompletePackElems};
        dimensions(k_com.in[0])  = {hdiff_cfg::kMsCompletePackElems};
        fifo_depth(net_ms_com)   = 2;

        // com_complete -> sel_complete: 4 compact mask rows
        auto net_com_sel = connect(k_com.out[0], k_sel.in[0]);
        dimensions(k_com.out[0]) = {hdiff_cfg::kMaskCompletePackElems};
        dimensions(k_sel.in[0])  = {hdiff_cfg::kMaskCompletePackElems};
        fifo_depth(net_com_sel)  = 2;

        // sel_complete -> update_complete: 4 selected flux rows
        auto net_sel_update = connect(k_sel.out[0], k_update.in[0]);
        dimensions(k_sel.out[0])    = {hdiff_cfg::kSelCompletePackElems};
        dimensions(k_update.in[0])  = {hdiff_cfg::kSelCompletePackElems};
        fifo_depth(net_sel_update)  = 4;

        // update_complete -> output: 1 row
        auto net_out = connect(k_update.out[0], out);
        dimensions(k_update.out[0]) = {COL};
        dimensions(out)             = {COL};
        fifo_depth(net_out)         = 2;
#endif
    }
};
