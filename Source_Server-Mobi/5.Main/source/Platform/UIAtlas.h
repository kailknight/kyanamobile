#pragma once
// =============================================================================
// Platform/UIAtlas.h
// UI texture atlasing pilot (mobile only).
//
// The 2D UI/HUD layer (RenderBitmap/RenderSprite family, ZzzOpenglUtil.cpp)
// still draws one glBegin/glEnd quad per call, and every call starts with
// BindTexture() on a distinct texture ID - so the immediate-mode batcher in
// Platform/gl_compat.cpp, which only coalesces consecutive same-texture
// quads, essentially never triggers for UI (a window border alternates
// between several different border-piece textures, an icon grid between many
// different icon textures, etc). Packing several small, widely-reused UI
// textures into one shared atlas texture lets consecutive draws bind the
// same GL texture even though each call still thinks it's drawing a
// different logical image, so the existing batcher can coalesce them for
// free - see GlobalBitmap.h's BITMAP_t comment and
// ZzzOpenglUtil.cpp's ResolveUIAtlas for the draw-time side of this.
// =============================================================================

#if defined(__ANDROID__) || defined(MU_IOS)

// Packs the pilot set of shared UI border-piece textures
// (CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_* - reused as-is by over a dozen
// other windows via their own aliased enum constants, e.g.
// NewUIGuildInfoWindow.h's IMAGE_GUILDINFO_*) into one small shared atlas
// texture. Call once, after these bitmaps are known to be loaded (currently
// hooked at the end of CNewUIInventoryCtrl::LoadImages(), which loads this
// exact set) - safe to call more than once, only the first call does
// anything.
//
// Known limitation: CGlobalBitmap::UnloadAllImages() deletes every loaded
// bitmap unconditionally, ignoring refcount, which would drop the source
// pixel data this pilot's atlas fields point at without re-running this
// function. If that ever proves to matter in practice (a full asset reload
// mid-session), the fix is re-invoking this after such a reload - not
// something this pilot needs to solve up front.
void MU_BuildUIAtlasPilot();

// Packs the skill-slot background pair (CNewUISkillList::IMAGE_SKILLBOX /
// IMAGE_SKILLBOX_USE - normal vs active/selected slot) into their own small
// atlas. These two are mutually exclusive per slot (never both drawn for the
// same slot the same frame) and are reused across the hotkey bar (5-6 slots)
// and the full skill picker (up to MAX_MAGIC slots when open). Call once,
// after these are loaded - currently hooked at the end of
// CNewUISkillList::LoadImages(), which loads this exact set.
void MU_BuildSkillBoxAtlas();

// Packs the always-on-screen HUD gauge textures (HP/MP/SD/BP/EXP -
// CNewUIMainFrameWindow::IMAGE_GAUGE_RED/GREEN/BLUE/AG/SD/EXBAR plus
// IMAGE_MASTER_GAUGE_BAR) into their own small atlas. Small draw count (5-6
// bars/frame) but guaranteed every single frame regardless of what UI is
// open. Call once, after these are loaded - currently hooked at the end of
// CNewUIMainFrameWindow::LoadImages(), after both the SS2-skin and
// default-skin branches (which load the same IDs from different files, so
// either one leaves the set fully loaded by the time this runs).
void MU_BuildHudGaugeAtlas();

#endif
