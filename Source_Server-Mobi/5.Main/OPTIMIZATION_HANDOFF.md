# MU Mobile — performance work handoff

Living notes for continuing the Android client optimisation. Written so a new
session (or another person) can resume without the original conversation.

## Where things stand

Same Lorencia scene, RedMagic 8 Pro (NX729J), Adreno:
**~8.5 → ~17 → 30-36 FPS in the busy town, 58-62 in open areas.**
The 26 FPS target is met **on this device**. See the MediaTek section below —
it is not met everywhere.

Frame at the end of the session (busy town, `scn` ≈ 21 ms, `pres` 0 ms):

| bucket | ms | notes |
|---|---|---|
| `chrR` | 4 | was 14-16; `post` 10 → 2 |
| `ui` | 5 | was 13-16 |
| `par` | 4-7 | particles, now the largest single bucket |
| `objR` | 1-8 | 8 ms in open plazas with ~334 visible objects |
| `spr` | 0.1-5 | sprites |
| `ter` | 0-1 | fine |

## The one finding that matters

**The client is CPU-bound, not GPU-bound. `pres` is 0 ms — the GPU is idle.**

Every win so far came from the same root cause: *the CPU redoing immutable work
every frame.* Expect the remaining hotspots to be the same bug in another place.

Ruled out by measurement (do not re-litigate):

- **Fill rate** — render scale at 0.75 (44% fewer pixels) bought ~0.3 FPS.
- **CPU skinning maths** — `BMD::Transform` is 0.8 ms of a ~110 ms frame.
- **GPU back-pressure** — `pres` 0 ms, always.
- **GPU skinning itself** — A/B on the same device/scene was neutral.
- **Missing frustum culling** — culling already runs at three levels in
  `RenderObjects` (per 16x16 block, per object, plus adaptive distance
  buckets). The `cull obj X/Y` overlay line shows what survives it.

## The big win this session

`gl_compat.cpp` orphaned the streaming VBO at `s_vboCapacity` — the high-water
mark across every batch in the session, which the bulk vertex-array draws push
into the hundreds of KB — before **every** draw, including 36-byte UI quads. At
~600 immediate-mode draws a frame that is hundreds of megabytes of driver
allocation per frame. Orphaning at the size actually being drawn fixed it:

| | before | after |
|---|---|---|
| FPS (same scene, D 467) | 17.3 | 29.7 - 36.3 |
| frame `scn` | 56 ms | 21 - 29 ms |
| 2D quad path `bmp` | 8.2 ms / 99 calls | 3.3 ms / 97 |
| character `post` | 10 ms | 2 ms |

## MediaTek / Mali — measured

Device: realme RMX5000, **MT6878 (Dimensity 7300), Mali-G615**, Android 16,
1080x2400. The "~5 FPS" report was the **pre-fix** build.

With the current build it runs a **stable 28-29.5 FPS** (`scn` 32-38 ms):
eight consecutive samples read 29.0 / 29.1 / 29.5 / 29.2 / 28.1 / 28.9 / 28.7 /
28.4. No jumping.

`pres` is 0-1 ms, so **Mali is CPU-bound too** — the diagnosis transfers.

**The jumping is a first-run effect.** Immediately after the initial 552 MB
`data.zip` download and extraction, frames spiked hard (FPS 7.4, `scn` 132 ms,
`objR` 58 ms, `chrR` 36 ms) while pure-CPU work stayed flat (`out`, the FreeType
rasterise, held at ~4 ms). After a clean relaunch with the data already
extracted it is stable. Treat spikes as asset streaming, and re-check when
entering a new map rather than assuming a steady-state problem.

**Ruled out by measurement — do not retry:** disabling VBO orphaning on Mali.
The theory was that Mali recycles buffer allocations from a pool and our
per-draw `glBufferData` exhausts it. `GL_SetSkipVBOOrphan(true)` made things
catastrophically worse, not better:

| | orphan on | orphan off |
|---|---|---|
| FPS | 28-29 | **3.1 - 3.8** |
| `scn` | 32-38 ms | 112 - 278 ms |
| `bmp` (2D quads) | 0.2 ms / 103 | **14.8 - 56.3 ms** / 102 |
| `ui` | 4 ms | 30 - 74 ms |

Without orphaning, `glBufferSubData` overwrites a buffer the GPU is still
reading and the driver stalls. Per-draw orphaning is correct on both GPUs.
The `g_ForceSkipVBOOrphan` switch in `android_main.cpp` is left in place,
defaulted off, with this result recorded at the call site.

