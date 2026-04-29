#pragma once

#include <adf.h>
#include "../ProcessUnit/hdiff.h"
#include "Config.h"

class StencilCoreGraph : public adf::graph {
public:
  static constexpr int kInputRows   = 12;
  static constexpr int kBlockDepth  = 12;
  static constexpr int kRowElems    = 256;

  adf::port<input>  in[kInputRows];
  adf::port<output> out[kInputRows];

  adf::kernel hdiff[kBlockDepth];


  StencilCoreGraph(int base_col, int base_row)
      : base_col_(base_col), base_row_(base_row) {

#if defined(__AIESIM__) || defined(__X86SIM__) || defined(__ADF_FRONTEND__)

    // ------------------------------------------------------------
    // kernel instances + placement
    // ------------------------------------------------------------
    for (int r = 0; r < kBlockDepth; ++r) {
      hdiff[r]   = adf::kernel::create(vec_hdiff);
   

      adf::source(hdiff[r])   = "ProcessUnit/vec_hdiff.cc";
     
      adf::runtime<adf::ratio>(hdiff[r])   = 0.90;

    }
    adf::location<adf::kernel>(hdiff[0])   = adf::tile(0,0);
    adf::location<adf::kernel>(hdiff[1])   = adf::tile(0,1);
    adf::location<adf::kernel>(hdiff[2])   = adf::tile(0,2);
    adf::location<adf::kernel>(hdiff[3])   = adf::tile(0,3);
    adf::location<adf::kernel>(hdiff[4])   = adf::tile(0,4);
    adf::location<adf::kernel>(hdiff[5])   = adf::tile(0,5);

    adf::location<adf::kernel>(hdiff[6])   = adf::tile(1,0);
    adf::location<adf::kernel>(hdiff[7])   = adf::tile(1,1);
    adf::location<adf::kernel>(hdiff[8])   = adf::tile(1,2);
    adf::location<adf::kernel>(hdiff[9])   = adf::tile(1,3);  
    adf::location<adf::kernel>(hdiff[10])  = adf::tile(1,4);
    adf::location<adf::kernel>(hdiff[11])  = adf::tile(1,5);


    // ------------------------------------------------------------
    // per-lane dataflow
    // 现在每个 lane 只有两级：
    // in[r] -> lap[r] -> flux[r] -> out[r]
    // ------------------------------------------------------------
    for (int r = 0; r < kBlockDepth; ++r) {
      // raw rows to lap
      adf::connect<>(in[r], hdiff[r].in[0]);
      adf::dimensions(hdiff[r].in[0]) = {5 * kRowElems};

      // lap packed output -> flux packed input
      // adf::connect<>(lap[r].out[0], flux[r].in[1]);
      // adf::dimensions(lap[r].out[0])  = {4*256};
      // adf::dimensions(flux[r].in[1]) = {4*256};

      // raw rows to flux
      // adf::connect<>(in[r], flux[r].in[0]);
      // adf::dimensions(flux[r].in[0]) = {5 * kRowElems};

      // flux packed output -> graph output
      // 注意：这里输出的是原来给 flux2 的中间包，不是 flux2 之后的最终 row
      adf::connect<>(hdiff[r].out[0], out[r]);
      adf::dimensions(hdiff[r].out[0]) = {256};
      adf::dimensions(out[r])          = {256};
    }

#endif
  }

private:
  int base_col_;
  int base_row_;
};