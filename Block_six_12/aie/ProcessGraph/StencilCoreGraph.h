#pragma once

#include <adf.h>
#include "../ProcessUnit/hdiff.h"
#include "Config.h"

using namespace adf;

class StencilCoreGraph : public adf::graph {
public:
  static constexpr int kInputRows   = 2;
  static constexpr int kBlockDepth  = 2;
  static constexpr int kRowElems    = 256;

  adf::port<input>  in[kInputRows*3];
  adf::port<output> out[kInputRows];

  adf::kernel k_lap2[kBlockDepth];
  adf::kernel k_sub[kBlockDepth];
  adf::kernel k_ms[kBlockDepth];
  adf::kernel k_com[kBlockDepth];
  adf::kernel k_sel[kBlockDepth];
  adf::kernel k_update[kBlockDepth];

  StencilCoreGraph(int base_col, int base_row)
      : base_col_(base_col), base_row_(base_row) {

#if defined(__AIESIM__) || defined(__X86SIM__) || defined(__ADF_FRONTEND__)

        k_lap2[0]   = kernel::create(hdiff_lap2);
        k_sub[0]    = kernel::create(hdiff_sub);
        k_ms[0]     = kernel::create(hdiff_ms);
        k_com[0]    = kernel::create(hdiff_com);
        k_sel[0]    = kernel::create(hdiff_sel);
        k_update[0] = kernel::create(hdiff_update);

      
        source(k_lap2[0])   = "ProcessUnit/hdiff_lap2.cc";
        source(k_sub[0])    = "ProcessUnit/hdiff_sub.cc";
        source(k_ms[0])     = "ProcessUnit/hdiff_ms.cc";
        source(k_com[0])    = "ProcessUnit/hdiff_com.cc";
        source(k_sel[0])    = "ProcessUnit/hdiff_sel.cc";
        source(k_update[0]) = "ProcessUnit/hdiff_update.cc";

        headers(k_lap2[0])   = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
        headers(k_sub[0])    = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
        headers(k_ms[0])     = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
        headers(k_com[0])    = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
        headers(k_sel[0])    = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
        headers(k_update[0]) = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};

    
        runtime<ratio>(k_lap2[0])   = 0.9;
        runtime<ratio>(k_sub[0])    = 0.9;
        runtime<ratio>(k_ms[0])     = 0.9;
        runtime<ratio>(k_com[0])    = 0.9;
        runtime<ratio>(k_sel[0])    = 0.9;
        runtime<ratio>(k_update[0]) = 0.9;

        location<kernel>(k_lap2[0])   = tile(0,1);
        location<kernel>(k_sub[0])    = tile(0,2);
        location<kernel>(k_ms[0])     = tile(0,3);
        location<kernel>(k_com[0])    = tile(0,4);
        location<kernel>(k_sel[0])    = tile(0,5);
        location<kernel>(k_update[0]) = tile(0,6);

        // -------------------------
        // External input fanout
        // -------------------------
        auto net_in_lap2_0   = connect(in[0], k_lap2[0].in[0]);
        auto net_in_ms_0     = connect(in[1], k_ms[0].in[0]);
        auto net_in_update_0 = connect(in[2], k_update[0].in[1]);

  
        dimensions(k_lap2[0].in[0])   = {COL};
        dimensions(k_ms[0].in[0])     = {COL};
        dimensions(k_update[0].in[1]) = {COL};

        // fifo_depth(net_in_lap1)   = 2;
        // fifo_depth(net_in_lap2)   = 2;
        // fifo_depth(net_in_ms)     = 2;
        // fifo_depth(net_in_update) = 2;

        // lap1 -> lap2 : 3 rows
    

        // lap2 -> sub : 3 rows
        auto net_lap2_sub_0 = connect(k_lap2[0].out[0], k_sub[0].in[0]);
        dimensions(k_lap2[0].out[0]) = {3 * COL};
        dimensions(k_sub[0].in[0])   = {3 * COL};
        fifo_depth(net_lap2_sub_0)  = 4;

        // sub -> ms : 4 rows
        auto net_sub_ms_0 = connect(k_sub[0].out[0], k_ms[0].in[1]);
        dimensions(k_sub[0].out[0]) = {4 * COL};
        dimensions(k_ms[0].in[1])   = {4 * COL};
        fifo_depth(net_sub_ms_0)   = 4;

        // sub -> comsel : 4 rows
        auto net_sub_sel_0 = connect(k_sub[0].out[0], k_sel[0].in[1]);
        dimensions(k_sel[0].in[1]) = {4 * COL};
        fifo_depth(net_sub_sel_0) = 4;

        // ms -> comsel : 3 rows   <-- 这里必须从旧版 4*COL 改成 3*COL
        auto net_ms_com_0 = connect(k_ms[0].out[0], k_com[0].in[0]);
        dimensions(k_ms[0].out[0])    = {3 * COL};
        dimensions(k_com[0].in[0])    = {3 * COL};
        // fifo_depth(net_ms_com)  = 2;

        auto net_com_sel_0 = connect(k_com[0].out[0], k_sel[0].in[0]);
        dimensions(k_com[0].out[0]) = {3 * (COL / 8)};
        dimensions(k_sel[0].in[0])  = {3 * (COL / 8)};
        fifo_depth(net_com_sel_0) = 3;
        // com -> update : 3 rows
        // 注意：只有当 hdiff_update.cc 也已经改成 3 行 shared-vertical 口径时，这里才成立。
        auto net_sel_update_0 = connect(k_sel[0].out[0], k_update[0].in[0]);
        dimensions(k_sel[0].out[0])   = {4 * COL};
        dimensions(k_update[0].in[0]) = {4 * COL};
        fifo_depth(net_sel_update_0) = 3;

