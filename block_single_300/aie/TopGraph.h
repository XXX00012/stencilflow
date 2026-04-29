#pragma once

#include <adf.h>
#include <string>
#include "ProcessGraph/StencilCoreGraph.h"

using namespace adf;

class TopStencilGraph : public graph {
public:
    static constexpr int kPhysIO      = 16;
    static constexpr int kFullGroups  = 15;
    static constexpr int kWaysFull    = 19;
    static constexpr int kWaysLast    = 15;
    static constexpr int kRowElems    = StencilCoreGraph::kRowElems;

    // 300 个计算核，整体放到 (0,2) ~ (49,7)
    StencilCoreGraph core;

    // 16 个物理输入 / 输出
    input_plio  in_plio[kPhysIO];
    output_plio out_plio[kPhysIO];

    // 前 15 组：1 -> 19, 19 -> 1
    pktsplit<kWaysFull> in_split[kFullGroups];
    pktmerge<kWaysFull> out_merge[kFullGroups];

    // 最后 1 组输入：改成“1 份输入广播给 15 个 lane”
    pktmerge<1>         in_merge_last_bcast;
    pktsplit<1>         in_split_last_bcast;

    // 最后 1 组输出：15 -> 1
    pktmerge<kWaysLast> out_merge_last;

    TopStencilGraph(const std::string& graphID)
        : core(0, 2)   // 计算核放到 row 2~7
    {
        const std::string base = "./data/";

        // 你想用的 8 个 shim 列
        // 16 个 PLIO：前 8 个用 channel 0，后 8 个用 channel 1
        constexpr int shim_cols[8] = {12, 13, 14, 15, 34, 35, 36, 37};

        // ------------------------------------------------------------
        // 创建 16 个物理 PLIO
        // ------------------------------------------------------------
        for (int i = 0; i < kPhysIO; ++i) {
            const int col = shim_cols[i % 8];
            const int ch  = i / 8;   // 0 or 1

            in_plio[i] = input_plio::create(
                graphID + "_in" + std::to_string(i),
                plio_32_bits,
                base + "input_plio_" + std::to_string(i) + ".txt"
            );

            out_plio[i] = output_plio::create(
                graphID + "_out" + std::to_string(i),
                plio_32_bits,
                base + "output_plio" + std::to_string(i) + ".txt"
            );

            location<PLIO>(in_plio[i])  = shim(col, ch);
            location<PLIO>(out_plio[i]) = shim(col, ch);
        }

        // ------------------------------------------------------------
        // 前 15 组：每组 19 个 lane，各吃各的数据
        // group g -> lane [g*19, g*19+18]
        // ------------------------------------------------------------
        for (int g = 0; g < kFullGroups; ++g) {
            in_split[g]  = pktsplit<kWaysFull>::create();
            out_merge[g] = pktmerge<kWaysFull>::create();

            connect<>(in_plio[g].out[0],   in_split[g].in[0]);
            connect<>(out_merge[g].out[0], out_plio[g].in[0]);

            for (int j = 0; j < kWaysFull; ++j) {
                const int lane = g * kWaysFull + j;   // 0..284

                connect<>(in_split[g].out[j], core.in[lane]);
                connect<>(core.out[lane],     out_merge[g].in[j]);

                dimensions(core.in[lane])  = {5 * kRowElems};
                dimensions(core.out[lane]) = {kRowElems};
            }
        }

        // ------------------------------------------------------------
        // 最后 1 组：lane [285..299]
        //
        // 目标：
        //   15 个 lane 共用“同一份输入”
        //
        // 做法：
        //   in_plio[15] --> pktmerge<1> --> pktsplit<1>
        //   然后把 in_split_last_bcast.out[0] 广播到 15 个 core.in[lane]
        //
        // 这利用的是官方支持的
        //   "pktmerge -> pktsplit 下，pktsplit.out[i] 可广播到多个目的地"
        // ------------------------------------------------------------
        in_merge_last_bcast = pktmerge<1>::create();
        in_split_last_bcast = pktsplit<1>::create();
        out_merge_last      = pktmerge<kWaysLast>::create();

        connect<>(in_plio[15].out[0],        in_merge_last_bcast.in[0]);
        connect<>(in_merge_last_bcast.out[0], in_split_last_bcast.in[0]);
        connect<>(out_merge_last.out[0],     out_plio[15].in[0]);

        for (int j = 0; j < kWaysLast; ++j) {
            const int lane = kFullGroups * kWaysFull + j;   // 285..299

            // 15 个 lane 全都吃同一个广播输入
            connect<>(in_split_last_bcast.out[0], core.in[lane]);

            // 输出仍然各自产生，再合并到同一个物理输出
            connect<>(core.out[lane], out_merge_last.in[j]);

            dimensions(core.in[lane])  = {5 * kRowElems};
            dimensions(core.out[lane]) = {kRowElems};
        }
    }
};

extern TopStencilGraph topStencil;