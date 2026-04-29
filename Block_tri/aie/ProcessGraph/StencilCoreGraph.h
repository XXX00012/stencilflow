#pragma once

#include <adf.h>
#include "../ProcessUnit/hdiff.h"
#include "Config.h"
using namespace hdiff_cfg;
using namespace adf;

class StencilCoreGraph : public adf::graph {
public:
  static constexpr int kInputRows   = 4;
  static constexpr int kBlockDepth  = 4;
  static constexpr int kRowElems    = 256;

  adf::port<input>  in[kInputRows];
  adf::port<output> out[kInputRows];

  adf::kernel lap[kBlockDepth];
  adf::kernel flux1[kBlockDepth];
  adf::kernel flux2[kBlockDepth];

  StencilCoreGraph(int base_col, int base_row)
      : base_col_(base_col), base_row_(base_row) {

#if defined(__AIESIM__) || defined(__X86SIM__) || defined(__ADF_FRONTEND__)

    // ------------------------------------------------------------
    // kernel instances + placement
    // ------------------------------------------------------------
    for (int r = 0; r < kBlockDepth; ++r) {
      lap[r]   = adf::kernel::create(hdiff_lap);
      flux1[r] = adf::kernel::create(hdiff_flux1);
      flux2[r] = adf::kernel::create(hdiff_flux2);

      adf::source(lap[r])   = "ProcessUnit/hdiff_lap.cc";
      adf::source(flux1[r]) = "ProcessUnit/hdiff_flux1.cc";
      adf::source(flux2[r]) = "ProcessUnit/hdiff_flux2.cc";

      adf::runtime<adf::ratio>(lap[r])   = 0.90;
      adf::runtime<adf::ratio>(flux1[r]) = 0.90;
      adf::runtime<adf::ratio>(flux2[r]) = 0.90;

      adf::location<adf::kernel>(lap[r])   = adf::tile(base_col_ + 0, base_row_ + r);
      adf::location<adf::kernel>(flux1[r]) = adf::tile(base_col_ + 1, base_row_ + r);
      adf::location<adf::kernel>(flux2[r]) = adf::tile(base_col_ + 2, base_row_ + r);
    }

    // ------------------------------------------------------------
    // per-lane dataflow
    // 每个 lane 独立：in[r] -> lap[r] / flux1[r] -> flux2[r] -> out[r]
    // ------------------------------------------------------------
    for (int r = 0; r < kBlockDepth; ++r) {
      // raw rows to lap
      adf::connect<>(in[r], lap[r].in[0]);
      adf::dimensions(lap[r].in[0]) = {5 * kRowElems};

      // lap packed output -> flux1 packed input
      auto net_lap_flux1 = adf::connect<>(lap[r].out[0], flux1[r].in[1]);
      adf::dimensions(lap[r].out[0])  = {hdiff_cfg::kFluxForwardPackElems};
      adf::dimensions(flux1[r].in[1]) = {hdiff_cfg::kFluxForwardPackElems};
      // fifo_depth(net_lap_flux1) = 2;

      // raw rows to flux1
      adf::connect<>(in[r], flux1[r].in[0]);
      adf::dimensions(flux1[r].in[0]) = {5 * kRowElems};

      // flux1 packed output -> flux2 packed input
      auto net_flux1_flux2 = adf::connect<>(flux1[r].out[0], flux2[r].in[0]);
      adf::dimensions(flux1[r].out[0]) = {hdiff_cfg::kFluxInterPackElems};
      adf::dimensions(flux2[r].in[0])  = {hdiff_cfg::kFluxInterPackElems};
      // fifo_depth(net_flux1_flux2) = 2;

      // flux2 per-lane output -> per-lane graph output
      adf::connect<>(flux2[r].out[0], out[r]);
      adf::dimensions(flux2[r].out[0]) = {kRowElems};
      adf::dimensions(out[r])          = {kRowElems};
    }

#endif
  }

private:
  int base_col_;
  int base_row_;
};