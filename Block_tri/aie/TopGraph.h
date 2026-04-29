#pragma once

#include <adf.h>
#include <string>
#include "ProcessGraph/StencilCoreGraph.h"

using namespace adf;

class TopStencilGraph : public graph {
public:
    static constexpr int kInputCount  = StencilCoreGraph::kInputRows;
    static constexpr int kOutputCount = StencilCoreGraph::kInputRows;

    StencilCoreGraph core;

    input_plio  in_plio[kInputCount];
    output_plio out_plio[kOutputCount];

    TopStencilGraph(const std::string& graphID)
        : core(0, 1)
    {
        const std::string base = "./data/";

        for (int i = 0; i < kInputCount; ++i) {
            in_plio[i] = input_plio::create(
                graphID + "_in" + std::to_string(i),
                plio_32_bits,
                base + "input_plio" + std::to_string(i) + ".txt"
            );

            connect<>(in_plio[i].out[0], core.in[i]);
        }

        for (int i = 0; i < kOutputCount; ++i) {
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