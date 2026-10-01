# World of Kira Android 1.0.24 (versionCode 24)

Release build: `./gradlew assembleRealDeviceRelease bundleRealDeviceRelease`
(no `-PmuDiag`, default ABIs arm64-v8a + armeabi-v7a, signed with
`keystore/world-of-kira-release.jks`).

- APK: `app/build/outputs/apk/realDevice/release/app-realDevice-release.apk`
- AAB: `app/build/outputs/bundle/realDeviceRelease/app-realDevice-release.aab`

None of the diagnostics are in this build. VpTrace (`mu_viewport_trace.txt`),
the drift log and the other file probes compile only with
`MU_DEV_DIAGNOSTICS`, which `-PmuDiag=true` turns on.

Shared source (`5.Main/source`) also changed, so the PC `Main.exe` gets the
minimap, party and viewport fixes too. After a PC rebuild, re-stamp the
MUGUARD CRC in `kyana.ah` (`Working Project\Tools\CrcStamp.exe`).

## Minimap (PC + mobile)

- **Corner minimap orientation (mobile).** `DrawRotatingMapQuad` sampled the
  texture upside down compared with the M map, so walking south moved the
  arrow north. The quad, the blips (`+normY`) and the facing cone
  (`sin a, cos a`) were fixed together. World +Y is up on every map now.
- **Tap-to-walk avoids walls.** The old auto-walk fanned a few headings toward
  the target and got stuck in any dead end. Now `PlanAutoRoute` runs A* over
  the full 256x256 `TerrainWall` grid (`TW_NOMOVE` = wall, no diagonal corner
  squeezing). The hero then walks the route in `PathFinding2` legs of up to 12
  tiles. Each leg is topped up at a tile boundary, so running doesn't drop to
  a walk between legs.
  - A tap on an unreachable tile walks to the nearest reachable one.
  - Getting knocked off the route re-plans up to 5 times.
  - The planned route is drawn as cyan dots on the M map.
- **Auto-walk cancels when** the hero gets any path the auto-walk didn't issue
  (joystick, click-to-move, attacking, the helper), or on Esc, death or a map
  change.
- **Party members on all minimaps.** Pink markers, with names on the M map.
  - `GetPartyMemberMapTile` finds visible members in `CharactersClient` by
    name. `Party[i].index` is only re-resolved while the party list window
    updates, so it can't be relied on.
  - Members out of sight use the 0x42 party-list position. The server sends
    that only on join/leave, so `RequestPartyPositions` re-requests 0x42 while
    a minimap is open: every 1.5 s on the M map, every 3 s on the corner maps.
    The server has no `HackPacketCheck` limit on 0x42.
  - Members on another map are not drawn.

## Reconnect after switching apps (mobile)

When the app was backgrounded for more than 60 s, the GameServer closed the
connection (`User.cpp` ConnectTickCount timeout). sokol parks the frame loop
while the activity is paused, so the 20 s `CheckHack` keepalive stopped.

A background thread in `android_main.cpp` now sends `CheckHack` every 15 s
between SUSPENDED/UNFOCUSED and RESUMED/FOCUSED.

**Known limit:** OEM cached-app freezers (TECNO/HiOS is aggressive) can freeze
the whole process. If reconnects still happen after long breaks, the next step
is a foreground service.

## Viewport ghost monsters / walking NPCs

- **Use-after-free on the packet being parsed.** The receive thread called
  `ClearGarbage()` and freed the packet the main thread was still parsing, so
  roughly 1 in 6 map joins created random monsters and garbage NPCs.
  - The garbage is now freed in `GetReadMsg` under the queue mutex.
  - `android_link_stubs.cpp` changed. `wsctlc_addon.h` (non-UTF-8) was left
    untouched.
- **Monsters that arrive by teleport.** For these, the server sets 0x40 in the
  0x13 KeyH. The client masked keys with 0x7FFF, which left frozen,
  unhittable monsters. It now masks with 0x3FFF. This is shared code, so PC
  is fixed too.

## Settings persistence (mobile)

Before this release every Options setting reset on launch, because
`GameConfig::Load/Save` and `GetPrivateProfileInt` were stubs on Android.

Settings now persist in `mu_settings.cfg` in the data root: key=value, written
via tmp+rename.
- `MobileSyncOptionSettings()` runs every frame and writes only when something
  changed.
- Audio, voice, graphics, option checkboxes, HP bar, names, minimap, title and
  item names are covered.
- Credentials and the server IP are deliberately not stored.

## Performance (crowds, Helio G85 / Mali-G52)

TECNO KL5 in a 20-player Lorencia crowd: 22 → 28-31 FPS.
- LOD for +15 armour ornaments (was 20% of the frame).
- Cached name-label text extents (HarfBuzz was re-measuring every frame).
- Bucketed particle and sprite batches.
- Persistent-mapped bone ring.
- Free-slot hints and high-water marks for sprite, joint and effect pools.
- `MuAudio.java` plays sounds off the game thread (no binder stall per
  footstep).
- Player and mount shadows are off on mobile. Monsters keep theirs.

## Other

- New joystick art (`assets/ui/joystick1.png`, `joystick2.png`). The crop
  constants are unchanged.
- Opening the party window hides the HUD except the chat box.
- `<profileable android:shell>` is `true` only in `-PmuDiag=true` builds, so
  simpleperf can profile a diagnostics release. It is `false` in this build.

## Not yet confirmed on device

Everything above except settings persistence (user-confirmed 1 Oct) still
needs a pass on real hardware. Check especially:
- the reconnect fix: leave the app for more than 60 s;
- party markers on the M map;
- auto-walk around walls;
- the joystick cancelling the auto-walk.
