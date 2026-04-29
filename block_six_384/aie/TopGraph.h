#pragma once

#include <adf.h>
#include <string>
#include "ProcessGraph/StencilCoreGraph.h"

using namespace adf;

class TopStencilGraph : public graph {
public:
    static constexpr int kBlockCount      = StencilCoreGraph::kNumBlocks;         // 64
    static constexpr int kInputsPerBlock  = StencilCoreGraph::kInputsPerBlock;    // 3
    static constexpr int kOutputCount     = kBlockCount;                          // 64
    static constexpr int kInputPlioCount  = kBlockCount * kInputsPerBlock;        // 192
    static constexpr int kInputFileCount  = 6;   // 仍然只复用 0~5 这 6 个输入文件

    StencilCoreGraph core;

    input_plio  in_plio[kInputPlioCount];
    output_plio out_plio[kOutputCount];

    TopStencilGraph(const std::string& graphID)
        : core()
    {
        const std::string base = "./data/";

        // ------------------------------------------------------------
        // 输入 PLIO:
        // 一共创建 192 路 input_plio，但文件名按 0~5 循环复用
        //
        // 即：
        //   in0   -> input_plio0.txt
        //   in1   -> input_plio1.txt
        //   ...
        //   in5   -> input_plio5.txt
        //   in6   -> input_plio0.txt
        //   in7   -> input_plio1.txt
        //   ...
        //   in191 -> input_plio5.txt
        // ------------------------------------------------------------
        for (int i = 0; i < kInputPlioCount; ++i) {
            const int file_idx = i % kInputFileCount;

            in_plio[i] = input_plio::create(
                graphID + "_in" + std::to_string(i),
                plio_32_bits,
                base + "input_plio" + std::to_string(file_idx) + ".txt"
            );

            connect<>(in_plio[i].out[0], core.in[i]);
        }

        // ------------------------------------------------------------
        // 输出 PLIO:
        // 一共创建 64 路 output_plio
        // 每个 block 独立输出到一个文件：
        //   out0  -> TestOutputS_0.txt
        //   ...
        //   out63 -> TestOutputS_63.txt
        // ------------------------------------------------------------
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