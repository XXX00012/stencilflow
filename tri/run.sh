#!/bin/bash
set -e

export XILINX_XRT=/usr

XCLBIN=${1:-svd.xclbin}
ITER=${2:-10}
INPUT_TXT=${3:-./data/input.txt}
OUTPUT_TXT=${4:-./data/aie_out_plio.txt}

if [ ! -f "$XCLBIN" ] && [ -f "a.xclbin" ]; then
  XCLBIN="a.xclbin"
fi

./host.exe "$XCLBIN" "$ITER" "$INPUT_TXT" "$OUTPUT_TXT"
