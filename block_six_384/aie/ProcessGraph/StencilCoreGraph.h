#pragma once

#include <adf.h>
#include "../ProcessUnit/hdiff.h"
#include "Config.h"

using namespace adf;

class StencilCoreGraph : public adf::graph {
public:
  // ------------------------------------------------------------
  // 原来 50 个 block 竖排：50 * 6 = 300 核
  // 新增 14 个 block 横排：14 * 6 = 84 核
  // 总计 64 个 block -> 384 核
  // ------------------------------------------------------------
  static constexpr int kMainBlocks            = 50;
  static constexpr int kExtraBlocksPerRow     = 7;
  static constexpr int kExtraRows             = 2;
  static constexpr int kExtraBlocks           = kExtraBlocksPerRow * kExtraRows; // 14
  static constexpr int kNumBlocks             = kMainBlocks + kExtraBlocks;      // 64

  static constexpr int kInputsPerBlock        = 3;   // lap2 / ms / update
  static constexpr int kRowElems              = 256;
  static constexpr int kKernelsPerBlock       = 6;   // lap2 sub ms com sel update

  adf::port<input>  in[kNumBlocks * kInputsPerBlock];
  adf::port<output> out[kNumBlocks];

  adf::kernel k_lap2[kNumBlocks];
  adf::kernel k_sub[kNumBlocks];
  adf::kernel k_ms[kNumBlocks];
  adf::kernel k_com[kNumBlocks];
  adf::kernel k_sel[kNumBlocks];
  adf::kernel k_update[kNumBlocks];

  // 保留两个参数只是为了不影响你上层构造调用；
  // 这版 placement 直接用绝对物理坐标，不再靠 base_row/base_col 偏移。
  StencilCoreGraph(int /*base_col*/ = 0, int /*base_row*/ = 0) {

#if defined(__AIESIM__) || defined(__X86SIM__) || defined(__ADF_FRONTEND__)

    for (int b = 0; b < kNumBlocks; ++b) {
      const int in_base = b * kInputsPerBlock;

      // ------------------------------------------------------------
      // kernel create
      // ------------------------------------------------------------
      k_lap2[b]   = kernel::create(hdiff_lap2);
      k_sub[b]    = kernel::create(hdiff_sub);
      k_ms[b]     = kernel::create(hdiff_ms);
      k_com[b]    = kernel::create(hdiff_com);
      k_sel[b]    = kernel::create(hdiff_sel);
      k_update[b] = kernel::create(hdiff_update);

      // ------------------------------------------------------------
      // source
      // ------------------------------------------------------------
      source(k_lap2[b])   = "ProcessUnit/hdiff_lap2.cc";
      source(k_sub[b])    = "ProcessUnit/hdiff_sub.cc";
      source(k_ms[b])     = "ProcessUnit/hdiff_ms.cc";
      source(k_com[b])    = "ProcessUnit/hdiff_com.cc";
      source(k_sel[b])    = "ProcessUnit/hdiff_sel.cc";
      source(k_update[b]) = "ProcessUnit/hdiff_update.cc";

      // ------------------------------------------------------------
      // headers
      // ------------------------------------------------------------
      headers(k_lap2[b])   = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
      headers(k_sub[b])    = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
      headers(k_ms[b])     = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
      headers(k_com[b])    = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
      headers(k_sel[b])    = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
      headers(k_update[b]) = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};

      // ------------------------------------------------------------
      // runtime ratio
      // ------------------------------------------------------------
      runtime<ratio>(k_lap2[b])   = 0.9;
      runtime<ratio>(k_sub[b])    = 0.9;
      runtime<ratio>(k_ms[b])     = 0.9;
      runtime<ratio>(k_com[b])    = 0.9;
      runtime<ratio>(k_sel[b])    = 0.9;
      runtime<ratio>(k_update[b]) = 0.9;

      // ------------------------------------------------------------
      // placement
      // 前 50 个 block：
      //   竖排放在 (0,2) ~ (49,7)
      //
      // 后 14 个 block：
      //   横排放在
      //   row 0: (0,0) ~ (41,0)
      //   row 1: (0,1) ~ (41,1)
      // ------------------------------------------------------------
      place_block(b);

      // ------------------------------------------------------------
      // External input
      // in[3*b + 0] -> lap2
      // in[3*b + 1] -> ms
      // in[3*b + 2] -> update
      // ------------------------------------------------------------
      connect(in[in_base + 0], k_lap2[b].in[0]);
      connect(in[in_base + 1], k_ms[b].in[0]);
      connect(in[in_base + 2], k_update[b].in[1]);

      dimensions(k_lap2[b].in[0])   = {COL};
      dimensions(k_ms[b].in[0])     = {COL};
      dimensions(k_update[b].in[1]) = {COL};

      // ------------------------------------------------------------
      // lap2 -> sub : 3 rows
      // ------------------------------------------------------------
      auto net_lap2_sub = connect(k_lap2[b].out[0], k_sub[b].in[0]);
      dimensions(k_lap2[b].out[0]) = {3 * COL};
      dimensions(k_sub[b].in[0])   = {3 * COL};

      // ------------------------------------------------------------
      // sub -> ms : 4 rows
      // ------------------------------------------------------------
      auto net_sub_ms = connect(k_sub[b].out[0], k_ms[b].in[1]);
      dimensions(k_sub[b].out[0]) = {4 * COL};
      dimensions(k_ms[b].in[1])   = {4 * COL};

      // ------------------------------------------------------------
      // sub -> sel : 4 rows
      // ------------------------------------------------------------
      auto net_sub_sel = connect(k_sub[b].out[0], k_sel[b].in[1]);
      dimensions(k_sel[b].in[1]) = {4 * COL};

      // ------------------------------------------------------------
      // ms -> com : 3 rows
      // ------------------------------------------------------------
      connect(k_ms[b].out[0], k_com[b].in[0]);
      dimensions(k_ms[b].out[0]) = {3 * COL};
      dimensions(k_com[b].in[0]) = {3 * COL};

      // ------------------------------------------------------------
      // com -> sel
      // ------------------------------------------------------------
      auto net_com_sel = connect(k_com[b].out[0], k_sel[b].in[0]);
      dimensions(k_com[b].out[0]) = {3 * (COL / 8)};
      dimensions(k_sel[b].in[0])  = {3 * (COL / 8)};
      fifo_depth(net_com_sel)     = 2;

      // ------------------------------------------------------------
      // sel -> update : 4 rows
      // ------------------------------------------------------------
      auto net_sel_update = connect(k_sel[b].out[0], k_update[b].in[0]);
      dimensions(k_sel[b].out[0])   = {4 * COL};
      dimensions(k_update[b].in[0]) = {4 * COL};

      // ------------------------------------------------------------
      // update -> out : 1 row
      // ------------------------------------------------------------
      connect(k_update[b].out[0], out[b]);
      dimensions(k_update[b].out[0]) = {COL};
      dimensions(out[b])             = {COL};
    }

#endif
  }

