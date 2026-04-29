#pragma once

#include <adf.h>
#include <string>
#include "ProcessGraph/StencilCoreGraph.h"

using namespace adf;

class TopStencilGraph : public graph {
public:
    static constexpr int kInputCount  = StencilCoreGraph::kInputCount;
    static constexpr int kOutputCount = StencilCoreGraph::kOutputCount;

    std::string graphID;
    StencilCoreGraph core;

    input_plio  in_plio[kInputCount];
    output_plio out_plio[kOutputCount];

    TopStencilGraph(const std::string& id = "stencil")
        : graphID(id), core(0, 1)
    {
        const std::string base = "./data/";

        // in_plio[0]: lap input,  5 * COL ints
        in_plio[0] = input_plio::create(
            graphID + "_lap_in",
            plio_32_bits,
            base + "input_plio0.txt"
        );
        connect<>(in_plio[0].out[0], core.in[0]);

        // in_plio[1]: flux input, 3 * COL ints
        in_plio[1] = input_plio::create(
            graphID + "_flux_in",
            plio_32_bits,
            base + "input_plio1.txt"
        );
        connect<>(in_plio[1].out[0], core.in[1]);

        // out_plio[0]: final output, COL ints
        out_plio[0] = output_plio::create(
            graphID + "_out0",
            plio_32_bits,
            base + "TestOutputS_0.txt"
        );
        connect<>(core.out[0], out_plio[0].in[0]);
    }
};

extern TopStencilGraph topStencil;