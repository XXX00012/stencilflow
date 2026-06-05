from pathlib import Path
import numpy as np

inp = Path("./input.txt")
out = Path("./input_plio.txt")

mat = np.loadtxt(inp, dtype=np.int32)  
vec = mat.reshape(-1)                 

with out.open("w", encoding="utf-8") as f:
    for v in vec:
        f.write(f"{int(v)}\n")

print("input shape :", mat.shape)
print("output lines:", vec.shape[0])
print("written to  :", out)