private:
  void place_block(int b) {
    if (b < kMainBlocks) {
      // ----------------------------------------------------------
      // 主区域：前 50 个 block 竖着放
      // 第 b 个 block 占：
      //   (b,2) lap2
      //   (b,3) sub
      //   (b,4) ms
      //   (b,5) com
      //   (b,6) sel
      //   (b,7) update
      // ----------------------------------------------------------
      const int col = b;

      location<kernel>(k_lap2[b])   = tile(col, 2);
      location<kernel>(k_sub[b])    = tile(col, 3);
      location<kernel>(k_ms[b])     = tile(col, 4);
      location<kernel>(k_com[b])    = tile(col, 5);
      location<kernel>(k_sel[b])    = tile(col, 6);
      location<kernel>(k_update[b]) = tile(col, 7);
    } else {
      // ----------------------------------------------------------
      // 额外 14 个 block 分两行横着摆
      //
      // b = 50..56 -> row 0
      // b = 57..63 -> row 1
      //
      // 每个 block 横向占 6 个 tile:
      //   lap2 | sub | ms | com | sel | update
      // ----------------------------------------------------------
      const int extra_id  = b - kMainBlocks;              // 0..13
      const int row       = extra_id / kExtraBlocksPerRow; // 0 or 1
      const int slot      = extra_id % kExtraBlocksPerRow; // 0..6
      const int start_col = slot * kKernelsPerBlock;       // 0,6,12,...,36

      location<kernel>(k_lap2[b])   = tile(start_col + 0, row);
      location<kernel>(k_sub[b])    = tile(start_col + 1, row);
      location<kernel>(k_ms[b])     = tile(start_col + 2, row);
      location<kernel>(k_com[b])    = tile(start_col + 3, row);
      location<kernel>(k_sel[b])    = tile(start_col + 4, row);
      location<kernel>(k_update[b]) = tile(start_col + 5, row);
    }
  }
};