**Where the frame actually goes on Mali** — text is the standout:

`text n18 h0 m0 | ext 0.7 out 3.6 wr 0.3 up 6.5` ≈ **11 ms of a 33 ms frame,
33%**, versus roughly 10% on Adreno. `up` (texture upload + draw) is ~360 µs
per string here against ~76 µs on Adreno, so Mali's `glTexSubImage2D` path is
about 5x more expensive. Two consequences:

1. **The profiling overlay is now a significant distortion.** Eight of those 18
   strings are the overlay's own lines, they are long, and they change every
   frame so they can never be cached. On this device the instrumentation costs
   roughly 4-5 ms/frame — about 15% of the frame. Numbers taken with it on are
   pessimistic; strip it before judging real-world performance.
2. **Fixing the text section cache matters far more here than on Adreno.** It
   targets exactly the `out` + `up` cost, which is ~10 ms/frame on Mali against
   ~3 ms on Adreno. See the Text rendering section for the bug that has it
   disabled.

## Method that worked

`adb logcat` returns nothing on these retail "user" builds, and the engine's
`PERF_LOGI` telemetry is therefore unreachable. Two workarounds:

1. **On-screen counters.** Temporary `g_Prof*` globals rendered into the FPS
   overlay in `ZzzScene.cpp`. This is how every number here was obtained. Read
   them by `adb exec-out screencap -p > file.png` and view the PNG.
2. **Crash tombstones** via `adb shell dumpsys dropbox --print`. Symbolize with
   the NDK's `llvm-addr2line -e <unstripped libmain.so> <pc>`; the unstripped
   library is under `android/app/build/intermediates/cxx/Debug/<hash>/obj/`.
   Check its Build ID matches the tombstone before trusting the line number.
3. **Frame-difference heat maps.** For flicker, capture consecutive frames with
   `screencap` and diff them pixel-wise. Solid blocks of change = something
   popping; speckle = normal animation. This is what found the cull oscillation
   and told it apart from a disconnect dialog that happened to appear mid-capture.

Measure before changing. Most hypotheses this session were wrong, and each was
killed by one cheap build instead of a risky rewrite. Builds are incremental and
take ~20-45 s, so A/B on device is cheap — use it.

## Adaptive systems keyed on FPS — read this before touching frame pacing

`ZzzObject.cpp` throttles object rendering based on the frame rate, with hard
thresholds (58/52/50/44/42/40/34/32/28/24/15). Because culling changes the frame
rate, sitting on a threshold oscillates: cull → faster → stop culling → slower →
cull. At ~58 FPS this toggled 176 of 340 objects on and off between consecutive
frames, with fences and walls visibly flickering. It never showed at 17 FPS
because the client sat permanently below every threshold.

All of these now go through `AdaptiveFpsBelow(threshold, margin)`, which latches
per-threshold state over a ~20-frame smoothed frame rate with a dead band.

**Two consequences to keep in mind:**

- A dead band has to be reachable from both sides. The cull gate was originally
  `fps >= 58 → never cull`, which under a 60 FPS cap could latch on and never
  release. It is now centred at 52±4 (engages below 48, releases above 56).
- **Capping the frame rate for thermal reasons will engage the culling.** A 40
  FPS cap sits below 48, so ~100-150 objects get dropped permanently. If you cap
  the frame rate (`g_adaptivePerf.targetFps` / `SetTargetFps`), re-centre these
  gates or key them off frame-time headroom against the cap rather than
  absolute FPS.

## Text rendering

`CUIRenderTextOriginal::RenderText` re-measured, re-rasterised (FreeType),
re-colourised and re-uploaded every string every frame — ~9 ms/frame for ~18
strings. Two caches were written for it, behind separate kill switches in
`UIControls.cpp`:

- **`g_TextExtentCacheEnabled` (ON, working).** Measuring is a pure function of
  (bytes, font). `ext` 2.8 → 0.8 ms.
- **`g_TextSectionCacheEnabled` (OFF, buggy).** Caches composed pixels in a
  persistent 1024x2048 atlas. **Do not turn this on without fixing it first.**
  With it on, Character Info stat lines render as whatever the profiling overlay
  last drew, changing every frame — two entries end up sharing one atlas slot.
  The LRU eviction, stale-entry takeover and key derivation were all reviewed
  without finding how. The pad labels (PK/CHAT/JWL) were a *different* bug and
  are genuinely fixed (see below).

