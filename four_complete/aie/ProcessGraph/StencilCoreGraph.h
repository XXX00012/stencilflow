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
    //   in[0] -> lap_complete     : reuse-oriented 4-row Laplacian package
    //   in[1] -> mscom_complete   : raw psi differences for compare masks
    //   in[2] -> selupdate        : center psi row for final update
    port<input>  in[3];
    port<output> out;

    kernel k_lap;
    kernel k_sub;
    kernel k_mscom;
    kernel k_selupdate;

    StencilCoreGraph() {
#if defined(__AIESIM__) || defined(__X86SIM__) || defined(__ADF_FRONTEND__)
        k_lap       = kernel::create(hdiff_lap);
        k_sub       = kernel::create(hdiff_sub);
        k_mscom     = kernel::create(hdiff_mscom);
        k_selupdate = kernel::create(hdiff_selupdate);

        source(k_lap)       = "ProcessUnit/hdiff_lap.cc";
        source(k_sub)       = "ProcessUnit/hdiff_sub.cc";
        source(k_mscom)     = "ProcessUnit/hdiff_mscom.cc";
        source(k_selupdate) = "ProcessUnit/hdiff_selupdate.cc";

        headers(k_lap)       = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
        headers(k_sub)       = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
        headers(k_mscom)     = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
        headers(k_selupdate) = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};

        runtime<ratio>(k_lap)       = 0.9;
        runtime<ratio>(k_sub)       = 0.9;
        runtime<ratio>(k_mscom)     = 0.9;
        runtime<ratio>(k_selupdate) = 0.9;

        location<kernel>(k_lap)       = tile(7, 0);
        location<kernel>(k_sub)       = tile(7, 1);
        location<kernel>(k_mscom)     = tile(7, 2);
        location<kernel>(k_selupdate) = tile(7, 3);

        // ------------------------------------------------------------
        // External row streams. Each firing consumes one new raw row.
        // The 5-row/center-row alignment is handled by buffer margins
        // in lap/mscom/selupdate.
        // ------------------------------------------------------------
        auto net_in_lap       = connect(in[0], k_lap.in[0]);
        auto net_in_mscom     = connect(in[1], k_mscom.in[0]);
        auto net_in_selupdate = connect(in[2], k_selupdate.in[2]);

        dimensions(k_lap.in[0])       = {COL};
        dimensions(k_mscom.in[0])     = {COL};
        dimensions(k_selupdate.in[2]) = {COL};
        fifo_depth(net_in_lap)         = 4;
        fifo_depth(net_in_mscom)       = 4;
        fifo_depth(net_in_selupdate)   = 4;

        // lap_complete -> sub_complete: 4-row reuse package
        auto net_lap_sub = connect(k_lap.out[0], k_sub.in[0]);
        dimensions(k_lap.out[0]) = {hdiff_cfg::kLapReusePackElems};
        dimensions(k_sub.in[0])  = {hdiff_cfg::kLapReusePackElems};
        fifo_depth(net_lap_sub)  = 2;

        // sub_complete branches to mscom and selupdate: 4 complete flux differences
        auto net_sub_mscom = connect(k_sub.out[0], k_mscom.in[1]);
        dimensions(k_sub.out[0])   = {hdiff_cfg::kSubCompletePackElems};
        dimensions(k_mscom.in[1])  = {hdiff_cfg::kSubCompletePackElems};
        fifo_depth(net_sub_mscom)  = 2;

        auto net_sub_selupdate = connect(k_sub.out[1], k_selupdate.in[1]);
        dimensions(k_sub.out[1])       = {hdiff_cfg::kSubCompletePackElems};
        dimensions(k_selupdate.in[1])  = {hdiff_cfg::kSubCompletePackElems};
        fifo_depth(net_sub_selupdate)  = 4;

        // mscom_complete -> selupdate: 4 compact mask rows
        auto net_mscom_selupdate = connect(k_mscom.out[0], k_selupdate.in[0]);
        dimensions(k_mscom.out[0])     = {hdiff_cfg::kMaskCompletePackElems};
        dimensions(k_selupdate.in[0])  = {hdiff_cfg::kMaskCompletePackElems};
        fifo_depth(net_mscom_selupdate) = 2;

        // selupdate -> output: 1 row
        auto net_out = connect(k_selupdate.out[0], out);
        dimensions(k_selupdate.out[0]) = {COL};
        dimensions(out)                = {COL};
        fifo_depth(net_out)            = 2;
#endif
    }
};
