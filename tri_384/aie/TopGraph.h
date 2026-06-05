#pragma once

#include <adf.h>
#include <string>
#include "ProcessGraph/StencilCoreGraph.h"

using namespace adf;

#define NUM_IN_PLIOS STENCIL_NUM_UNITS
#define NUM_OUT_PLIOS STENCIL_NUM_UNITS

class TopStencilGraph : public graph {
public:
    static constexpr int kInputPlioCount = NUM_IN_PLIOS;
    static constexpr int kOutputPlioCount = NUM_OUT_PLIOS;

    StencilCoreGraph core;

    input_plio  in_plio[kInputPlioCount];
    output_plio out_plio[kOutputPlioCount];

    TopStencilGraph(const std::string& graphID) {
        const std::string base = "./data/";

        for (int lane = 0; lane < STENCIL_NUM_UNITS; ++lane) {
            in_plio[lane] = input_plio::create(
                graphID + "_in" + std::to_string(lane),
                plio_64_bits,
                base + "input_plio" + std::to_string(lane) + ".txt");

            for (int i = 0; i < StencilCoreGraph::kInputCount; ++i) {
                connect<>(in_plio[lane].out[0],
                          core.in[StencilCoreGraph::input_index(lane, i)]);
            }

            out_plio[lane] = output_plio::create(
                graphID + "_out" + std::to_string(lane),
                plio_64_bits,
                base + "TestOutputS_" + std::to_string(lane) + ".txt");

            connect<>(core.out[lane], out_plio[lane].in[0]);
        }
    }
};

extern TopStencilGraph topStencil;
