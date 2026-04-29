#pragma once

#include <adf.h>
#include "../ProcessUnit/include.h"
#include "../Config.h"
#include "../ProcessUnit/hdiff.h"

using namespace adf;

class StencilCoreGraph : public graph {
public:
    port<input>  in;
    port<output> out;

    kernel k_lap;
    kernel k_flux1;
    kernel k_flux2;

    StencilCoreGraph() {
#if defined(__AIESIM__) || defined(__X86SIM__) || defined(__ADF_FRONTEND__)
        k_lap   = kernel::create(hdiff_lap);
        k_flux1 = kernel::create(hdiff_flux1);
        k_flux2 = kernel::create(hdiff_flux2);

        source(k_lap)   = "ProcessUnit/hdiff_lap.cc";
        source(k_flux1) = "ProcessUnit/hdiff_flux1.cc";
        source(k_flux2) = "ProcessUnit/hdiff_flux2.cc";

        headers(k_lap)   = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
        headers(k_flux1) = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};
        headers(k_flux2) = {"ProcessUnit/hdiff.h", "ProcessUnit/include.h", "Config.h"};

        runtime<ratio>(k_lap)   = 0.9;
        runtime<ratio>(k_flux1) = 0.9;
        runtime<ratio>(k_flux2) = 0.9;

        location<kernel>(k_lap)   = tile(hdiff_cfg::kTileCol, hdiff_cfg::kLapTileRow);
        location<kernel>(k_flux1) = tile(hdiff_cfg::kTileCol, hdiff_cfg::kFlux1TileRow);
        location<kernel>(k_flux2) = tile(hdiff_cfg::kTileCol, hdiff_cfg::kFlux2TileRow);

        auto net_in_lap   = connect(in, k_lap.in[0]);
        auto net_in_flux1 = connect(in, k_flux1.in[0]);

        dimensions(k_lap.in[0])   = {hdiff_cfg::kLapInputSampleElems};
        dimensions(k_flux1.in[0]) = {hdiff_cfg::kFlux1RawInputSampleElems};
      


        auto net_lap_f1 = connect(k_lap.out[0], k_flux1.in[1]);

        dimensions(k_lap.out[0])  = {hdiff_cfg::kFluxForwardPackElems};
        dimensions(k_flux1.in[1]) = {hdiff_cfg::kFluxForwardPackElems};



        auto net_f1_f2 = connect(k_flux1.out[0], k_flux2.in[0]);

        dimensions(k_flux1.out[0]) = {hdiff_cfg::kFluxInterPackElems};
        dimensions(k_flux2.in[0])  = {hdiff_cfg::kFluxInterPackElems};


        auto net_out = connect(k_flux2.out[0], out);

        dimensions(k_flux2.out[0]) = {COL};
  
#endif
    }
};
