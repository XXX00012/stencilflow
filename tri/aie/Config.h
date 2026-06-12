#pragma once

#include <cstdint>

namespace hdiff_cfg {

constexpr int kTileCol = 7;
constexpr int kLapTileRow = 1;
constexpr int kFlux1TileRow = 2;
constexpr int kFlux2TileRow = 3;

constexpr int kBatchRows = 2;
constexpr int kRowElems = 256;
constexpr int kWindowRows = 5;
constexpr int kWindowMarginRows = 4;
constexpr int kFlux1WindowRows = 3;
constexpr int kFlux1WindowMarginRows = 3;

constexpr int kLapForwardElems = kBatchRows * kRowElems;
constexpr int kFluxInterElems = kBatchRows * 2 * kRowElems;
constexpr int kOutputElems = kBatchRows * kRowElems;

constexpr int kScalarBytes = sizeof(int32_t);
constexpr int kRowBytes = kRowElems * kScalarBytes;
constexpr int kWindowBytes = kWindowRows * kRowBytes;
constexpr int kWindowMarginBytes = kWindowMarginRows * kRowBytes;
constexpr int kFlux1WindowBytes = kFlux1WindowRows * kRowBytes;
constexpr int kFlux1WindowMarginBytes = kFlux1WindowMarginRows * kRowBytes;
constexpr int kFluxInterBytes = kFluxInterElems * kScalarBytes;
constexpr int kOutputBytes = kBatchRows * kRowBytes;

constexpr int kLapInputSampleElems = kBatchRows * kRowElems;
constexpr int kLapInputMarginElems = kWindowMarginRows * kRowElems;
constexpr int kFlux1RawInputSampleElems = kBatchRows * kRowElems;
constexpr int kFlux1RawInputMarginElems = kFlux1WindowMarginRows * kRowElems;

constexpr int kLapWarmupIterations = kWindowRows - 1;


constexpr int kInputObjectFifoDepth = 6;
constexpr int kLapObjectFifoDepth = 5;
constexpr int kFluxInterObjectFifoDepth = 6;
constexpr int kOutputObjectFifoDepth = 2;

constexpr int kDefaultOutputRows = 20;
static_assert(kDefaultOutputRows % kBatchRows == 0);
constexpr int kDefaultIterations = kDefaultOutputRows / kBatchRows;

} 
