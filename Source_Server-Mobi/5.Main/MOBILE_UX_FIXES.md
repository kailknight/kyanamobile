# MU Mobile — touch/UI fix notes

Living notes on the mobile client's input and UI defects, written so a new
session can resume without the original conversation. Companion to
`OPTIMIZATION_HANDOFF.md`, which covers frame time rather than behaviour.

Every entry records the **root cause**, not just the change, because most of
these were mis-diagnosed at first and the wrong fix looked plausible.

---

## The recurring shapes

Four failure patterns account for nearly every bug in here. Check these first.

### 1. Hover does not exist on touch

The desktop client feeds `SelectedCharacter` and most tooltip/highlight state
from the mouse *sweeping over* things. A finger only produces discrete taps, so
anything built on hover degrades to "tap it first", or to nothing.

Seen in: player names needing a tap; the equipment/bag tooltip gates; buff
skills self-casting.

### 2. Mixed coordinate spaces in a hit test

Positions are routinely converted into the 640×480 space `MouseX`/`MouseY` live
in — `(XPos + ScaleLoginMetric(k)) / g_fScreenRate_x` — while the **sizes**
passed alongside them are left in window/physical units. The box is then
`g_fScreenRate` times too large and swallows its neighbours.

The reliable way to settle it: find how far apart consecutive items are drawn
*in the same expression*, and require the box to match that. Do not reason about
what a scaling helper's units "mean".

Seen in: the saved-account list (a row box covered ~3 rows).

### 3. Login scene is not the main scene

A large amount of UI is constructed in `CNewUISystem::LoadMainSceneInterface()`,
**not** `Create()`. Before the main scene exists those pointers are null and the
objects that drive them never run. `g_pBCustomMenuInfo` is the usual casualty.

Consequence: anything that renders via `gInterface.Work()` is invisible on the
login screen, and `NULL`-derefs if you call it directly.

Seen in: registration feedback (see below).

### 4. A modal that cannot be dismissed becomes a permanent mute

`Interface::OpenMessageBox` starts with

```cpp
if (gInterface.Data[eWindowMessageBox].OnShow) return;
```

so a box that is set but never rendered (and therefore never dismissed) silently
discards **every subsequent message**. The symptom is not "no dialog" — it is a
feature that stops responding entirely, forever.

Whenever a request seems to do nothing at all, check whether `OnShow` is latched.

---

## Fixed

### Registration gave no feedback; button looked dead

`SubmitRegistration` / `RecvKQRegInGame` report through
`gInterface.OpenMessageBox`, painted only by
`Interface::DrawMessageBox` ← `gInterface.Work()` ←
`CNewUIBCustomMenuInfo::Render()`. That object is built in
`LoadMainSceneInterface()`, so **nothing drew it on the login screen**
(pattern 3). The first message then latched `OnShow` and muted the rest,
including the success confirmation (pattern 4).

`AndroidRenderLoginNotice` (android_main.cpp) drains the pending box into its own
panel and **releases `OnShow`** — that release is the load-bearing half. Drawn
last in `NewRenderLogInScene`'s 2D pass; tap-handled *before* the register
overlay, whose catch-all claims every tap on its panel.

SEASON3B's `g_MessageBox` is not an alternative: created in `Create()` but only
`Show(true)`n in `LoadMainSceneInterface()`.

### Buff skills always self-cast

The virtual pad overwrote the target just before firing, for every
`eTypeSkill_Buff` / `eTypeSkill_FrendlySkill`:

```cpp
SelectedCharacter = GetHeroCharacterIndex();   // "safer when bound to self"
```

`EnsureSupportSkillTarget` now prefers the AIM lock, then `SelectedCharacter`,
then self. The AIM picker already suits this and needed no change:
`IsAndroidTargetPickerCandidate` is players-only, and `IsValidAutoCombatTarget`
has **no attackability test**, so friendly players pass.

Two things worth keeping:

- The lock must outrank `SelectedCharacter` — auto-acquire rewrites the latter to
  the nearest monster on every offensive cast.
- There were **two** copies of the self-bind (skill arc and Q/W/E/R hotkey).
  Fixing one produces an intermittent-looking bug.

### Player names required a tap

