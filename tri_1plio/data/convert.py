from pathlib import Path

import numpy as np


def write_plio64_i32(path: Path, vec: np.ndarray) -> None:
    vec = np.asarray(vec, dtype=np.int32).reshape(-1)
    if vec.size % 2 != 0:
        raise ValueError(f"{path} requires an even int32 count for plio_64_bits")

    with path.open("w", encoding="utf-8") as f:
        for i in range(0, vec.size, 2):
            f.write(f"{int(vec[i])} {int(vec[i + 1])}\n")


data_dir = Path(__file__).resolve().parent
inp = data_dir / "input.txt"
out = data_dir / "input_plio.txt"

mat = np.loadtxt(inp, dtype=np.int32)
vec = mat.reshape(-1)
write_plio64_i32(out, vec)

print("input shape :", mat.shape)
print("int32 count :", vec.shape[0])
print("plio64 lines:", vec.shape[0] // 2)
print("written to  :", out)
