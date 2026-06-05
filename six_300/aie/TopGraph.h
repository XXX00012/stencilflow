#pragma once

#include <adf.h>
#include <string>
#include "ProcessGraph/StencilCoreGraph.h"

using namespace adf;

#define NUM_COLS      STENCIL_NUM_COLS
#define NUM_IN_PLIOS  NUM_COLS
#define NUM_OUT_PLIOS STENCIL_NUM_UNITS
#define NUM_IN_FILES  6

class TopStencilGraph : public graph {
public:
    StencilCoreGraph core;

    input_plio  in_plio [NUM_IN_PLIOS];
    output_plio out_plio[NUM_OUT_PLIOS];

    TopStencilGraph(const std::string& graphID) {
        const std::string base = "./data/";

        for (int c = 0; c < NUM_COLS; ++c) {
            const int fid = c % NUM_IN_FILES;

            in_plio[c] = input_plio::create(
                graphID + "_in_c" + std::to_string(c),
                plio_32_bits,
                base + "input_plio" + std::to_string(fid) + ".txt"
            );

            for (int u = 0; u < STENCIL_UNITS_PER_COL; ++u) {
                const int idx = c * STENCIL_UNITS_PER_COL + u;
                const int in_base = idx * STENCIL_INPUTS_PER_UNIT;

                for (int p = 0; p < STENCIL_INPUTS_PER_UNIT; ++p) {
                    connect<>(in_plio[c].out[0], core.in[in_base + p]);
                }
            }
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
