#pragma once

#include <adf.h>
#include <string>
#include "ProcessGraph/StencilCoreGraph.h"

using namespace adf;

#define NUM_IN_PLIOS  (STENCIL_NUM_LANES * STENCIL_INPUTS_PER_LANE)
#define NUM_OUT_PLIOS STENCIL_NUM_LANES

class TopStencilGraph : public graph {
public:
    StencilCoreGraph core;

    input_plio  in_plio [NUM_IN_PLIOS];
    output_plio out_plio[NUM_OUT_PLIOS];

    TopStencilGraph(const std::string& graphID) {
        const std::string base = "./data/";

        for (int i = 0; i < NUM_OUT_PLIOS; ++i) {
            const int in_base = i * STENCIL_INPUTS_PER_LANE;

            for (int p = 0; p < STENCIL_INPUTS_PER_LANE; ++p) {
                const int plio_idx = in_base + p;

                in_plio[plio_idx] = input_plio::create(
                    graphID + "_in" + std::to_string(plio_idx),
                    plio_32_bits,
                    base + "input_plio.txt"
                );

                connect<>(in_plio[plio_idx].out[0], core.in[plio_idx]);
            }

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
