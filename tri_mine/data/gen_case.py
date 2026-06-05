from __future__ import annotations

import argparse
from pathlib import Path
import numpy as np


COL = 256
# run(6) consumes 6 source rows and produces 6 rows.
# The first 4 rows are invalid margin-priming rows; the last 2 are compared.
GRID_ROWS = 2
DEFAULT_DEPTH = 1
DEFAULT_C_COEFF = 7

GENERATED_PATTERNS = (
    "gold_out.txt",
    "input.txt",
    "input_plio.txt",
    "hdiff_in*.txt",
    "TestOutputS.txt",
)


# ----------------------------
# basic io
# ----------------------------

def write_matrix_txt(path: Path, mat: np.ndarray) -> None:
    mat = np.asarray(mat, dtype=np.int32)
    with path.open("w", encoding="utf-8") as f:
        for row in mat:
            f.write(" ".join(str(int(v)) for v in row) + "\n")


def read_matrix_txt(path: Path, rows: int | None = None, cols: int | None = None) -> np.ndarray:
    data = []
    with path.open("r", encoding="utf-8") as f:
        for line in f:
            s = line.strip()
            if not s:
                continue
            vals = [int(x, 0) for x in s.split()]
            data.append(vals)

    arr = np.asarray(data, dtype=np.int32)
    if rows is not None and arr.shape[0] != rows:
        raise ValueError(f"{path} row count mismatch: got {arr.shape[0]}, expect {rows}")
    if cols is not None and arr.shape[1] != cols:
        raise ValueError(f"{path} col count mismatch: got {arr.shape[1]}, expect {cols}")
    return arr


def write_stream_txt(path: Path, vec: np.ndarray) -> None:
    vec = np.asarray(vec, dtype=np.int32).reshape(-1)
    with path.open("w", encoding="utf-8") as f:
        for v in vec:
            f.write(f"{int(v)}\n")


def write_plio64_i32(path: Path, vec: np.ndarray) -> None:
    vec = np.asarray(vec, dtype=np.int32).reshape(-1)
    if vec.size % 2 != 0:
        raise ValueError(f"{path} requires an even int32 count for plio_64_bits")

    with path.open("w", encoding="utf-8") as f:
        for i in range(0, vec.size, 2):
            f.write(f"{int(vec[i])} {int(vec[i + 1])}\n")


def cleanup_generated_files(data_dir: Path) -> None:
    for pattern in GENERATED_PATTERNS:
        for path in data_dir.glob(pattern):
            if path.is_file():
                path.unlink()


# ----------------------------
# input generation
# ----------------------------

