#pragma once

#include <adf.h>
#include "../ProcessUnit/include.h"
#include "../ProcessUnit/hdiff.h"

using namespace adf;

class StencilCoreGraph : public adf::graph {
public:
  static constexpr int kInputCount   = 2;   // lap raw + flux raw
  static constexpr int kOutputCount  = 1;   // final output
  static constexpr int kLapRows      = 5;
  static constexpr int kFluxRows     = 3;
  static constexpr int kForwardRows  = 4;
  static constexpr int kRowElems     = COL;

  adf::port<input>  in[kInputCount];
  adf::port<output> out[kOutputCount];

  adf::kernel lap;
  adf::kernel flux;

  StencilCoreGraph(int base_col, int base_row)
      : base_col_(base_col), base_row_(base_row) {

#if defined(__AIESIM__) || defined(__X86SIM__) || defined(__ADF_FRONTEND__)

    // ------------------------------------------------------------
    // kernel instances + placement
    // ------------------------------------------------------------
    lap  = adf::kernel::create(hdiff_lap);
    flux = adf::kernel::create(hdiff_flux);

    adf::source(lap)  = "ProcessUnit/hdiff_lap.cc";
    adf::source(flux) = "ProcessUnit/hdiff_flux.cc";

    adf::headers(lap)  = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h"};
    adf::headers(flux) = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h"};

    adf::runtime<adf::ratio>(lap)  = 0.90;
    adf::runtime<adf::ratio>(flux) = 0.90;

    adf::location<adf::kernel>(lap)  = adf::tile(base_col_ + 0, base_row_ + 0);
    adf::location<adf::kernel>(flux) = adf::tile(base_col_ + 1, base_row_ + 0);

    // ------------------------------------------------------------
    // single-lane dataflow
    // in[0] -> lap -> flux -> out[0]
    // in[1] ----------^
    // ------------------------------------------------------------

    // raw rows to lap: 5 * COL
    adf::connect<>(in[0], lap.in[0]);
    adf::dimensions(in[0])     = {kLapRows * kRowElems};
    adf::dimensions(lap.in[0]) = {kLapRows * kRowElems};

    // lap packed output -> flux packed input: 4 * COL
    auto net_lap_flux = adf::connect<>(lap.out[0], flux.in[1]);
    adf::dimensions(lap.out[0])  = {kForwardRows * kRowElems};
    adf::dimensions(flux.in[1])  = {kForwardRows * kRowElems};
    adf::fifo_depth(net_lap_flux) = 6;

    // raw rows to flux: 3 * COL
    adf::connect<>(in[1], flux.in[0]);
    adf::dimensions(in[1])      = {kFluxRows * kRowElems};
    adf::dimensions(flux.in[0]) = {kFluxRows * kRowElems};

    // flux final output -> graph output: COL
    adf::connect<>(flux.out[0], out[0]);
    adf::dimensions(flux.out[0]) = {kRowElems};
    adf::dimensions(out[0])      = {kRowElems};

#endif
  }

private:
  int base_col_;
  int base_row_;
};