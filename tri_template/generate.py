from __future__ import annotations

import argparse
import math
import shutil
from pathlib import Path

try:
    from jinja2 import Environment, FileSystemLoader, StrictUndefined
except ImportError as exc:
    raise SystemExit(
        "jinja2 is required: python -m pip install jinja2"
    ) from exc


ROOT = Path(__file__).resolve().parents[1]
TEMPLATE_DIR = Path(__file__).resolve().parent / "templates"
SOURCE_PROJECT = ROOT / "tri_16plio"


def build_context(lanes: int, project: str) -> dict[str, int | str | bool]:
    lanes_per_pl = 8
    lanes_per_depth = 2
    kernels_per_lane = 3
    grid_rows = 256
    row_elems = 256
    warmup_rows = 4
    ints_per_ddr_word = 16

    if lanes % lanes_per_pl != 0:
        raise ValueError(f"lanes must be a multiple of {lanes_per_pl}")
    if lanes % lanes_per_depth != 0:
        raise ValueError(f"lanes must be a multiple of {lanes_per_depth}")

    toppl_cus = lanes // lanes_per_pl
    grid_depth = lanes // lanes_per_depth
    total_cores = lanes * kernels_per_lane
    data_iter = grid_rows // lanes_per_depth + warmup_rows
    ddr_words_per_row = row_elems // ints_per_ddr_word
    volume_ddr_words = grid_depth * grid_rows * ddr_words_per_row

    main_lanes = min(lanes, 100)
    top_lanes = lanes - main_lanes
    top_lanes_per_row = 14
    top_rows = math.ceil(top_lanes / top_lanes_per_row) if top_lanes else 0

    return {
        "project": project,
        "lanes": lanes,
        "grid_depth": grid_depth,
        "grid_rows": grid_rows,
        "row_elems": row_elems,
        "lanes_per_depth": lanes_per_depth,
        "rows_per_lane": grid_rows // lanes_per_depth,
        "warmup_rows": warmup_rows,
        "data_iter": data_iter,
        "toppl_cus": toppl_cus,
        "lanes_per_pl": lanes_per_pl,
        "kernels_per_lane": kernels_per_lane,
        "total_cores": total_cores,
        "volume_ddr_words": volume_ddr_words,
        "main_lanes": main_lanes,
        "top_lanes": top_lanes,
        "top_lanes_per_row": top_lanes_per_row,
        "top_rows": top_rows,
        "use_top_rows": top_lanes > 0,
    }


def render(env: Environment, template_name: str, dst: Path, ctx: dict) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_text(env.get_template(template_name).render(**ctx), encoding="utf-8")


def copy_tree(src: Path, dst: Path) -> None:
    if dst.exists():
        shutil.rmtree(dst)
    shutil.copytree(src, dst)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate stencilflow tri_*plio projects from Jinja templates."
    )
    parser.add_argument("--lanes", type=int, required=True)
    parser.add_argument("--project", default=None)
    parser.add_argument("--out-dir", type=Path, default=None)
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()

    project = args.project or f"tri_{args.lanes}plio"
    out_dir = args.out_dir or (ROOT / project)
    if out_dir.exists() and not args.force:
        raise SystemExit(f"{out_dir} exists; pass --force to overwrite generated files")

    ctx = build_context(args.lanes, project)
    env = Environment(
        loader=FileSystemLoader(TEMPLATE_DIR),
        undefined=StrictUndefined,
        trim_blocks=True,
        lstrip_blocks=True,
        keep_trailing_newline=True,
    )

    out_dir.mkdir(parents=True, exist_ok=True)
    for name in ("run.sh", "xrt.ini", "SystemConfig.h"):
        shutil.copy2(SOURCE_PROJECT / name, out_dir / name)

    copy_tree(SOURCE_PROJECT / "aie" / "ProcessUnit",
              out_dir / "aie" / "ProcessUnit")
    copy_tree(SOURCE_PROJECT / "data", out_dir / "data")

    shutil.copy2(SOURCE_PROJECT / "aie" / "TopGraph.h",
                 out_dir / "aie" / "TopGraph.h")
    shutil.copy2(SOURCE_PROJECT / "aie" / "TopGraph.cpp",
                 out_dir / "aie" / "TopGraph.cpp")
    shutil.copy2(SOURCE_PROJECT / "aie" / "ProcessGraph" / "StencilCoreGraph.cpp",
                 out_dir / "aie" / "ProcessGraph" / "StencilCoreGraph.cpp")

    render(env, "Makefile.j2", out_dir / "Makefile", ctx)
    render(env, "Config.h.j2", out_dir / "aie" / "Config.h", ctx)
    render(env, "StencilCoreGraph.h.j2",
           out_dir / "aie" / "ProcessGraph" / "StencilCoreGraph.h", ctx)
    render(env, "TopPL.cpp.j2", out_dir / "pl" / "TopPL.cpp", ctx)
    render(env, "conn.cfg.j2", out_dir / "conn.cfg", ctx)
    render(env, "host.cpp.j2", out_dir / "ps" / "host.cpp", ctx)

    print(f"generated {project}: {args.lanes} lanes, {ctx['toppl_cus']} TopPL CUs")


if __name__ == "__main__":
    main()
