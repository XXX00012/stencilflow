# five_complete

Five-kernel complete-window hdiff graph.

Dataflow:

```text
raw rows in[0] -> hdiff_lap -> hdiff_sub -> hdiff_comsel -> hdiff_update -> out
raw rows in[1] -----------------> hdiff_ms ----------------^
raw rows in[2] -------------------------------------------> hdiff_update
```

This version follows `five` for the project layout and follows
`six_complete` for the complete-window computation. The only structural
difference from `six_complete` is that `hdiff_com` and `hdiff_sel` are fused
into one `hdiff_comsel` kernel.

The lap stage is reuse-oriented: it emits only
`[lap_left | lap_right | lap_down | lap_mid]` for the current row, with
`lap_mid` deliberately placed last. `hdiff_sub` uses a one-row input margin to
reuse the previous package's trailing `lap_mid` as the current row's `lap_up`;
current `lap_left/lap_right/lap_mid/lap_down` are provided by the current lap
package. The downstream complete four-direction package remains unchanged.

Build from this directory:

```bash
make clear_aie
make aie
```

The `aie/Makefile` is only a wrapper that forwards targets to the project root.
