#pragma once

#include <adf.h>
#include <string>
#include "ProcessGraph/StencilCoreGraph.h"

using namespace adf;

#define NUM_IN_PLIOS  (STENCIL_NUM_UNITS * STENCIL_INPUTS_PER_UNIT)
#define NUM_OUT_PLIOS STENCIL_NUM_UNITS
#define NUM_IN_FILES  6

class TopStencilGraph : public graph {
public:
    StencilCoreGraph core;

    input_plio  in_plio [NUM_IN_PLIOS];
    output_plio out_plio[NUM_OUT_PLIOS];

    TopStencilGraph(const std::string& graphID) {
        const std::string base = "./data/";

        for (int i = 0; i < NUM_IN_PLIOS; ++i) {
            const int unit = i / STENCIL_INPUTS_PER_UNIT;
            const int col = unit / STENCIL_UNITS_PER_COL;
            const int fid = col % NUM_IN_FILES;

            in_plio[i] = input_plio::create(
                graphID + "_in" + std::to_string(i),
                plio_32_bits,
                base + "input_plio" + std::to_string(fid) + ".txt"
            );

            connect<>(in_plio[i].out[0], core.in[i]);
        }

        for (int i = 0; i < NUM_OUT_PLIOS; ++i) {
            out_plio[i] = output_plio::create(
                graphID + "_out" + std::to_string(i),
                plio_32_bits,
                base + "TestOutputS_" + std::to_string(i) + ".txt"
            );

            connect<>(core.out[i], out_plio[i].in[0]);
        }
    }
};

extern TopStencilGraph topStencil;
