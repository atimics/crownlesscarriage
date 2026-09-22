# Readable carriage overview

The combined town / Oven Court / carriage review at main `c5589af` exposed a normal-input UI defect: after accepting Mara's delivery and boarding in Thornford, the fixed third column extended beyond a 1200-pixel window and the last manifest rows were covered by the departure action tray. This cut repairs that reading surface; it does not replace the carriage simulation.

## Controls and scope

The Overview tab has a responsive cargo column and a wrapped promise/team column. Body text uses the existing reading font and text-size preference. Cargo actually carried sorts before empty stock types; every goods type and quantity remains available. Mouse wheel over the reading area, Page Up / Page Down, or the explicit scroll buttons reveal the rest. The departure action tray stays outside the clipped reading viewport and remains available at any scroll offset. The Ponies tab and its controls are unchanged.

The scroll position is local presentation state, not a new save field. Reading and scrolling do not advance the clock, transfer cargo, accept a promise, reserve goods, charge money or change the team. This is not a new inventory screen, a change to economics, or a general mobile UI redesign.

## Reproduction and tests

`crownless_carriage --test-carriage-overview` checks logical window layouts from 1040x620 through 1920x1080, all three text settings, content/control separation, reachability of the last cargo row, clamping, complete unique cargo ordering, and queued normal Page Up / Page Down input. Hash checks assert that measurement and scrolling do not mutate the simulation.

Staged capture IDs 31, 32 and 33 are overview top, overview end, and empty manifest. They are explicit fixtures, not claimed completed journeys:

```sh
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ./build/crownless_carriage \
  --capture-ux 31 overview-top.png 1040 2
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ./build/crownless_carriage \
  --capture-ux 32 overview-end.png 1040 2
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ./build/crownless_carriage \
  --capture-ux 33 overview-empty.png 1040 0
```

The same capture entry point is checked at 1040, 1200 and 1280 logical pixels. The explicit reading-area clip applies to both geometry and batched body text; the overlay is flushed before ending the scissor. Measuring and drawing share the same wrapping path so changing text size also changes scroll extent.

## Executed integration evidence

[Native overview run 35686928049](https://github.com/atimics/crownlesscarriage/actions/runs/35686928049) built the Release client with warnings as errors, passed **43/43 selected CTests**, and produced six native reading captures (three widths, large text at top/end, empty manifest). Tested implementation: `1769e6a7229439309c7b7822c43677ee325c68f6`. The later read-only workflow and this note do not alter that implementation. The `carriage-overview.yml` workflow retains layout/capture checks on relevant PRs; a generated screenshot is not itself an automated aesthetic judgment.

The preceding combined town/Oven Court/carriage run [35685640054](https://github.com/atimics/crownlesscarriage/actions/runs/35685640054) passed 54 selected integration tests and produced 33 fresh town/Oven Court views. Its tree `d59d74f359acb0890acc4cfa51e533b967c33650` exactly matches merged main `c5589af`. The first combined run exposed compiler-dependent historical mine migration; #878 fixed ordered coordinate RNG consumption without changing fixture bytes, golden hashes or previously generated saved networks.

This work does not close complete out-and-back visual acceptance, passenger seating fit, legacy local-scene pony publication, or physical phone/Safari validation. The integration review retains those boundaries.
