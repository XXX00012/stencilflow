#pragma once

#include <adf.h>
#include "../ProcessUnit/hdiff.h"
#include "Config.h"
using namespace adf;
class StencilCoreGraph : public adf::graph {
public:
  static constexpr int kInputRows   = 6;
  static constexpr int kBlockDepth  = 6;
  static constexpr int kRowElems    = 256;

  adf::port<input>  in[kInputRows];
  adf::port<output> out[kInputRows];

  adf::kernel lap[kBlockDepth];
  adf::kernel flux[kBlockDepth];

  StencilCoreGraph(int base_col, int base_row)
      : base_col_(base_col), base_row_(base_row) {

#if defined(__AIESIM__) || defined(__X86SIM__) || defined(__ADF_FRONTEND__)

    // ------------------------------------------------------------
    // kernel instances + placement
    // ------------------------------------------------------------
    for (int r = 0; r < kBlockDepth; ++r) {
      lap[r]   = adf::kernel::create(hdiff_lap);
      flux[r] = adf::kernel::create(hdiff_flux);

      adf::source(lap[r])   = "ProcessUnit/hdiff_lap.cc";
      adf::source(flux[r]) = "ProcessUnit/hdiff_flux.cc";

      adf::runtime<adf::ratio>(lap[r])   = 0.90;
      adf::runtime<adf::ratio>(flux[r]) = 0.90;

      adf::location<adf::kernel>(lap[r])   = adf::tile(base_col_ + 0, base_row_ + r);
      adf::location<adf::kernel>(flux[r]) = adf::tile(base_col_ + 1, base_row_ + r);
    }

    // ------------------------------------------------------------
    // per-lane dataflow
    // 现在每个 lane 只有两级：
    // in[r] -> lap[r] -> flux[r] -> out[r]
    // ------------------------------------------------------------
    for (int r = 0; r < kBlockDepth; ++r) {
      // raw rows to lap
      adf::connect<>(in[r], lap[r].in[0]);
      adf::dimensions(lap[r].in[0]) = {5 * kRowElems};

      // lap packed output -> flux packed input
      auto net_lap_flux = adf::connect<>(lap[r].out[0], flux[r].in[1]);
      adf::dimensions(lap[r].out[0])  = {4*256};
      adf::dimensions(flux[r].in[1]) = {4*256};
      fifo_depth(net_lap_flux) = 4;


      // raw rows to flux
      adf::connect<>(in[r], flux[r].in[0]);
      adf::dimensions(flux[r].in[0]) = {5 * kRowElems};

      // flux packed output -> graph output
      // 注意：这里输出的是原来给 flux2 的中间包，不是 flux2 之后的最终 row
      adf::connect<>(flux[r].out[0], out[r]);
      adf::dimensions(flux[r].out[0]) = {256};
      adf::dimensions(out[r])          = {256};
    }

#endif
  }

private:
  int base_col_;
  int base_row_;
};
