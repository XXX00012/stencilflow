from pathlib import Path

import numpy as np


def write_plio128_i32(path: Path, vec: np.ndarray) -> None:
    vec = np.asarray(vec, dtype=np.int32).reshape(-1)
    if vec.size % 4 != 0:
        raise ValueError(f"{path} requires a multiple-of-4 int32 count for plio_128_bits")

    with path.open("w", encoding="utf-8") as f:
        for i in range(0, vec.size, 4):
            f.write(
                f"{int(vec[i])} {int(vec[i + 1])} "
                f"{int(vec[i + 2])} {int(vec[i + 3])}\n"
            )


data_dir = Path(__file__).resolve().parent
inp = data_dir / "input.txt"
out = data_dir / "input_plio.txt"

mat = np.loadtxt(inp, dtype=np.int32)
vec = mat.reshape(-1)
write_plio128_i32(out, vec)

print("input shape :", mat.shape)
print("int32 count :", vec.shape[0])
print("plio128 lines:", vec.shape[0] // 4)
print("written to  :", out)