`RenderName` shows all names only when `g_bGMObservation`. `Winmain.cpp` seeds it
from `mShowName`; nothing did on mobile, while `android_link_stubs.cpp` already
had `mShowName = 1` — so the Options "Show Name" checkbox read *checked* with
names off. Seeded in `InitializeTakumiProtectState`.

### Saved-account list: two rows lit, wrong one selected

Pattern 2. Row boxes were `KC` tall while rows are drawn `KC / g_fScreenRate_y`
apart. The loop takes the first matching `i`, so the topmost overlapping row
always won. Sizes now divided; helpers shared between `DrawInfo`'s highlight and
`HitsControlArea`'s dispatch, which mirror each other by hand.

Affects PC too (`g_fScreenRate` is `WindowWidth/640` there, not 1).

### Drop-item names printed white

`RenderItemName`/`RenderItemNameS6` pick their colour by writing `glColor3f` then
reading it back with `glGetFloatv(GL_CURRENT_COLOR)`. That is fixed-function
state GLES3 does not have, so the call raised `GL_INVALID_ENUM` and wrote
nothing, leaving the caller's `{1,1,1,0}` initialiser — every channel ≥ 0.9, so
the override was skipped. `GL_GetFloatv` now answers `GL_CURRENT_COLOR` from
`s_cur`, which *is* this layer's current colour.

Not fixed: `TEXT_COLOR_GREEN_BLUE` (ancient) is commented out in
`SetColorItemNameS6`, so ancient inherits a stale colour — identical on PC, so
it is pre-existing rather than a mobile gap.

### Warehouse hold-to-move only worked one way

Depositing already worked (the hold's right-click pulse reaches
`HandleInventoryActions`). Withdrawing could not arm at all, because
`FindAndroidBagItemAndCtrlAt` only searched the bag. Same asymmetry the craft box
had. The never-hotkey-bind guard had to be extended too — a vault is mostly
jewels and potions, which pass `CanRegisterItemHotKey`.

### 576p render target broke text layout

`fontSize = ceil(12 + (WindowHeight - 480)/200)` is only weakly tied to render
height while `g_fScreenRate_*` are strictly proportional. Pinning height to 576
raised logical text width ~35%, centring went negative, and `AndroidTextOut`'s
`dstX < 0` guard dropped leading glyphs ("ccount", "assword").

Reverted to the **fractional** 0.75 scale, which keeps font size and screen rate
moving together. An absolute target needs a proportional font pass first — a
whole-UI visual change, not a constant.

---

## Known-bad, not yet fixed

- **`AndroidTextOut`'s clear loop has no negative-`x` guard.** It does
  `bmp->data + dstY * pitch + x * 3` while the copy loop below correctly checks
  `dstX < 0`. Any negative-x write memsets before the row start — a real
  out-of-bounds write, reachable via the overflow case above.
- **`OBJECT::Owner` can be garbage.** Contained only: `IsCharacterOwnedObject`
  verifies the pointer is really a `&CharactersClient[n].Object`, and
  `ReportBadObjectOwner` logs Type/SubType to `mu_badowner.txt`. The cause of the
  stale pointer is still unknown — that file names the culprit when it recurs.
- **Hardcoded Vietnamese remains in the client.** `SubmitRegistration` has
  "Vui long nhap tai khoan", "Thao tac cham lai" and others. An accent-based
  grep misses these: they are unaccented. Server-side strings were moved to
  `Message.txt` (3076-3125); the client's equivalent is `GlobalText`.

---

## Practical notes

- **`android_main.cpp` carries an uncommitted change that is not ours** — a
  message-box / register-overlay input reorder. It has been split out of every
  commit in this series. Verify `git diff` on that file before committing.
- **`kyana.ah` must be re-patched after every `Main.exe` rebuild.** MUGUARD
  stores the exe's CRC-32 at byte offset **228**, obfuscated per byte as
  `enc = (raw ^ 0x7D) - 0xF5`. Deploy the exe **first**, then the CRC — a
  mismatch makes the client exit with no message. The running test client holds
  a write lock on `ClientBuild\Main.exe`.
- **`CBGetMain.bin` and `CBTextInfo.bin` are different blobs.**
  `MAIN_FILE_INFO` config is in the former; buff/item tooltip tables are in the
  latter. `ReadMainFile` compares the file size *exactly*, so a struct change
  means shipping the blob and the exe together.
- **logcat is empty for this app.** `fopen` traces are the proven route; the tag
  would be `MuMain`.
