#pragma once

#include <adf.h>
#include <string>
#include "ProcessGraph/StencilCoreGraph.h"

using namespace adf;

class TopStencilGraph : public graph {
public:
    StencilCoreGraph core;

    input_plio  in_plio[3];
    output_plio out_plio[1];

    TopStencilGraph(const std::string& graphID) {
        const std::string base = "./data/";

        // The three inputs intentionally read the same row stream.
        // They correspond to lap/raw-product/update-center consumers.
        in_plio[0] = input_plio::create(
            graphID + "_in0",
            plio_32_bits,
            base + "input_plio.txt"
        );

        in_plio[1] = input_plio::create(
            graphID + "_in1",
            plio_32_bits,
            base + "input_plio.txt"
        );

        in_plio[2] = input_plio::create(
            graphID + "_in2",
            plio_32_bits,
            base + "input_plio.txt"
        );

        out_plio[0] = output_plio::create(
            graphID + "_out0",
            plio_32_bits,
            base + "TestOutputS.txt"
        );

        connect<>(in_plio[0].out[0], core.in[0]);
        connect<>(in_plio[1].out[0], core.in[1]);
        connect<>(in_plio[2].out[0], core.in[2]);
        connect<>(core.out, out_plio[0].in[0]);
    }
};

extern TopStencilGraph topStencil;