Two real bugs were found and fixed while doing this, both worth keeping:

- **Stale `CachTexture`.** `BindTexture` skips the real `glBindTexture` when its
  shadow says the texture is already bound, but lots of code raw-binds without
  updating that shadow — the virtual pad's PNG draws do it every frame. Any draw
  that does not re-bind explicitly can sample the wrong texture. The old text
  path was immune only because it re-bound the font texture before every draw.
- **The font DIB is never a clean slate.** `AndroidTextOut` clears only the box
  it draws into, so the scratch buffer keeps the tail of whatever longer string
  was rendered last. The old code got away with it because every string was
  re-rendered in draw order every frame. Both paths now clear the region
  `WriteText` is about to read.

## Data / code mismatch — causes crashes, not just wrong text

The device's `data.zip` ships a text table whose string IDs do not match this
client build. Decoding both (`Text_eng.bmd`, header `0x5447` + per-string
`{key,len}` XOR `fc cf ab`):

| entry | desktop `data/` | device |
|---|---|---|
| 2045 | `"Khả năng phòng thủ: %d"` | `"Slot %d: %s Option active"` |

`CNewUICharacterInfoWindow::RenderAttribute` passes **one** integer, so the
device's extra `%s` consumed an unset vararg as a pointer → SIGSEGV in `strlen`.
That is the "clicking CHAR crashes" bug. Pushing the matching `Text_eng.bmd`
to the device fixes it, but the real fix is in the data pipeline
(`PreloadActivity.java` downloads `data.zip`) — other users still crash.

**This is a whole class of bug.** There are dozens of
`_sprintf(buf, GlobalText[N], ints...)` calls whose format strings come from
data. Any mismatch segfaults. A sanitising formatter would turn those into
wrong-but-harmless text.

## Next steps, in order

1. **Fix the text section cache.** Highest value now: it is ~10 ms/frame on
   Mali and ~3 ms on Adreno, and it is the one piece of work already written
   but disabled. See the Text rendering section for the exact symptom.
2. **Strip or gate the profiling overlay** before any further judgement of real
   performance — it costs ~4-5 ms/frame on Mali.
3. **`par` (particles) 4-7 ms** — the largest remaining bucket on Adreno.
   `ZzzEffectBlurSpark.cpp`, `ZzzEffectMagicSkill.cpp`, `Sprite.cpp`,
   `ShadowVolume.cpp`, `SideHair.cpp`, `PhysicsManager.cpp`,
   `CSWaterTerrain.cpp` still use `glBegin` immediate mode.
4. **Static VBOs for world props.** ~334 visible objects cost ~8 ms in open
   areas, ~24 µs each, re-submitted every frame. Same fix pattern as the two
   wins so far, and no visual cost — unlike shortening draw distance.
5. **Split-resolution UI** (3D to a scaled FBO, upscale, then UI at native res)
   if UI sharpness matters. The FBO and blit already exist in
   `RenderBackend.cpp`; today the whole frame goes through them, which is why
   the UI is soft at render scale 0.75. Note this is a *quality* change, not a
   thermal one — fill rate was measured irrelevant here.

## Temporary scaffolding — REMOVE BEFORE RELEASE

- `g_Prof*` counters and the extra overlay lines (`ZzzScene.cpp`,
  `android_main.cpp`, `ZzzOpenglUtil.cpp`, `NewUIManager.cpp`, `UIControls.cpp`).
  The overlay itself costs ~1-2 ms and pollutes the text cache with strings that
  change every frame.
- `g_TextExtentCacheEnabled` / `g_TextSectionCacheEnabled` (`UIControls.cpp`).
- `g_GpuSkinningTestEnabled` kill switch (`ZzzBMD.cpp`).
- `g_RenderScaleX/Y` in `android_main.cpp` (currently 0.75). `1.0` = off.

## Known-good restore points

- Tag `known-good-gpu-skinning`, branch `backup/pre-cpu-skinning-removal`.
- `_backup_apk/app-known-good-gpu-skinning.apk` (git-ignored).

## Gotchas that cost time

- **Two frame loops exist.** The live one is sokol_app's
  `frame_cb → OnAndroidSappFrame → RunAndroidGameFrame`. The old SDL
  `while (!Destroy)` loop lower in `android_main.cpp` is inside `#if 0` **and**
  its `Scenes/*.cpp` support files are excluded from the CMake build.
