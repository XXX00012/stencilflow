#pragma once

#include <cstdint>

namespace hdiff_cfg {

constexpr int kTileCol = 7;
constexpr int kLapTileRow = 1;
constexpr int kFlux1TileRow = 2;
constexpr int kFlux2TileRow = 3;

constexpr int kRowsPerCall = 2;   // B: rows processed per kernel run
constexpr int kRowElems = 256;
constexpr int kWindowRows = 5;
constexpr int kWindowMarginRows = 4;
constexpr int kFlux1WindowRows = 3;
constexpr int kFlux1WindowMarginRows = 3;

constexpr int kFluxForwardPackRows = 4;
constexpr int kFluxForwardPackElems = kRowsPerCall * kFluxForwardPackRows * kRowElems;

constexpr int kFluxMaskWordsPerRow = kRowElems / 8;
constexpr int kMaskCompletePackElems = kRowsPerCall * 4 * kFluxMaskWordsPerRow;

constexpr int kScalarBytes = sizeof(int32_t);
constexpr int kRowBytes = kRowElems * kScalarBytes;
constexpr int kWindowBytes = kWindowRows * kRowBytes;
constexpr int kWindowMarginBytes = kWindowMarginRows * kRowBytes;
constexpr int kFlux1WindowBytes = kFlux1WindowRows * kRowBytes;
constexpr int kFlux1WindowMarginBytes = kFlux1WindowMarginRows * kRowBytes;
constexpr int kFluxForwardPackBytes = kFluxForwardPackElems * kScalarBytes;
constexpr int kMaskCompletePackBytes = kMaskCompletePackElems * kScalarBytes;
constexpr int kOutputBytes = kRowsPerCall * kRowBytes;

constexpr int kLapInputSampleElems = kRowsPerCall * kRowElems;
constexpr int kLapInputMarginElems = kWindowMarginRows * kRowElems;
constexpr int kFlux1RawInputSampleElems = kRowsPerCall * kRowElems;
constexpr int kFlux1RawInputMarginElems = kFlux1WindowMarginRows * kRowElems;
constexpr int kFlux2RawInputSampleElems = kRowsPerCall * kRowElems;
constexpr int kFlux2RawInputMarginElems = kFlux1WindowMarginRows * kRowElems;

constexpr int kLapWarmupIterations = kWindowRows - 1;


constexpr int kInputObjectFifoDepth = 2;
constexpr int kDelayedInputObjectFifoDepth = 2;
constexpr int kLapObjectFifoDepth = 5;
constexpr int kFluxInterObjectFifoDepth = 2;
constexpr int kOutputObjectFifoDepth = 2;

constexpr int kDefaultIterations = 512;

} 
