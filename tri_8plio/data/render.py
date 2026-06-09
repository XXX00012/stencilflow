from pathlib import Path

try:
    from jinja2 import Environment, FileSystemLoader
except ImportError as exc:
    raise SystemExit(
        "tri_8plio render requires Python package 'jinja2'. "
        "Install it in the build environment before running make."
    ) from exc


ROOT = Path(__file__).resolve().parent

CONFIG = {
    "lanes": 8,
    "toppl_count": 1,
    "lanes_per_pl": 8,
    "grid_rows": 256,
    "grid_depth": 4,
    "lanes_per_depth": 2,
    "volume_ddr_words": 4 * 256 * (256 // 16),
}


def render(template_name: str, output_path: str) -> None:
    env = Environment(
        loader=FileSystemLoader(ROOT / "templates"),
        trim_blocks=True,
        lstrip_blocks=True,
        keep_trailing_newline=True,
    )
    text = env.get_template(template_name).render(**CONFIG)
    path = ROOT / output_path
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="\n")


def main() -> None:
    render("TopPL.cpp.j2", "pl/TopPL.cpp")
    render("conn.cfg.j2", "conn.cfg")


if __name__ == "__main__":
    main()
