# four_complete

This is the four-kernel complete-window version. It keeps the six_complete
external raw-row inputs for `mscom` and `selupdate`, but fuses `ms+com` and
`sel+update`.

Dataflow:

```text
raw rows in[0] -> hdiff_lap -> hdiff_sub ---------------------> hdiff_selupdate -> out
raw rows in[1] ---------------------> hdiff_mscom ------------^
raw rows in[2] ------------------------------------------------> hdiff_selupdate
```

Semantic target:

- Structure follows the four-kernel split: `lap -> sub -> mscom -> selupdate`.
- Computation mode follows the three-kernel version: after the fifth raw row arrives, one firing computes all four flux directions for the same logical output row.
- `lap/sub` use one-row cross-firing reuse: the previous package's trailing
  `lap_mid` becomes the current row's `lap_up`.
- `mscom` computes the four compare products and emits compact masks directly.
- `selupdate` applies the masks to the sub rows and immediately performs the
  final update with its external center-psi input.

Package layouts:

- `lap_complete`: `[lap_left | lap_right | lap_down | lap_mid]`
- `sub_complete`: `[sub_left | sub_right | sub_up | sub_down]`
- `mscom_complete`: `[mask_left | mask_right | mask_up | mask_down]`
- `selupdate_complete`: `out = psi + C * (right - left - up + down)`, `C = -7`, matching the uploaded three-kernel `flux2` algebra.

Integration notes:

1. Copy `TopGraph.h` to your graph root, and `ProcessGraph/StencilCoreGraph.h` plus the `ProcessUnit/*.cc` files to the matching project folders.
2. Merge the declarations and type aliases in `ProcessUnit/hdiff.h` into your existing `hdiff.h` if your project already has one. Do not delete your existing declarations if other versions still need them.
3. The graph uses three identical input PLIO streams from `./data/input_plio.txt`, following your current six_complete style.
4. The current tile placement is a single vertical lane at column 7, rows 0..3. Adjust only if your full block placement requires different coordinates.
