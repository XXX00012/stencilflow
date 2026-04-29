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

  
    kernel k_lap2;
    kernel k_sub;
    kernel k_ms;
    kernel k_comsel;
    kernel k_update;

    StencilCoreGraph() {
#if defined(__AIESIM__) || defined(__X86SIM__) || defined(__ADF_FRONTEND__)
  
        k_lap2   = kernel::create(hdiff_lap2);
        k_sub    = kernel::create(hdiff_sub);
        k_ms     = kernel::create(hdiff_ms);
        k_comsel = kernel::create(hdiff_comsel);
        k_update = kernel::create(hdiff_update);

       
        source(k_lap2)   = "ProcessUnit/hdiff_lap2.cc";
        source(k_sub)    = "ProcessUnit/hdiff_sub.cc";
        source(k_ms)     = "ProcessUnit/hdiff_ms.cc";
        source(k_comsel) = "ProcessUnit/hdiff_comsel.cc";
        source(k_update) = "ProcessUnit/hdiff_update.cc";

   
        headers(k_lap2)   = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
        headers(k_sub)    = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
        headers(k_ms)     = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
        headers(k_comsel) = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
        headers(k_update) = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};

  
        runtime<ratio>(k_lap2)   = 0.9;
        runtime<ratio>(k_sub)    = 0.9;
        runtime<ratio>(k_ms)     = 0.9;
        runtime<ratio>(k_comsel) = 0.9;
        runtime<ratio>(k_update) = 0.9;


        location<kernel>(k_lap2)   = tile(7,1);
        location<kernel>(k_sub)    = tile(7,2);
        location<kernel>(k_ms)     = tile(7,3);
        location<kernel>(k_comsel) = tile(7,4);
        location<kernel>(k_update) = tile(7,5);

        // -------------------------
        // External input fanout
        // -------------------------
  
        auto net_in_lap2   = connect(in[0], k_lap2.in[0]);
        auto net_in_ms     = connect(in[1], k_ms.in[0]);
        auto net_in_update = connect(in[2], k_update.in[1]);


        dimensions(k_lap2.in[0])   = {COL};
        dimensions(k_ms.in[0])     = {COL};
        dimensions(k_update.in[1]) = {COL};

        // fifo_depth(net_in_lap1)   = 2;
        // fifo_depth(net_in_lap2)   = 2;
        // fifo_depth(net_in_ms)     = 2;
        // fifo_depth(net_in_update) = 2;

     

        // lap2 -> sub : 3 rows
        auto net_lap2_sub = connect(k_lap2.out[0], k_sub.in[0]);
        dimensions(k_lap2.out[0]) = {3 * COL};
        dimensions(k_sub.in[0])   = {3 * COL};
        // fifo_depth(net_lap2_sub)  = 2;

        // sub -> ms : 4 rows
        auto net_sub_ms = connect(k_sub.out[0], k_ms.in[1]);
        dimensions(k_sub.out[0]) = {4 * COL};
        dimensions(k_ms.in[1])   = {4 * COL};

        auto net_sub_comsel = connect(k_sub.out[1], k_comsel.in[1]);
        dimensions(k_sub.out[1])   = {4 * COL};
        dimensions(k_comsel.in[1]) = {4 * COL};
        // fifo_depth(net_sub_comsel) = 2;

        // ms -> comsel : 3 rows   <-- 这里必须从旧版 4*COL 改成 3*COL
        auto net_ms_comsel = connect(k_ms.out[0], k_comsel.in[0]);
        dimensions(k_ms.out[0])    = {3 * COL};
        dimensions(k_comsel.in[0]) = {3 * COL};
        // fifo_depth(net_ms_comsel)  = 2;

        // comsel -> update : 3 rows
        // 注意：只有当 hdiff_update.cc 也已经改成 3 行 shared-vertical 口径时，这里才成立。
        auto net_comsel_update = connect(k_comsel.out[0], k_update.in[0]);
        dimensions(k_comsel.out[0]) = {4 * COL};
        dimensions(k_update.in[0])  = {4 * COL};
        // fifo_depth(net_comsel_update) = 2;

        // update -> out : 1 row
        auto net_out = connect(k_update.out[0], out);
        dimensions(k_update.out[0]) = {COL};
        dimensions(out)             = {COL};
        // fifo_depth(net_out)         = 2;
#endif
    }
};