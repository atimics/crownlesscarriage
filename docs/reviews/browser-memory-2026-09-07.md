# Why Safari reloads the browser game

Source: local Release web build of `97696d8`, measured with Playwright WebKit
26.5 and Chromium, 1280x900, opening town, audio on unless stated.

Nine merged changes since 2026-08-31 have reduced browser memory. Safari still
reloads the tab. The measurements below say why: the game's own memory was
never the cost, so shrinking it could not have fixed this.

## The game's memory is about fifty megabytes

Held steady over several minutes of play, in both engines:

| What | Size |
| --- | --- |
| WebAssembly heap | 22 MB |
| Textures | 2.6 MB across 6 objects |
| Vertex buffers | 6.4 MB across 509 objects |
| JavaScript heap | 16 MB |
| Canvas | 1280x720 at device pixel ratio 1 |

Compiling the 4.1 MB module costs 44 MB in WebKit and 8 MB in Chromium, so
neither code size nor Asyncify is the problem. `startup-memory.json`, which the
browser checks already write, measures this side of the ledger and looks healthy.

## The browser's memory is two and a half gigabytes

The WebKit web content process reaches **2.0-2.5 GB within about fifteen seconds
of gameplay and holds it**. Chromium peaks near 1 GB and falls back to 150 MB.
`vmmap` attributes the WebKit total to `WebKit Malloc`: 1.8 GB resident spread
across dozens of 32 MB chunks.

Resident size climbs in step with the volume uploaded to the GPU and then stops
at the allocator's high water mark:

| Frames drawn | Uploaded so far | Web process |
| --- | --- | --- |
| 45 | 4 MB | 876 MB |
| 139 | 286 MB | 1394 MB |
| 320 | 878 MB | 2064 MB |
| 3942 | 12730 MB | 2101 MB |

## Where the traffic comes from

Per frame, measured identically in both engines:

| Call | Per frame |
| --- | --- |
| `bufferSubData` | 162.7 calls, 3.1 MB |
| `drawElements` + `drawArrays` | 361.8 calls, 575346 vertices |
| `bindTexture` | 571.5 calls, over 7 distinct textures |
| `useProgram` | 736.3 calls |
| `bindVertexArray` | 513.7 calls |

The uploads land in exactly four vertex buffers, each taking 38.9
`bufferSubData` calls per frame in a 12:12:8:4 byte ratio — raylib's immediate
mode batch, flushed 39 times a frame. About 3.1 MB of geometry streams through
it every frame, roughly 170 MB/s.

**A third of that is the same bytes twice.** Counting uploads whose contents
match the previous upload to the same buffer: 15432 of 43301 on the second
buffer, 13390 of 43301 on the third.

The batch is flushed whenever a shader uniform has to change —
`SetWorldForegroundReveal` and `SetWorldTerrainSurface` in
`src/client/local3d/asset_loading.inc` both call `rlDrawRenderBatchActive()`
before setting theirs — and once per `EndMode3D`, of which the client has twelve
call sites across `actor_rendering.inc`, `road_book.inc` and `open_world.inc`.
Objects are drawn one at a time with their own state, which is why seven
textures cost 571 binds.

## What this change does

It adds the per-frame numbers to the browser checks as `frame-budget.json` with
ceilings, so the traffic is visible in CI and cannot quietly grow. The ceilings
record today's behaviour; they are not a target.

It also stops the music warmer reading each track into memory on its way to the
cache. That removes a copy of up to 16 MB per track across a 28 track library,
which is worth doing, but measured no change in the WebKit total: the ~400 MB
that disappears when audio is blocked entirely belongs to the playing track's
media pipeline, not to warming.

## What is left

Cutting the per-frame traffic is the fix, and it is a renderer change rather
than a memory change:

1. Group draws that share shader state so the batch is flushed a few times a
   frame instead of thirty-nine.
2. Skip uploads whose contents did not change since the last frame.
3. Merge the scenery drawn one model at a time into the cached meshes that
   terrain already uses, cutting draws and the state changes around them.

Each of these changes draw order or geometry residency in a scene with reveal
cutaways, so each wants a graphics capture pass beside it.
