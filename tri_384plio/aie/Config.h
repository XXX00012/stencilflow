#pragma once

#include <cstdint>

namespace hdiff_cfg {

constexpr int kNumLanes = 128;
constexpr int kKernelsPerLane = 3;
constexpr int kTotalCores = kNumLanes * kKernelsPerLane;

constexpr int kRowElems = 256;
constexpr int kWindowRows = 5;
constexpr int kWindowMarginRows = 4;
constexpr int kFlux1WindowRows = 3;
constexpr int kFlux1WindowMarginRows = 3;

constexpr int kFluxForwardPackRows = 4;
constexpr int kFluxForwardPackElems = kFluxForwardPackRows * kRowElems;

constexpr int kFluxMaskWordsPerRow = kRowElems / 8;
constexpr int kMaskCompletePackElems = 4 * kFluxMaskWordsPerRow;

constexpr int kScalarBytes = sizeof(int32_t);
constexpr int kRowBytes = kRowElems * kScalarBytes;
constexpr int kWindowBytes = kWindowRows * kRowBytes;
constexpr int kWindowMarginBytes = kWindowMarginRows * kRowBytes;
constexpr int kFlux1WindowBytes = kFlux1WindowRows * kRowBytes;
constexpr int kFlux1WindowMarginBytes = kFlux1WindowMarginRows * kRowBytes;
constexpr int kFluxForwardPackBytes = kFluxForwardPackElems * kScalarBytes;
constexpr int kMaskCompletePackBytes = kMaskCompletePackElems * kScalarBytes;
constexpr int kOutputBytes = kRowBytes;

constexpr int kLapInputSampleElems = kRowElems;
constexpr int kLapInputMarginElems = kWindowMarginRows * kRowElems;
constexpr int kFlux1RawInputSampleElems = kRowElems;
constexpr int kFlux1RawInputMarginElems = kFlux1WindowMarginRows * kRowElems;
constexpr int kFlux2RawInputSampleElems = kRowElems;
constexpr int kFlux2RawInputMarginElems = kFlux1WindowMarginRows * kRowElems;

constexpr int kLapWarmupIterations = kWindowRows - 1;

constexpr int kInputObjectFifoDepth = 6;
constexpr int kLapObjectFifoDepth = 5;
constexpr int kFluxInterObjectFifoDepth = 6;
constexpr int kOutputObjectFifoDepth = 2;

// 128 useful output rows per lane plus 4 warmup/halo rows.
constexpr int kDefaultIterations = 132;

}
