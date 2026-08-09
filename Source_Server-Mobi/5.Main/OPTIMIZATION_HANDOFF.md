# MU Mobile — performance work handoff

Living notes for continuing the Android client optimisation. Written so a new
session (or another person) can resume without the original conversation.

## Where things stand

Same Lorencia scene, RedMagic 8 Pro (NX729J): **~8.5 → ~18.3 FPS, no visual change.**
Target is 26 FPS.

Current frame (`scn` ≈ 51 ms, `pres` 0 ms):

| bucket | ms | notes |
|---|---|---|
| `ui` | 15 | suspiciously high for 2D work — likely same re-upload pattern |
| unattributed in `scn` | ~19 | effects / sprites / particles / joints |
| `chrR` | 14 | was 50; mostly fixed |
| `objR` | 2 | was 33; fixed |
| `ter` | 1 | fine |

## The one finding that matters

**The client is CPU-bound, not GPU-bound. `pres` is 0 ms — the GPU is idle.**

Both big wins came from the same root cause: *the CPU redoing immutable work
every frame.* Expect the remaining hotspots to be the same bug in another place.

Ruled out by measurement (do not re-litigate):

- **Fill rate** — render scale at 0.75 (44% fewer pixels) bought ~0.3 FPS.
- **CPU skinning maths** — `BMD::Transform` is 0.8 ms of a ~110 ms frame.
- **GPU back-pressure** — `pres` 0 ms, always.
- **GPU skinning itself** — A/B on the same device/scene was neutral
  (`chrR` 52 both ways). The wins came from *how* geometry was submitted,
  not from moving bone maths to the shader.

## Method that worked

`adb logcat` returns nothing on these retail "user" builds, and the engine's
`PERF_LOGI` telemetry is therefore unreachable. Two workarounds:

1. **On-screen counters.** Temporary `g_Prof*` globals rendered into the FPS
   overlay in `ZzzScene.cpp` (~line 3010). This is how every number above was
   obtained. Read them by `adb shell screencap` + pull + view the PNG.
2. **Crash tombstones** via `adb shell dumpsys dropbox --print`. This is how
   the Devias crash was found (`logcat -b crash` is also empty).

Measure before changing. Three of four hypotheses this session were wrong, and
each was killed by one cheap build instead of a risky rewrite.

## Next steps, in order

1. **`ui` 15 ms** — find what the 2D/UI path re-uploads per frame.
2. **The ~19 ms unattributed** — add `g_Prof*` buckets for effects/sprites/
   particles/joints. Note `ZzzEffectBlurSpark.cpp`, `ZzzEffectMagicSkill.cpp`,
   `Sprite.cpp`, `ShadowVolume.cpp`, `SideHair.cpp`, `PhysicsManager.cpp`,
   `CSWaterTerrain.cpp` still use `glBegin` immediate mode — the slow path,
   measured at 4.5× the cost per mesh of the GPU path.
3. Consider static VBOs for terrain and static world props (same fix as
   commit `258cd36`, applied elsewhere).

## Temporary scaffolding — REMOVE BEFORE RELEASE

- `g_Prof*` counters and the two extra overlay lines (`ZzzScene.cpp`).
- `g_GpuSkinningTestEnabled` kill switch (`ZzzBMD.cpp`) — set `false` to fall
  back to the CPU path everywhere with no other change. Useful for A/B.
- `g_RenderScaleX/Y` in `android_main.cpp` (currently 0.75). `1.0` = off.
  Worth keeping below 1.0 for heat/battery even though it does not buy FPS.

## Known-good restore points

- Tag `known-good-gpu-skinning`, branch `backup/pre-cpu-skinning-removal`.
- `_backup_apk/app-known-good-gpu-skinning.apk` (git-ignored) — installable
  immediately without an ~8 minute rebuild.

## Gotchas that cost time

- **Two frame loops exist.** The live one is sokol_app's
  `frame_cb → OnAndroidSappFrame → RunAndroidGameFrame`. The old SDL
  `while (!Destroy)` loop lower in `android_main.cpp` is inside `#if 0` **and**
  its `Scenes/*.cpp` support files are excluded from the CMake build. Editing
  it does nothing and it will not even link.
- **The engine resets the viewport itself** (`glViewport2(0,0,WindowWidth,
  WindowHeight)` in a dozen files). Render scaling therefore works by telling
  the engine a smaller screen size, not by shrinking the render target alone.
- **Touch normalises against the physical size**, not the drawable size —
  sokol reports touch in physical pixels.
- **GLES guarantees only 16 vertex attribute locations.** Using >= 16 makes
  the program fail to *link*, silently, because `LOGE` is compiled out in
  `gl_compat.cpp`. Cost a full debugging cycle.
- **A `mat4` uniform consumes four consecutive locations.** Overlapping them
  is invalid GLSL that Adreno happens to tolerate.

## Unrelated bugs fixed along the way

- `CHARACTER::ID` is `char[32]` but `MonsterScript[].Name` is `char[64]`.
  Any monster with a name over 31 chars aborted the process via FORTIFY, on
  any map. Fixed with bounded copies; **long names now render truncated**.
  The cleaner fix is widening `CHARACTER::ID` to 64.
- A separate **exit-time crash** still exists (`_sapp_android_on_destroy` →
  `CUITextInputBox` dtor → null deref in `AndroidGetEditControl`). Pre-existing,
  cosmetic, not investigated.
