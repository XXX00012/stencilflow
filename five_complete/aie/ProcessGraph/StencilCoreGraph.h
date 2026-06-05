#pragma once

#include <adf.h>
#include "../ProcessUnit/include.h"
#include "../Config.h"
#include "../ProcessUnit/hdiff.h"

using namespace adf;

class StencilCoreGraph : public graph {
public:
    port<input>  in[3];
    port<output> out;

    kernel k_lap;
    kernel k_sub;
    kernel k_ms;
    kernel k_comsel;
    kernel k_update;

    StencilCoreGraph() {
#if defined(__AIESIM__) || defined(__X86SIM__) || defined(__ADF_FRONTEND__)
        k_lap    = kernel::create(hdiff_lap);
        k_sub    = kernel::create(hdiff_sub);
        k_ms     = kernel::create(hdiff_ms);
        k_comsel = kernel::create(hdiff_comsel);
        k_update = kernel::create(hdiff_update);

        source(k_lap)    = "aie/ProcessUnit/hdiff_lap.cc";
        source(k_sub)    = "aie/ProcessUnit/hdiff_sub.cc";
        source(k_ms)     = "aie/ProcessUnit/hdiff_ms.cc";
        source(k_comsel) = "aie/ProcessUnit/hdiff_comsel.cc";
        source(k_update) = "aie/ProcessUnit/hdiff_update.cc";

        headers(k_lap)    = {"aie/ProcessUnit/hdiff.h", "aie/ProcessUnit/include.h", "aie/Config.h"};
        headers(k_sub)    = {"aie/ProcessUnit/hdiff.h", "aie/ProcessUnit/include.h", "aie/Config.h"};
        headers(k_ms)     = {"aie/ProcessUnit/hdiff.h", "aie/ProcessUnit/include.h", "aie/Config.h"};
        headers(k_comsel) = {"aie/ProcessUnit/hdiff.h", "aie/ProcessUnit/include.h", "aie/Config.h"};
        headers(k_update) = {"aie/ProcessUnit/hdiff.h", "aie/ProcessUnit/include.h", "aie/Config.h"};

        runtime<ratio>(k_lap)    = 0.9;
        runtime<ratio>(k_sub)    = 0.9;
        runtime<ratio>(k_ms)     = 0.9;
        runtime<ratio>(k_comsel) = 0.9;
        runtime<ratio>(k_update) = 0.9;

        location<kernel>(k_lap)    = tile(7, 1);
        location<kernel>(k_sub)    = tile(7, 2);
        location<kernel>(k_ms)     = tile(7, 3);
        location<kernel>(k_comsel) = tile(7, 4);
        location<kernel>(k_update) = tile(7, 5);

        connect<>(in[0], k_lap.in[0]);
        connect<>(in[1], k_ms.in[0]);
        connect<>(in[2], k_update.in[1]);

        dimensions(k_lap.in[0])    = {COL};
        dimensions(k_ms.in[0])     = {COL};
        dimensions(k_update.in[1]) = {COL};

        auto net_lap_sub = connect(k_lap.out[0], k_sub.in[0]);
        dimensions(k_lap.out[0]) = {hdiff_cfg::kLapReusePackElems};
        dimensions(k_sub.in[0])  = {hdiff_cfg::kLapReusePackElems};
        fifo_depth(net_lap_sub)  = 2;

        auto net_sub_ms = connect(k_sub.out[0], k_ms.in[1]);
        dimensions(k_sub.out[0]) = {hdiff_cfg::kSubCompletePackElems};
        dimensions(k_ms.in[1])   = {hdiff_cfg::kSubCompletePackElems};
        fifo_depth(net_sub_ms)   = 2;

        auto net_sub_comsel = connect(k_sub.out[1], k_comsel.in[1]);
        dimensions(k_sub.out[1])    = {hdiff_cfg::kSubCompletePackElems};
        dimensions(k_comsel.in[1])  = {hdiff_cfg::kSubCompletePackElems};
        fifo_depth(net_sub_comsel)  = 4;

        auto net_ms_comsel = connect(k_ms.out[0], k_comsel.in[0]);
        dimensions(k_ms.out[0])    = {hdiff_cfg::kMsCompletePackElems};
        dimensions(k_comsel.in[0]) = {hdiff_cfg::kMsCompletePackElems};
        fifo_depth(net_ms_comsel)  = 2;

        auto net_comsel_update = connect(k_comsel.out[0], k_update.in[0]);
        dimensions(k_comsel.out[0]) = {hdiff_cfg::kSelCompletePackElems};
        dimensions(k_update.in[0])  = {hdiff_cfg::kSelCompletePackElems};
        fifo_depth(net_comsel_update) = 2;

        auto net_out = connect(k_update.out[0], out);
        dimensions(k_update.out[0]) = {COL};
        dimensions(out)             = {COL};
        fifo_depth(net_out)         = 2;
#endif
    }
};
