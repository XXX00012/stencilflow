#pragma once

#include <adf.h>
#include <string>
#include "ProcessGraph/StencilCoreGraph.h"

using namespace adf;

class TopStencilGraph : public graph {
public:
    // 这里必须和新的 StencilCoreGraph 对齐
    static constexpr int kInputCount  = StencilCoreGraph::kInputCount;
    static constexpr int kOutputCount = StencilCoreGraph::kOutputCount;

    TopStencilGraph(const std::string& graphID)
        : core(0, 1)
    {
        const std::string base = "./data/";

        for (int i = 0; i < kInputCount; ++i) {
            // 只循环使用 0~5 这 6 个输入文件
            const int file_idx = i % StencilCoreGraph::kInputRows;

            in_plio[i] = input_plio::create(
                graphID + "_in" + std::to_string(i),   // PLIO 名字仍然唯一
                plio_32_bits,
                base + "input_plio" + std::to_string(file_idx) + ".txt"
            );

            connect<>(in_plio[i].out[0], core.in[i]);
        }

        // 输出仍然一一对应，各自写各自的文件
        for (int i = 0; i < kOutputCount; ++i) {
            out_plio[i] = output_plio::create(
                graphID + "_out" + std::to_string(i),
                plio_32_bits,
                base + "TestOutputS_" + std::to_string(i) + ".txt"
            );

            connect<>(core.out[i], out_plio[i].in[0]);
        }
    }

    StencilCoreGraph core;
    input_plio  in_plio[kInputCount];
    output_plio out_plio[kOutputCount];
};

extern TopStencilGraph topStencil;