        // update -> out : 1 row
        auto net_out_0 = connect(k_update[0].out[0], out[0]);
        dimensions(k_update[0].out[0]) = {COL};
        dimensions(out[0])             = {COL};
        // fifo_depth(net_out)         = 2;

        k_lap2[1]   = kernel::create(hdiff_lap2);
        k_sub[1]    = kernel::create(hdiff_sub);
        k_ms[1]     = kernel::create(hdiff_ms);
        k_com[1]    = kernel::create(hdiff_com);
        k_sel[1]    = kernel::create(hdiff_sel);
        k_update[1] = kernel::create(hdiff_update);

      
        source(k_lap2[1])   = "ProcessUnit/hdiff_lap2.cc";
        source(k_sub[1])    = "ProcessUnit/hdiff_sub.cc";
        source(k_ms[1])     = "ProcessUnit/hdiff_ms.cc";
        source(k_com[1])    = "ProcessUnit/hdiff_com.cc";
        source(k_sel[1])    = "ProcessUnit/hdiff_sel.cc";
        source(k_update[1]) = "ProcessUnit/hdiff_update.cc";

        headers(k_lap2[1])   = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
        headers(k_sub[1])    = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
        headers(k_ms[1])     = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
        headers(k_com[1])    = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
        headers(k_sel[1])    = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
        headers(k_update[1]) = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};

    
        runtime<ratio>(k_lap2[1])   = 0.9;
        runtime<ratio>(k_sub[1])    = 0.9;
        runtime<ratio>(k_ms[1])     = 0.9;
        runtime<ratio>(k_com[1])    = 0.9;
        runtime<ratio>(k_sel[1])    = 0.9;
        runtime<ratio>(k_update[1]) = 0.9;

        location<kernel>(k_lap2[1])   = tile(1,1);
        location<kernel>(k_sub[1])    = tile(1,2);
        location<kernel>(k_ms[1])     = tile(1,3);
        location<kernel>(k_com[1])    = tile(1,4);
        location<kernel>(k_sel[1])    = tile(1,5);
        location<kernel>(k_update[1]) = tile(1,6);

        // -------------------------
        // External input fanout
        // -------------------------
        auto net_in_lap2_1   = connect(in[3], k_lap2[1].in[0]);
        auto net_in_ms_1     = connect(in[4], k_ms[1].in[0]);
        auto net_in_update_1 = connect(in[5], k_update[1].in[1]);

  
        dimensions(k_lap2[1].in[0])   = {COL};
        dimensions(k_ms[1].in[0])     = {COL};
        dimensions(k_update[1].in[1]) = {COL};

        // fifo_depth(net_in_lap1)   = 2;
        // fifo_depth(net_in_lap2)   = 2;
        // fifo_depth(net_in_ms)     = 2;
        // fifo_depth(net_in_update) = 2;

        // lap1 -> lap2 : 3 rows
    

        // lap2 -> sub : 3 rows
        auto net_lap2_sub_1 = connect(k_lap2[1].out[0], k_sub[1].in[0]);
        dimensions(k_lap2[1].out[0]) = {3 * COL};
        dimensions(k_sub[1].in[0])   = {3 * COL};
        fifo_depth(net_lap2_sub_1)  = 4;

        // sub -> ms : 4 rows
        auto net_sub_ms_1 = connect(k_sub[1].out[0], k_ms[1].in[1]);
        dimensions(k_sub[1].out[0]) = {4 * COL};
        dimensions(k_ms[1].in[1])   = {4 * COL};
        fifo_depth(net_sub_ms_1)   = 4;

        // sub -> comsel : 4 rows
        auto net_sub_sel_1 = connect(k_sub[1].out[0], k_sel[1].in[1]);
        dimensions(k_sel[1].in[1]) = {4 * COL};
        fifo_depth(net_sub_sel_1) = 4;

        // ms -> comsel : 3 rows   <-- 这里必须从旧版 4*COL 改成 3*COL
        auto net_ms_com_1 = connect(k_ms[1].out[0], k_com[1].in[0]);
        dimensions(k_ms[1].out[0])    = {3 * COL};
        dimensions(k_com[1].in[0])    = {3 * COL};
        // fifo_depth(net_ms_com)  = 2;

        auto net_com_sel_1 = connect(k_com[1].out[0], k_sel[1].in[0]);
        dimensions(k_com[1].out[0]) = {3 * (COL / 8)};
        dimensions(k_sel[1].in[0])  = {3 * (COL / 8)};
        fifo_depth(net_com_sel_1) = 3;
        // com -> update : 3 rows
        // 注意：只有当 hdiff_update.cc 也已经改成 3 行 shared-vertical 口径时，这里才成立。
        auto net_sel_update_1 = connect(k_sel[1].out[0], k_update[1].in[0]);
        dimensions(k_sel[1].out[0])   = {4 * COL};
        dimensions(k_update[1].in[0]) = {4 * COL};
        fifo_depth(net_sel_update_1) = 3;

        // update -> out : 1 row
        auto net_out_1 = connect(k_update[1].out[0], out[1]);
        dimensions(k_update[1].out[0]) = {COL};
        dimensions(out[1])             = {COL};
        // fifo_depth(net_out)         = 2;



#endif
  }

private:
  int base_col_;
  int base_row_;
};