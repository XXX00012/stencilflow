
#pragma once
#define VECTORIZED_KERNEL
#include <stdint.h>
#include <cstdint>
#include "../Config.h"
#define GRIDROW 256
#define GRIDCOL 256
#define GRIDDEPTH 1
#define TOTAL_INPUT GRIDROW *GRIDCOL *GRIDDEPTH

#define ROW 256

#define TILE_SIZE COL

#define WMARGIN 256

#define NBYTES 4 

#define AVAIL_CORES 25 * 25

#define CORE_REQUIRED TOTAL_INPUT / TILE_SIZE

#ifdef MULTI_CORE
#ifdef MULTI_2x2
#define HW_ROW 2
#define HW_COL 2
#else
#define HW_ROW 4
#define HW_COL 4
#endif

#define USED_CORE HW_ROW *HW_COL
#define NITER TOTAL_INPUT / (USED_CORE * TILE_SIZE) 
#else

#define NITER TOTAL_INPUT / (TILE_SIZE) 
#endif

#ifdef WITH_MARGIN
#define INPUT_FILE "../../data/TestInputS.txt"
#else
#define INPUT_FILE "../../data/input.txt"
#endif

#define OUTPUT_FILE "../../data/TestOutputS.txt"
