# four_flux

Four-kernel complete-window HDiff ablation version.

Dataflow:

```text
raw rows in[0] -> hdiff_lap -- lap_to_x --> hdiff_flux_x -- Dx --> hdiff_update -> out
                         \-- lap_to_y --> hdiff_flux_y -- Dy ----^
raw rows in[1] ---------------------------> hdiff_flux_x
raw rows in[2] ---------------------------> hdiff_flux_y
raw rows in[3] -----------------------------------------------> hdiff_update
```

Kernel split:

- `hdiff_lap`: computes only Laplacian and writes two identical packages.
- `hdiff_flux_x`: computes x-direction flux closure internally and outputs `Dx = flux_right - flux_left`.
- `hdiff_flux_y`: computes y-direction flux closure internally and outputs `Dy = flux_down - flux_up`.
- `hdiff_update`: computes only `out = psi - 7 * (Dx + Dy)`.

Package layouts:

- `lap_to_x`, `lap_to_y`: `[lap_left | lap_right | lap_down | lap_mid]`
- `Dx`: one `COL` row
- `Dy`: one `COL` row

The four input PLIOs intentionally read the same `./data/input_plio.txt` raw row stream to avoid relying on graph-internal fan-out for raw psi.