- **The engine resets the viewport itself** (`glViewport2(0,0,WindowWidth,
  WindowHeight)` in a dozen files). Render scaling therefore works by telling
  the engine a smaller screen size — which is also why the UI is scaled.
- **Touch normalises against the physical size**, not the drawable size.
- **GLES guarantees only 16 vertex attribute locations.** Using >= 16 makes
  the program fail to *link*, silently, because `LOGE` is compiled out.
- **A `mat4` uniform consumes four consecutive locations.**
- **`fopen` debug logging does not work on device** — the cwd is not writable.
  Two such logs were found writing nothing. Use the overlay.
- **The client disconnects from the server periodically during long test
  sessions.** A "Connection to the server has been disconnected" dialog
  appearing mid-capture invalidated one flicker measurement. Check for it.
- **Git Bash mangles `adb` device paths.** Use `MSYS_NO_PATHCONV=1` for
  `adb push` / `adb pull`.
- **`glColor4fv` is a silent no-op stub on Android** (`Platform/PlatformDefs.h`),
  and `glGetFloatv(GL_CURRENT_COLOR, ...)` is not a valid GLES query either
  (leaves its output buffer uninitialized). `CUIRenderTextOriginal::UploadText`
  used exactly that pair to save/restore the pre-shadow color around a
  `SetShadowText` glyph draw — on Android the restore did nothing, so `glColor`
  stayed black from the shadow pass and every shadowed string rendered solid
  black regardless of `SetTextColor`. Fixed (`UIControls.cpp`, both the live
  path and the disabled text-section-cache path) by resetting to
  `glColor4f(1,1,1,1)` directly instead of round-tripping through the broken
  pair — the text color is already baked into the glyph texture, so the draw
  only ever needed a neutral white multiplier. Took three attempts to find
  because most on-screen text compiles under `#if(ShadowText)` with the macro
  at `0` (`Defined_Global.h`), so it never hits this path and looks fine;
  `RenderHPBar`/`RenderHPBarNew` call `SetShadowText` directly, unguarded,
  which is what made them the visible symptom.

## Unrelated bugs found along the way

- **`NewUIGuildInfoWindow.cpp` opened the Guild window and FPS fell to ~6.**
  Root cause: its decorative table borders draw the `IMAGE_GUILDINFO_*_PIXEL`
  textures (1 texel across their short axis) by looping 1 destination pixel at
  a time and issuing a fresh `RenderImage`/`RenderBitmap` call per pixel —
  `RenderBitmap` is `glBegin`/`glEnd` immediate mode, so each of those is a
  full driver round trip. The default tab alone (`Render_Guild_Enum`, shown on
  open since `m_nCurrentTab` starts at 1) issued ~830 such calls every frame.
  Fixed by collapsing each loop into a single stretched `RenderImage` call
  spanning the same destination rect — visually identical since the source
  texture is 1 texel wide/tall on that axis, and it's the same technique
  `NewUIInventoryCtrl.cpp:970` already uses for the same texture ID. Only
  `NewUIGuildInfoWindow.cpp` was touched; **other legacy windows may share the
  same per-pixel-loop pattern and are worth a grep** (`for(int x=...x++)` /
  `for(int y=...y++)` immediately followed by a 1-px-wide/tall `RenderImage`
  call) if another window turns up slow. A `CreateGuildMark`-per-frame
  `glTexImage2D` reupload on the same window's History tab was also noticed
  but **not** cached — `BITMAP_GUILD` is a texture slot shared with the trade
  window and the soccer scoreboard (each does create-then-immediately-draw),
  so caching by mark index risks showing a stale mark if another window wrote
  the slot in between frames. Left alone rather than risk that without a way
  to verify it visually.
- `CHARACTER::ID` is `char[32]` but `MonsterScript[].Name` is `char[64]`.
  Fixed with bounded copies; **long names now render truncated**. The cleaner
  fix is widening `CHARACTER::ID` to 64.
- A separate **exit-time crash** still exists (`_sapp_android_on_destroy` →
  `CUITextInputBox` dtor → null deref in `AndroidGetEditControl`).
- The custom "Menu" window shows "Could not find message N!" and overlapping
  label/value strings. **Pre-existing** — verified identical with all caching
  disabled. It is the `CCustomMessage` table, a different system from
  `GlobalText`.
