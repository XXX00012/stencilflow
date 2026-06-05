#!/bin/bash
set -e

export XILINX_XRT=/usr

XCLBIN=${1:-svd.xclbin}
ITER=${2:-2}
INPUT_STREAM=${3:-./data/input_plio.txt}
OUTPUT_TXT=${4:-./data/aie_out_gmio.txt}

if [ ! -f "$XCLBIN" ] && [ -f "a.xclbin" ]; then
  XCLBIN="a.xclbin"
fi

echo "[run.sh] XCLBIN       = $XCLBIN"
echo "[run.sh] ITER         = $ITER"
echo "[run.sh] INPUT_STREAM = $INPUT_STREAM"
echo "[run.sh] OUTPUT_TXT   = $OUTPUT_TXT"

./host.exe "$XCLBIN" "$ITER" "$INPUT_STREAM" "$OUTPUT_TXT"
