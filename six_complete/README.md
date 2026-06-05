# six_complete

This is the six-kernel complete-window version.

Dataflow:

```text
raw rows in[0] -> hdiff_lap -> hdiff_sub -> hdiff_sel -> hdiff_update -> out
raw rows in[1] ---------------------> hdiff_ms -> hdiff_com -^
raw rows in[2] ------------------------------------------------> hdiff_update
```

Semantic target:

- Structure follows the six-kernel split: `lap -> sub -> ms -> com -> sel -> update`.
- Computation mode follows the three-kernel version: after the fifth raw row arrives, one firing computes all four flux directions for the same logical output row.
- `lap/sub` use one-row cross-firing reuse: the previous package's trailing
  `lap_mid` becomes the current row's `lap_up`.

Package layouts:

- `lap_complete`: `[lap_left | lap_right | lap_down | lap_mid]`
- `sub_complete`: `[sub_left | sub_right | sub_up | sub_down]`
- `ms_complete`: `[ms_left | ms_right | ms_up | ms_down]`
- `com_complete`: `[mask_left | mask_right | mask_up | mask_down]`
- `sel_complete`: `[flux_left | flux_right | flux_up | flux_down]`
- `update_complete`: `out = psi + C * (right - left - up + down)`, `C = -7`, matching the uploaded three-kernel `flux2` algebra.

Integration notes:

1. Copy `TopGraph.h` to your graph root, and `ProcessGraph/StencilCoreGraph.h` plus the `ProcessUnit/*.cc` files to the matching project folders.
2. Merge the declarations and type aliases in `ProcessUnit/hdiff.h` into your existing `hdiff.h` if your project already has one. Do not delete your existing declarations if other versions still need them.
3. The graph uses three identical input PLIO streams from `./data/input_plio.txt`, following your current six-partial style.
4. The current tile placement is a single vertical lane at column 7, rows 0..5. Adjust only if your full block placement requires different coordinates.
