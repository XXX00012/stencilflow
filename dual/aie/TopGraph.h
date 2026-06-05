#pragma once

#include <adf.h>
#include <string>
#include "ProcessGraph/StencilCoreGraph.h"

using namespace adf;

class TopStencilGraph : public graph {
public:
    StencilCoreGraph core;

    input_plio  in_plio[StencilCoreGraph::kInputCount];
    output_plio out_plio[1];

    TopStencilGraph(const std::string& graphID) {
        const std::string base = "./data/";

        for (int i = 0; i < StencilCoreGraph::kInputCount; ++i) {
            in_plio[i] = input_plio::create(
                graphID + "_in" + std::to_string(i),
                plio_32_bits,
                base + "input_plio0.txt"
            );

            connect<>(in_plio[i].out[0], core.in[i]);
        }

        out_plio[0] = output_plio::create(
            graphID + "_out0",
            plio_32_bits,
            base + "TestOutputS.txt"
        );

        connect<>(core.out, out_plio[0].in[0]);
    }
};

extern TopStencilGraph topStencil;
