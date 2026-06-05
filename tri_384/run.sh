#!/bin/bash
set -e

export XILINX_XRT=/usr

XCLBIN=${1:-svd.xclbin}
ITER=${2:-132}
INPUT_SRC=${3:-./board_data/input.txt}
OUTPUT_TXT=${4:-./aie_out_pl.txt}

if [ ! -f "$XCLBIN" ] && [ -f "a.xclbin" ]; then
  XCLBIN="a.xclbin"
fi

echo "[run.sh] XCLBIN       = $XCLBIN"
echo "[run.sh] ITER         = $ITER"
echo "[run.sh] INPUT_SRC    = $INPUT_SRC"
echo "[run.sh] OUTPUT_TXT   = $OUTPUT_TXT"

./host.exe "$XCLBIN" "$ITER" "$INPUT_SRC" "$OUTPUT_TXT"
