
#pragma once

#define VECTORIZED_KERNEL

#include <cstdint>
#include "../Config.h"

using hdiff_data_t = std::int32_t;


#define GRIDROW hdiff_cfg::kRowElems
#define GRIDCOL hdiff_cfg::kRowElems
#define GRIDDEPTH 1
#define TOTAL_INPUT (GRIDROW * GRIDCOL * GRIDDEPTH)

#define ROW hdiff_cfg::kRowElems
#define COL hdiff_cfg::kRowElems
#define TILE_SIZE COL

#define WMARGIN hdiff_cfg::kRowElems
#define NBYTES hdiff_cfg::kScalarBytes

#define AVAIL_CORES (25 * 25)
#define CORE_REQUIRED (TOTAL_INPUT / TILE_SIZE)

#ifdef MULTI_CORE
#ifdef MULTI_2x2
#define HW_ROW 2
#define HW_COL 2
#else
#define HW_ROW 4
#define HW_COL 4
#endif
#define USED_CORE (HW_ROW * HW_COL)
#define NITER (TOTAL_INPUT / (USED_CORE * TILE_SIZE))
#else
#define NITER (TOTAL_INPUT / TILE_SIZE)
#endif

#define INPUT_FILE "./data/dataset_256x256x64.txt"
#define INPUT_STREAM_PREFIX "./data/hdiff"
#define OUTPUT_FILE "./data/TestOutputS.txt"