def make_input_rows(kind: str, rows: int, cols: int, seed: int,
                    low: int, high: int, const_value: int) -> np.ndarray:
    rng = np.random.default_rng(seed)

    if kind == "zeros":
        x = np.zeros((rows, cols), dtype=np.int32)
    elif kind == "const":
        x = np.full((rows, cols), const_value, dtype=np.int32)
    elif kind == "ramp":
        base = np.arange(cols, dtype=np.int32)
        x = np.stack([base + np.int32(10 * r) for r in range(rows)], axis=0)
    elif kind == "checker":
        base = np.array([1 if i % 2 == 0 else -1 for i in range(cols)], dtype=np.int32)
        x = np.stack([(r + 1) * base for r in range(rows)], axis=0).astype(np.int32)
    elif kind == "impulse":
        x = np.zeros((rows, cols), dtype=np.int32)
        for r in range(rows):
            pos = (cols // 2 + r) % cols
            x[r, pos] = np.int32(5 + r)
    elif kind == "random":
        x = rng.integers(low, high + 1, size=(rows, cols), dtype=np.int32)
    else:
        raise ValueError(f"unsupported kind: {kind}")

    return x.astype(np.int32)


# ----------------------------
# formula-level reference
# ----------------------------

class FlatPsi:
    def __init__(self, mat: np.ndarray, cols: int, flat_oob: str):
        if mat.ndim != 2:
            raise ValueError(f"input must be a 2-D matrix, got shape {mat.shape}")
        if mat.shape[1] != cols:
            raise ValueError(f"input column mismatch: got {mat.shape[1]}, expect {cols}")

        self.cols = cols
        self.flat_oob = flat_oob
        self.flat = np.asarray(mat, dtype=np.int32).reshape(-1).astype(np.int64)

    def at(self, row: int, col: int) -> int:
        idx = row * self.cols + col
        if 0 <= idx < self.flat.size:
            return int(self.flat[idx])

        if self.flat_oob == "zero":
            return 0
        if self.flat_oob == "wrap":
            return int(self.flat[idx % self.flat.size])
        if self.flat_oob == "clamp":
            return int(self.flat[min(max(idx, 0), self.flat.size - 1)])
        if self.flat_oob == "error":
            raise IndexError(
                f"flat index out of input range: row={row}, col={col}, idx={idx}"
            )
        raise ValueError(f"unsupported flat_oob: {self.flat_oob}")

    def lap(self, row: int, col: int) -> int:
        return (
            4 * self.at(row, col)
            - self.at(row + 1, col)
            - self.at(row - 1, col)
            - self.at(row, col + 1)
            - self.at(row, col - 1)
        )


def formula_flux(diff_lap: int, diff_psi: int, select_when: str) -> int:
    product = diff_lap * diff_psi
    if select_when == "le0":
        return diff_lap if product <= 0 else 0
    if select_when == "gt0":
        return diff_lap if product > 0 else 0
    raise ValueError(f"unsupported select_when: {select_when}")


def hdiff_formula_rows(mat: np.ndarray,
                       cols: int,
                       c_coeff: int,
                       center_row_offset: int,
                       col_offset: int,
                       select_when: str,
                       flat_oob: str) -> np.ndarray:
    mat = np.asarray(mat, dtype=np.int32)
    if mat.ndim != 2 or mat.shape[1] != cols:
        raise ValueError(f"mat must be shape (N, {cols}), got {mat.shape}")
    if mat.shape[0] < 5:
        raise ValueError("need at least 5 input rows")

    psi = FlatPsi(mat, cols, flat_oob)
    num_valid_rows = mat.shape[0] - 4
    out = np.empty((num_valid_rows, cols), dtype=np.int32)

    for out_r in range(num_valid_rows):
        r = center_row_offset + out_r
        for out_c in range(cols):
            c = col_offset + out_c

            lap_rc = psi.lap(r, c)
            lap_left = psi.lap(r, c - 1)
            lap_right = psi.lap(r, c + 1)
            lap_up = psi.lap(r - 1, c)
            lap_down = psi.lap(r + 1, c)

            left = formula_flux(
                lap_rc - lap_left,
                psi.at(r, c) - psi.at(r, c - 1),
                select_when,
            )
            right = formula_flux(
                lap_right - lap_rc,
                psi.at(r, c + 1) - psi.at(r, c),
                select_when,
            )
            up = formula_flux(
                lap_rc - lap_up,
                psi.at(r, c) - psi.at(r - 1, c),
                select_when,
            )
            down = formula_flux(
                lap_down - lap_rc,
                psi.at(r + 1, c) - psi.at(r, c),
                select_when,
            )

            updated = psi.at(r, c) - c_coeff * (right - left - up + down)
            out[out_r, out_c] = np.int32(updated)

    return out


# ----------------------------
# window simulator
# ----------------------------

class Win:
    def __init__(self, arr: np.ndarray, oob_mode: str = "zero"):
        self.arr = np.asarray(arr, dtype=np.int32).reshape(-1)
        self.pos = 0
        self.oob_mode = oob_mode

    def _get(self, idx: int) -> np.int32:
        n = self.arr.shape[0]
        if 0 <= idx < n:
            return np.int32(self.arr[idx])

        if self.oob_mode == "zero":
            return np.int32(0)
        if self.oob_mode == "clamp":
            return np.int32(self.arr[0] if idx < 0 else self.arr[-1])
        if self.oob_mode == "periodic":
            return np.int32(self.arr[idx % n])

        raise ValueError(f"unsupported oob_mode: {self.oob_mode}")

    def _slice8(self, start: int) -> np.ndarray:
        return np.asarray([self._get(start + k) for k in range(8)], dtype=np.int32)

    def readincr_v8(self) -> np.ndarray:
        v = self._slice8(self.pos)
        self.pos += 8
        return v

    def read_v8(self) -> np.ndarray:
        return self._slice8(self.pos)

    def decr_v8(self, n: int) -> None:
        self.pos -= 8 * n


# ----------------------------
# vector helpers
# ----------------------------

def upd_w(buf16: np.ndarray, idx: int, v8: np.ndarray) -> np.ndarray:
    out = np.asarray(buf16, dtype=np.int32).copy()
    out[idx * 8:(idx + 1) * 8] = np.asarray(v8, dtype=np.int32)
    return out


def ext_w(buf16: np.ndarray, idx: int) -> np.ndarray:
    return np.asarray(buf16[idx * 8:(idx + 1) * 8], dtype=np.int32)


def concat(lower8: np.ndarray, upper8: np.ndarray) -> np.ndarray:
    return np.concatenate([
        np.asarray(lower8, dtype=np.int32),
        np.asarray(upper8, dtype=np.int32)
    ]).astype(np.int32)


def mul8(buf16: np.ndarray, off: int, vec8: np.ndarray) -> np.ndarray:
    a = np.asarray(buf16[off:off + 8], dtype=np.int64)
    b = np.asarray(vec8, dtype=np.int64)
    return a * b


def lmul8_sim(buf16: np.ndarray, off: int, vec8: np.ndarray) -> np.ndarray:
    return mul8(buf16, off, vec8)


def lmac8_sim(acc: np.ndarray, buf16: np.ndarray, off: int, vec8: np.ndarray) -> np.ndarray:
    return np.asarray(acc, dtype=np.int64) + mul8(buf16, off, vec8)


def lmsc8_sim(acc: np.ndarray, buf16: np.ndarray, off: int, vec8: np.ndarray) -> np.ndarray:
    return np.asarray(acc, dtype=np.int64) - mul8(buf16, off, vec8)


def to_i32(v: np.ndarray) -> np.ndarray:
    return np.asarray(v, dtype=np.int32)


# ----------------------------
# kernel-faithful simulators
# ----------------------------

def hdiff_lap_kernel(row0: np.ndarray, row1: np.ndarray, row2: np.ndarray,
                     row3: np.ndarray, row4: np.ndarray,
                     oob_mode: str = "zero") -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    coeffs = np.full(8, -4, dtype=np.int32)
    coeffs_rest = np.full(8, -1, dtype=np.int32)

    row0_win = Win(row0, oob_mode)
    row1_win = Win(row1, oob_mode)
    row2_win = Win(row2, oob_mode)
    row3_win = Win(row3, oob_mode)
    row4_win = Win(row4, oob_mode)

    data_buf1 = np.zeros(16, dtype=np.int32)
    data_buf2 = np.zeros(16, dtype=np.int32)

    data_buf1 = upd_w(data_buf1, 0, row3_win.readincr_v8())
    data_buf1 = upd_w(data_buf1, 1, row3_win.read_v8())

    data_buf2 = upd_w(data_buf2, 0, row1_win.readincr_v8())
    data_buf2 = upd_w(data_buf2, 1, row1_win.read_v8())

    out_flux1 = []
    out_flux2 = []
    out_flux3 = []
    out_flux4 = []

    for _ in range(COL // 8):
        acc_0 = lmul8_sim(data_buf2, 2, coeffs_rest)
        acc_1 = lmul8_sim(data_buf2, 1, coeffs_rest)

        acc_0 = lmac8_sim(acc_0, data_buf1, 2, coeffs_rest)
        acc_1 = lmac8_sim(acc_1, data_buf1, 1, coeffs_rest)

        data_buf2 = upd_w(data_buf2, 0, row2_win.readincr_v8())
        data_buf2 = upd_w(data_buf2, 1, row2_win.read_v8())

        acc_0 = lmac8_sim(acc_0, data_buf2, 1, coeffs_rest)
        acc_0 = lmsc8_sim(acc_0, data_buf2, 2, coeffs)
        acc_0 = lmac8_sim(acc_0, data_buf2, 3, coeffs_rest)
        lap_ij = to_i32(acc_0)

        acc_1 = lmac8_sim(acc_1, data_buf2, 0, coeffs_rest)
        acc_1 = lmsc8_sim(acc_1, data_buf2, 1, coeffs)
        acc_1 = lmac8_sim(acc_1, data_buf2, 2, coeffs_rest)
        lap_0 = to_i32(acc_1)

        flux1 = to_i32(lap_ij.astype(np.int64) - lap_0.astype(np.int64))
        out_flux1.append(flux1)

        acc_0 = lmul8_sim(data_buf1, 3, coeffs_rest)
        acc_0 = lmsc8_sim(acc_0, data_buf2, 3, coeffs)

        row1_win.decr_v8(1)
        data_buf1 = upd_w(data_buf1, 0, row1_win.readincr_v8())
        data_buf1 = upd_w(data_buf1, 1, row1_win.read_v8())

        acc_0 = lmac8_sim(acc_0, data_buf2, 2, coeffs_rest)
        acc_0 = lmac8_sim(acc_0, data_buf2, 4, coeffs_rest)
        acc_0 = lmac8_sim(acc_0, data_buf1, 3, coeffs_rest)
        lap_0 = to_i32(acc_0)

        flux2 = to_i32(lap_0.astype(np.int64) - lap_ij.astype(np.int64))
        out_flux2.append(flux2)

        acc_1 = lmul8_sim(data_buf2, 2, coeffs_rest)
        acc_0 = lmul8_sim(data_buf2, 2, coeffs_rest)

        data_buf2 = upd_w(data_buf2, 0, row0_win.readincr_v8())
        data_buf2 = upd_w(data_buf2, 1, row0_win.read_v8())

        acc_1 = lmsc8_sim(acc_1, data_buf1, 2, coeffs)
        acc_1 = lmac8_sim(acc_1, data_buf1, 1, coeffs_rest)
        acc_1 = lmac8_sim(acc_1, data_buf2, 2, coeffs_rest)

        data_buf2 = upd_w(data_buf2, 0, row4_win.readincr_v8())
        data_buf2 = upd_w(data_buf2, 1, row4_win.read_v8())

        acc_1 = lmac8_sim(acc_1, data_buf1, 3, coeffs_rest)
        acc_0 = lmac8_sim(acc_0, data_buf2, 2, coeffs_rest)

        lap_0 = to_i32(acc_1)
        flux3 = to_i32(lap_ij.astype(np.int64) - lap_0.astype(np.int64))
        out_flux3.append(flux3)

        row3_win.decr_v8(1)
        data_buf1 = upd_w(data_buf1, 0, row3_win.readincr_v8())
        data_buf1 = upd_w(data_buf1, 1, row3_win.read_v8())

        acc_0 = lmsc8_sim(acc_0, data_buf1, 2, coeffs)
        acc_0 = lmac8_sim(acc_0, data_buf1, 1, coeffs_rest)

        data_buf2 = upd_w(data_buf2, 0, row1_win.readincr_v8())
        data_buf2 = upd_w(data_buf2, 1, row1_win.read_v8())

        acc_0 = lmac8_sim(acc_0, data_buf1, 3, coeffs_rest)
        flux4 = to_i32(acc_0.astype(np.int64) - lap_ij.astype(np.int64))
        out_flux4.append(flux4)

        data_buf1 = upd_w(data_buf1, 0, row3_win.readincr_v8())
        data_buf1 = upd_w(data_buf1, 1, row3_win.read_v8())

    return (
        np.concatenate(out_flux1).astype(np.int32),
        np.concatenate(out_flux2).astype(np.int32),
        np.concatenate(out_flux3).astype(np.int32),
        np.concatenate(out_flux4).astype(np.int32),
    )


def hdiff_flux1_kernel(row1: np.ndarray, row2: np.ndarray, row3: np.ndarray,
                       flux_forward1: np.ndarray, flux_forward2: np.ndarray,
                       flux_forward3: np.ndarray, flux_forward4: np.ndarray,
                       oob_mode: str = "zero") -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    row1_win = Win(row1, oob_mode)
    row2_win = Win(row2, oob_mode)
    row3_win = Win(row3, oob_mode)

    f1_win = Win(flux_forward1, oob_mode)
    f2_win = Win(flux_forward2, oob_mode)
    f3_win = Win(flux_forward3, oob_mode)
    f4_win = Win(flux_forward4, oob_mode)

    data_buf1 = np.zeros(16, dtype=np.int32)
    data_buf2 = np.zeros(16, dtype=np.int32)

    data_buf1 = upd_w(data_buf1, 0, row1_win.readincr_v8())
    data_buf1 = upd_w(data_buf1, 1, row1_win.read_v8())

    data_buf2 = upd_w(data_buf2, 0, row2_win.readincr_v8())
    data_buf2 = upd_w(data_buf2, 1, row2_win.read_v8())

    inter1 = []
    inter2 = []
    inter3 = []
    inter4 = []
    inter5 = []

    for _ in range(COL // 8):
        flux_sub = f1_win.readincr_v8()
        acc = lmul8_sim(data_buf2, 2, flux_sub)
        acc = lmsc8_sim(acc, data_buf2, 1, flux_sub)
        inter1.append(flux_sub)
        inter1.append(to_i32(acc))

        flux_sub = f2_win.readincr_v8()
        acc = lmul8_sim(data_buf2, 3, flux_sub)
        acc = lmsc8_sim(acc, data_buf2, 2, flux_sub)
        inter2.append(flux_sub)
        inter2.append(to_i32(acc))

        flux_sub = f3_win.readincr_v8()
        acc = lmul8_sim(data_buf2, 2, flux_sub)
        acc = lmsc8_sim(acc, data_buf1, 2, flux_sub)
        inter3.append(flux_sub)
        inter3.append(to_i32(acc))

        data_buf1 = upd_w(data_buf1, 0, row3_win.readincr_v8())
        data_buf1 = upd_w(data_buf1, 1, row3_win.read_v8())

        flux_sub = f4_win.readincr_v8()
        acc = lmul8_sim(data_buf1, 2, flux_sub)
        acc = lmsc8_sim(acc, data_buf2, 2, flux_sub)
        inter4.append(flux_sub)
        inter4.append(to_i32(acc))

        data_buf1 = upd_w(data_buf1, 0, row1_win.readincr_v8())
        data_buf1 = upd_w(data_buf1, 1, row1_win.read_v8())

        inter5.append(ext_w(data_buf2, 1))
        inter5.append(ext_w(data_buf2, 0))

        data_buf2 = upd_w(data_buf2, 0, row2_win.readincr_v8())
        data_buf2 = upd_w(data_buf2, 1, row2_win.read_v8())

    return (
        np.concatenate(inter1).astype(np.int32),
        np.concatenate(inter2).astype(np.int32),
        np.concatenate(inter3).astype(np.int32),
        np.concatenate(inter4).astype(np.int32),
        np.concatenate(inter5).astype(np.int32),
    )


def hdiff_flux2_kernel(flux_inter1: np.ndarray, flux_inter2: np.ndarray,
                       flux_inter3: np.ndarray, flux_inter4: np.ndarray,
                       flux_inter5: np.ndarray,
                       oob_mode: str = "zero") -> np.ndarray:
    inter1_win = Win(flux_inter1, oob_mode)
    inter2_win = Win(flux_inter2, oob_mode)
    inter3_win = Win(flux_inter3, oob_mode)
    inter4_win = Win(flux_inter4, oob_mode)
    inter5_win = Win(flux_inter5, oob_mode)

    out = []

    for _ in range(COL // 8):
        flux_sub = inter1_win.readincr_v8()
        flux_prod = inter1_win.readincr_v8()
        out1 = np.where(flux_prod.astype(np.int64) > 0, 0, flux_sub).astype(np.int32)

        flux_sub = inter2_win.readincr_v8()
        flux_prod = inter2_win.readincr_v8()
        out2 = np.where(flux_prod.astype(np.int64) > 0, 0, flux_sub).astype(np.int32)

        flux_sub = inter3_win.readincr_v8()
        flux_prod = inter3_win.readincr_v8()
        out3 = np.where(flux_prod.astype(np.int64) > 0, 0, flux_sub).astype(np.int32)

        flux_sub = inter4_win.readincr_v8()
        flux_prod = inter4_win.readincr_v8()
        out4 = np.where(flux_prod.astype(np.int64) > 0, 0, flux_sub).astype(np.int32)

        flx_out4 = (
            out2.astype(np.int64)
            - out1.astype(np.int64)
            - out3.astype(np.int64)
            + out4.astype(np.int64)
        ).astype(np.int32)

        tmp1 = inter5_win.readincr_v8()
        tmp2 = inter5_win.readincr_v8()
        data_buf2 = concat(tmp2, tmp1)

        final_out = (-7 * flx_out4.astype(np.int64) + data_buf2[2:10].astype(np.int64)).astype(np.int32)
        out.append(final_out)

    return np.concatenate(out).astype(np.int32)


def hdiff_one_window_exact(rows5: np.ndarray, oob_mode: str = "zero") -> np.ndarray:
    rows5 = np.asarray(rows5, dtype=np.int32)
    if rows5.shape != (5, COL):
        raise ValueError(f"rows5 must be shape (5, {COL}), got {rows5.shape}")

    row0, row1, row2, row3, row4 = rows5

    flux1, flux2, flux3, flux4 = hdiff_lap_kernel(
        row0, row1, row2, row3, row4, oob_mode=oob_mode
    )

    inter1, inter2, inter3, inter4, inter5 = hdiff_flux1_kernel(
        row1, row2, row3,
        flux1, flux2, flux3, flux4,
        oob_mode=oob_mode
    )

    out = hdiff_flux2_kernel(
        inter1, inter2, inter3, inter4, inter5,
        oob_mode=oob_mode
    )

    return out.astype(np.int32)


def hdiff_multi_windows_exact(rows: np.ndarray, oob_mode: str = "zero") -> np.ndarray:
    rows = np.asarray(rows, dtype=np.int32)
    if rows.ndim != 2 or rows.shape[1] != COL:
        raise ValueError(f"rows must be shape (N, {COL}), got {rows.shape}")
    if rows.shape[0] < 5:
        raise ValueError("need at least 5 input rows")

    num_iter = rows.shape[0] - 4
    outs = []
    for i in range(num_iter):
        outs.append(hdiff_one_window_exact(rows[i:i + 5], oob_mode=oob_mode))
    return np.stack(outs, axis=0).astype(np.int32)


# ----------------------------
# depth-aware packing
# ----------------------------

def make_depth_slices(kind: str,
                      depth: int,
                      grid_rows: int,
                      cols: int,
                      seed: int,
                      low: int,
                      high: int,
                      const_value: int) -> np.ndarray:
    slices = []
    for d in range(depth):
        slice_seed = seed + d if kind == "random" else seed
        sl = make_input_rows(
            kind=kind,
            rows=grid_rows + 4,
            cols=cols,
            seed=slice_seed,
            low=low,
            high=high,
            const_value=const_value,
        )
        slices.append(sl.astype(np.int32))
    return np.stack(slices, axis=0).astype(np.int32)


def gold_from_depth_slices(slices3d: np.ndarray,
                           gold_mode: str,
                           oob_mode: str,
                           c_coeff: int,
                           center_row_offset: int,
                           col_offset: int,
                           select_when: str,
                           flat_oob: str) -> np.ndarray:
    outs = []
    for d in range(slices3d.shape[0]):
        if gold_mode == "formula":
            valid = hdiff_formula_rows(
                slices3d[d],
                cols=COL,
                c_coeff=c_coeff,
                center_row_offset=center_row_offset,
                col_offset=col_offset,
                select_when=select_when,
                flat_oob=flat_oob,
            )
        elif gold_mode == "kernel-faithful":
            valid = hdiff_multi_windows_exact(slices3d[d], oob_mode=oob_mode)
        else:
            raise ValueError(f"unsupported gold_mode: {gold_mode}")
        invalid = np.zeros((4, COL), dtype=np.int32)
        outs.append(np.concatenate([invalid, valid], axis=0).astype(np.int32))
    return np.concatenate(outs, axis=0).astype(np.int32)


def dump_human_readable_inputs_depthwise(slices3d: np.ndarray, out_dir: Path, graph_id: str = "hdiff") -> None:
    depth, rows_with_halo, _ = slices3d.shape
    num_iter_per_depth = rows_with_halo - 4

    for i in range(5):
        mats = []
        for d in range(depth):
            mats.append(slices3d[d, i:i + num_iter_per_depth])
        mat = np.concatenate(mats, axis=0).astype(np.int32)
        write_matrix_txt(out_dir / f"{graph_id}_in{i}.txt", mat)


def dump_aie_stream_inputs_depthwise(slices3d: np.ndarray, out_dir: Path, graph_id: str = "hdiff") -> None:
    depth, rows_with_halo, _ = slices3d.shape
    num_iter_per_depth = rows_with_halo - 4

    for i in range(5):
        chunks = []
        for d in range(depth):
            chunks.append(slices3d[d, i:i + num_iter_per_depth].reshape(-1))
        stream = np.concatenate(chunks).astype(np.int32)
        write_stream_txt(out_dir / f"{graph_id}_in{i}_stream.txt", stream)


# ----------------------------
# main
# ----------------------------

def main() -> None:
    base_dir = Path(__file__).resolve().parent

    ap = argparse.ArgumentParser(
        description="Generate the 1-PLIO input files for tri_1plio."
    )
    ap.add_argument("--skip-gold", action="store_true",
                    help="only generate input files, do not compute gold_out.txt")
    ap.add_argument("--emit-debug-streams", action="store_true",
                    help="also generate the old 5-input debug files hdiff_in*.txt and hdiff_in*_stream.txt")
    ap.add_argument("--data-dir", type=Path, default=base_dir)
    ap.add_argument("--grid-rows", type=int, default=GRID_ROWS,
                    help="number of output rows per depth plane")
    ap.add_argument("--depth", type=int, default=DEFAULT_DEPTH,
                    help="number of independent depth planes")
    ap.add_argument("--iter", type=int, default=None, dest="iter_cnt",
                    help="optional host iter count; default = (grid_rows + 4) * depth")
    ap.add_argument("--cols", type=int, default=COL)
    ap.add_argument("--kind", choices=["zeros", "const", "ramp", "checker", "impulse", "random"], default="random")
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--low", type=int, default=-20)
    ap.add_argument("--high", type=int, default=20)
    ap.add_argument("--const-value", type=int, default=7)
    ap.add_argument("--graph-id", default="hdiff")
    ap.add_argument("--gold-mode", choices=["formula", "kernel-faithful"], default="formula",
                    help="formula is an independent reference; kernel-faithful replays the AIE window logic")
    ap.add_argument("--c-coeff", type=int, default=DEFAULT_C_COEFF,
                    help="formula update coefficient")
    ap.add_argument("--center-row-offset", type=int, default=2,
                    help="first formula center row; 2 means the first valid output uses rows 0..4")
    ap.add_argument("--col-offset", type=int, default=2,
                    help="first formula center column; 2 matches the current output lane alignment")
    ap.add_argument("--select-when", choices=["le0", "gt0"], default="le0",
                    help="formula limiter: le0 keeps flux when diff_lap*diff_psi <= 0")
    ap.add_argument("--flat-oob", choices=["zero", "wrap", "clamp", "error"], default="zero",
                    help="policy only for addresses outside the flattened input buffer")
    ap.add_argument("--oob-mode", choices=["zero", "clamp", "periodic"], default="zero")
    args = ap.parse_args()

    if args.grid_rows <= 0:
        raise ValueError("--grid-rows must be positive")
    if args.depth <= 0:
        raise ValueError("--depth must be positive")
    if args.cols != COL:
        raise ValueError(f"this flow expects cols={COL}")

    source_rows_per_depth = args.grid_rows + 4
    expected_iter = source_rows_per_depth * args.depth
    if args.iter_cnt is None:
        args.iter_cnt = expected_iter
    elif args.iter_cnt != expected_iter:
        raise ValueError(
            f"--iter must equal (grid_rows + 4) * depth = {expected_iter}, got {args.iter_cnt}"
        )

    data_dir = args.data_dir
    data_dir.mkdir(parents=True, exist_ok=True)
    cleanup_generated_files(data_dir)

    slices3d = make_depth_slices(
        kind=args.kind,
        depth=args.depth,
        grid_rows=args.grid_rows,
        cols=COL,
        seed=args.seed,
        low=args.low,
        high=args.high,
        const_value=args.const_value,
    )

    input_txt = data_dir / "input.txt"
    input_plio_txt = data_dir / "input_plio.txt"
    gold_txt = data_dir / "gold_out.txt"

    # Human-readable source rows, depth-major.
    input_mat = slices3d.reshape(-1, COL)
    write_matrix_txt(input_txt, input_mat)
    write_plio64_i32(input_plio_txt, input_mat.reshape(-1))

    if not args.skip_gold:
        gold = gold_from_depth_slices(
            slices3d,
            gold_mode=args.gold_mode,
            oob_mode=args.oob_mode,
            c_coeff=args.c_coeff,
            center_row_offset=args.center_row_offset,
            col_offset=args.col_offset,
            select_when=args.select_when,
            flat_oob=args.flat_oob,
        )
        write_plio64_i32(gold_txt, gold.reshape(-1))

    if args.emit_debug_streams:
        dump_human_readable_inputs_depthwise(slices3d, data_dir, graph_id=args.graph_id)
        dump_aie_stream_inputs_depthwise(slices3d, data_dir, graph_id=args.graph_id)

    print(f"logical workload : {args.grid_rows} x {COL} x {args.depth}")
    print(f"host iter_cnt    : {args.iter_cnt}")
    print(f"source rows/plane: {source_rows_per_depth}")
    print(f"input matrix     : {input_mat.shape[0]} x {input_mat.shape[1]}")
    print(f"input plio64     : {input_mat.size // 2} lines")
    print(f"written input    : {input_txt}")
    print(f"written plio     : {input_plio_txt}")
    if not args.skip_gold:
        print(f"gold mode        : {args.gold_mode}")
        if args.gold_mode == "formula":
            print(f"formula params   : c={args.c_coeff}, row_offset={args.center_row_offset}, "
                  f"col_offset={args.col_offset}, select={args.select_when}, flat_oob={args.flat_oob}")
        print(f"gold output elems: {gold.shape[0]} x {gold.shape[1]} int32")
        print(f"gold PLIO lines  : {gold.size // 2} lines, 2 int32 per line")
        print("compare output   : drop first 4 rows / 512 plio64 lines of each depth plane")
    if args.emit_debug_streams:
        print(f"debug stream len : each is {args.grid_rows * args.depth * COL} scalars")


if __name__ == "__main__":
    main()
