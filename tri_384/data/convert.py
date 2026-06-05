from pathlib import Path
import argparse
import numpy as np


DEFAULT_LANES = 128
COL = 256
GRID_ROWS = 256
DEFAULT_DEPTH = 64
STENCIL_RADIUS_ROWS = 2
WARMUP_ROWS = 2 * STENCIL_RADIUS_ROWS


def write_stream(path: Path, vec: np.ndarray) -> None:
    with path.open("w", encoding="utf-8") as f:
        for v in vec:
            f.write(f"{int(v)}\n")


def write_lane_map(path: Path, lanes: int, depth: int, grid_rows: int) -> None:
    lanes_per_depth, rows_per_lane, iter_cnt = lane_schedule(lanes, depth, grid_rows)
    with path.open("w", encoding="utf-8") as f:
        f.write("# lane depth part input_rows output_rows raw_output_rows\n")
        for lane in range(lanes):
            d = lane // lanes_per_depth
            part = lane % lanes_per_depth
            out_start = part * rows_per_lane
            out_end = out_start + rows_per_lane - 1
            in_start = out_start - STENCIL_RADIUS_ROWS
            in_end = in_start + iter_cnt - 1
            f.write(
                f"{lane} {d} {part} "
                f"{in_start}..{in_end} "
                f"{out_start}..{out_end} "
                f"{WARMUP_ROWS}..{iter_cnt - 1}\n"
            )


def lane_schedule(lanes: int, depth: int, grid_rows: int) -> tuple[int, int, int]:
    if lanes % depth != 0:
        raise ValueError(f"lanes ({lanes}) must be a multiple of depth ({depth})")
    lanes_per_depth = lanes // depth
    if grid_rows % lanes_per_depth != 0:
        raise ValueError(
            f"grid_rows ({grid_rows}) must be divisible by lanes_per_depth "
            f"({lanes_per_depth})"
        )
    rows_per_lane = grid_rows // lanes_per_depth
    return lanes_per_depth, rows_per_lane, rows_per_lane + WARMUP_ROWS


def lane_rows(plane: np.ndarray, out_start: int, rows_per_lane: int) -> np.ndarray:
    grid_rows, cols = plane.shape
    if cols != COL:
        raise ValueError(f"expected {COL} cols, got {cols}")

    rows = np.zeros((rows_per_lane + WARMUP_ROWS, COL), dtype=np.int32)
    src_start = out_start - STENCIL_RADIUS_ROWS
    for i in range(rows.shape[0]):
        src_row = src_start + i
        if 0 <= src_row < grid_rows:
            rows[i] = plane[src_row]
    return rows


def main() -> None:
    ap = argparse.ArgumentParser(
        description="Convert input.txt to per-lane tri_384 PLIO streams."
    )
    ap.add_argument("--input", type=Path, default=Path("./input.txt"))
    ap.add_argument("--output-dir", type=Path, default=Path("."))
    ap.add_argument("--output-prefix", default="input_plio")
    ap.add_argument("--no-lane-map", action="store_true")
    ap.add_argument("--lanes", type=int, default=DEFAULT_LANES)
    ap.add_argument("--depth", type=int, default=DEFAULT_DEPTH)
    ap.add_argument("--rows", type=int, default=GRID_ROWS)
    ap.add_argument("--cols", type=int, default=COL)
    args = ap.parse_args()

    mat = np.loadtxt(args.input, dtype=np.int32)
    if mat.shape != (args.depth * args.rows, args.cols):
        raise ValueError(
            f"{args.input} must be shape "
            f"({args.depth * args.rows}, {args.cols}), got {mat.shape}"
        )
    if args.cols != COL:
        raise ValueError(f"this flow expects cols={COL}")

    args.output_dir.mkdir(parents=True, exist_ok=True)

    volume = mat.reshape(args.depth, args.rows, args.cols).astype(np.int32)
    lanes_per_depth, rows_per_lane, iter_cnt = lane_schedule(
        args.lanes,
        args.depth,
        args.rows,
    )

    for lane in range(args.lanes):
        depth = lane // lanes_per_depth
        part = lane % lanes_per_depth
        out_start = part * rows_per_lane
        stream = lane_rows(volume[depth], out_start, rows_per_lane).reshape(-1)
        write_stream(args.output_dir / f"{args.output_prefix}{lane}.txt", stream)

    if not args.no_lane_map:
        write_lane_map(args.output_dir / "lane_map.txt",
                       args.lanes,
                       args.depth,
                       args.rows)

    print("input shape :", mat.shape)
    print("volume      :", f"{args.depth} x {args.rows} x {args.cols}")
    print("lanes       :", args.lanes)
    print("lanes/depth :", lanes_per_depth)
    print("rows/lane   :", rows_per_lane)
    print("iter_cnt    :", iter_cnt)
    print("output lines:", iter_cnt * COL)
    print("written to  :", args.output_dir / f"{args.output_prefix}0.txt", "..."
          f" {args.output_prefix}{args.lanes - 1}.txt")


if __name__ == "__main__":
    main()
