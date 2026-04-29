#pragma once

#include <adf.h>
#include "../ProcessUnit/hdiff.h"
#include "Config.h"

using namespace adf;

class StencilCoreGraph : public adf::graph {
public:
  static constexpr int kInputRows    = 6;    // 每个 dual block 6 路
  static constexpr int kBlockDepth   = 6;    // 每列 6 个 kernel
  static constexpr int kRowElems     = 256;

  static constexpr int kColsPerBlock = 2;    // lap 1列 + flux 1列
  static constexpr int kNumBlocks    = 25;   // 50列 / 2 = 25个dual block

  static constexpr int kInputCount   = kNumBlocks * kInputRows;   // 150
  static constexpr int kOutputCount  = kNumBlocks * kInputRows;   // 150

  adf::port<input>  in[kInputCount];
  adf::port<output> out[kOutputCount];

  adf::kernel lap[kNumBlocks][kBlockDepth];
  adf::kernel flux[kNumBlocks][kBlockDepth];

  StencilCoreGraph(int base_col = 0, int base_row = 0)
      : base_col_(base_col), base_row_(base_row) {

#if defined(__AIESIM__) || defined(__X86SIM__) || defined(__ADF_FRONTEND__)

    for (int b = 0; b < kNumBlocks; ++b) {
      const int lap_col  = base_col_ + b * kColsPerBlock + 0;
      const int flux_col = base_col_ + b * kColsPerBlock + 1;

      for (int r = 0; r < kBlockDepth; ++r) {
        const int idx = b * kInputRows + r;

        // ------------------------------------------------------------
        // kernel instances
        // ------------------------------------------------------------
        lap[b][r]  = adf::kernel::create(hdiff_lap);
        flux[b][r] = adf::kernel::create(hdiff_flux);

        adf::source(lap[b][r])  = "ProcessUnit/hdiff_lap.cc";
        adf::source(flux[b][r]) = "ProcessUnit/hdiff_flux.cc";

        adf::runtime<adf::ratio>(lap[b][r])  = 0.90;
        adf::runtime<adf::ratio>(flux[b][r]) = 0.90;

        // ------------------------------------------------------------
        // placement
        // lap : (0,0~5), (2,0~5), ... , (48,0~5)
        // flux: (1,0~5), (3,0~5), ... , (49,0~5)
        // ------------------------------------------------------------
        adf::location<adf::kernel>(lap[b][r])  = adf::tile(lap_col,  base_row_ + r);
        adf::location<adf::kernel>(flux[b][r]) = adf::tile(flux_col, base_row_ + r);

        // ------------------------------------------------------------
        // per-lane dataflow
        // in[idx] -> lap[b][r] -> flux[b][r] -> out[idx]
        // 同时 raw input 还直接送给 flux[b][r].in[0]
        // ------------------------------------------------------------

        // raw rows to lap
        adf::connect<>(in[idx], lap[b][r].in[0]);
        adf::dimensions(lap[b][r].in[0]) = {5 * kRowElems};

        // lap packed output -> flux packed input
        auto net_lap_flux = adf::connect<>(lap[b][r].out[0], flux[b][r].in[1]);
        (void)net_lap_flux;
        adf::dimensions(lap[b][r].out[0])  = {4 * kRowElems};
        adf::dimensions(flux[b][r].in[1])  = {4 * kRowElems};
        // adf::fifo_depth(net_lap_flux) = 2;

        // raw rows to flux
        adf::connect<>(in[idx], flux[b][r].in[0]);
        adf::dimensions(flux[b][r].in[0]) = {5 * kRowElems};

        // flux output -> graph output
        adf::connect<>(flux[b][r].out[0], out[idx]);
        adf::dimensions(flux[b][r].out[0]) = {kRowElems};
        adf::dimensions(out[idx])          = {kRowElems};
      }
    }

#endif
  }

private:
  int base_col_;
  int base_row_;
};