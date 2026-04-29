#pragma once

#include <adf.h>
#include "../ProcessUnit/hdiff.h"
#include "Config.h"

class StencilCoreGraph : public adf::graph {
public:
  static constexpr int kColsUsed     = 50;   // 0 ~ 49
  static constexpr int kRowsPerCol   = 6;    // 0 ~ 5
  static constexpr int kKernelCount  = kColsUsed * kRowsPerCol; // 300
  static constexpr int kInputRows    = kKernelCount;
  static constexpr int kBlockDepth   = kKernelCount;
  static constexpr int kRowElems     = 256;

  adf::port<input>  in[kInputRows];
  adf::port<output> out[kInputRows];

  adf::kernel hdiff[kBlockDepth];

  StencilCoreGraph(int base_col = 0, int base_row = 0)
      : base_col_(base_col), base_row_(base_row) {

#if defined(__AIESIM__) || defined(__X86SIM__) || defined(__ADF_FRONTEND__)

    // ------------------------------------------------------------
    // kernel instances
    // ------------------------------------------------------------
    for (int r = 0; r < kBlockDepth; ++r) {
      hdiff[r] = adf::kernel::create(vec_hdiff);

      adf::source(hdiff[r]) = "ProcessUnit/vec_hdiff.cc";
      adf::runtime<adf::ratio>(hdiff[r]) = 0.90;
    }

    // ------------------------------------------------------------
    // placement:
    // (0,0) ~ (0,5), (1,0) ~ (1,5), ... , (49,0) ~ (49,5)
    // 支持 base_col/base_row 偏移
    // ------------------------------------------------------------
    for (int r = 0; r < kBlockDepth; ++r) {
      int col = base_col_ + (r / kRowsPerCol);
      int row = base_row_ + (r % kRowsPerCol);

      adf::location<adf::kernel>(hdiff[r]) = adf::tile(col, row);
    }

    // ------------------------------------------------------------
    // per-lane dataflow
    // ------------------------------------------------------------
    for (int r = 0; r < kBlockDepth; ++r) {
      adf::connect<>(in[r], hdiff[r].in[0]);
      adf::dimensions(hdiff[r].in[0]) = {5 * kRowElems};

      adf::connect<>(hdiff[r].out[0], out[r]);
      adf::dimensions(hdiff[r].out[0]) = {kRowElems};
      adf::dimensions(out[r])          = {kRowElems};
    }

#endif
  }

private:
  int base_col_;
  int base_row_;
};