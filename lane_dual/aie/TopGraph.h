#pragma once

#include <adf.h>
#include <string>
#include "ProcessGraph/StencilCoreGraph.h"

using namespace adf;

class TopStencilGraph : public graph {
public:
    std::string graphID;
    StencilCoreGraph core;

    input_plio  in_plio[5];
    output_plio out_plio[1];

    TopStencilGraph(const std::string& graphID) {
        const std::string base = "./data/";

        in_plio[0] = input_plio::create(
                graphID + "_in" + std::to_string(0),
                plio_32_bits,
                base + "input_plio" + std::to_string(0) + ".txt"
            );

            connect<>(in_plio[0].out[0], core.in[0]);

        in_plio[1] = input_plio::create(
                graphID + "_in" + std::to_string(1),
                plio_32_bits,
                base + "input_plio" + std::to_string(1) + ".txt"
            );
            connect<>(in_plio[1].out[0], core.in[1]);

        in_plio[2] = input_plio::create(
                graphID + "_in" + std::to_string(2),
                plio_32_bits,       
                base + "input_plio" + std::to_string(2) + ".txt"
        );
            connect<>(in_plio[2].out[0], core.in[2]);

        in_plio[3] = input_plio::create(
                graphID + "_in" + std::to_string(3),
                plio_32_bits,       
                base + "input_plio" + std::to_string(3) + ".txt"
        );
            connect<>(in_plio[3].out[0], core.in[3]);

        in_plio[4] = input_plio::create(
                graphID + "_in" + std::to_string(4),
                plio_32_bits,       
                base + "input_plio" + std::to_string(4) + ".txt"
        );
            connect<>(in_plio[4].out[0], core.in[4]);

        out_plio[0] = output_plio::create(
                graphID + "_out" + std::to_string(0),
                plio_32_bits,       
                base + "TestOutputS_" + std::to_string(0) + ".txt"
        );
            connect<>(core.out, out_plio[0].in[0]);
    }
};