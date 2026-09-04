#include "stdafx.h"
#include "Platform/UIAtlas.h"

#if defined(__ANDROID__) || defined(MU_IOS)

#include "GlobalBitmap.h"
#include "ZzzTexture.h"
#include "NewUIInventoryCtrl.h"
#include "NewUIMainFrameWindow.h"
#include <GLES3/gl32.h>
#include <vector>

// ZzzOpenglUtil.cpp's BindTexture() shadow - see the note in GlobalBitmap.cpp.
extern int CachTexture;

namespace
{
	// Padding between packed images and around the sheet edge, so bilinear
	// filtering never samples a neighboring slot's pixels (texture bleeding).
	const int kAtlasPadding = 2;

	struct PlacedImage
	{
		GLuint id;
		int x, y, w, h;
	};

	// Shelf-packs bitmaps already resident in Bitmaps[] (Width/Height/Buffer
	// populated by the normal LoadBitmap path) into one new RGBA atlas
	// texture, then marks each source BITMAP_t as an atlas member so
	// ZzzOpenglUtil.cpp's ResolveUIAtlas transparently redirects draws at
	// it. Good enough for a handful of small, similarly-sized UI pieces;
	// not meant to scale to hundreds of assets - that's the offline packer
	// tool described in the UI-atlasing plan, not this pilot.
	void PackBitmapsIntoAtlas(const std::vector<GLuint>& ids)
	{
		std::vector<PlacedImage> placed;
		placed.reserve(ids.size());

		// Sheet width must fit the single widest source image (plus padding
		// on both sides) or the shelf-wrap check below can never trigger for
		// that image, leaving it placed with p.x + p.w past the sheet edge -
		// glTexSubImage2D would then write outside the destination texture.
		// A HUD gauge fill can easily be wider than the 256px this started
		// at (a full-width bar), so this has to be sized from the actual
		// inputs, not assumed.
		int maxSingleWidth = 0;
		for (size_t i = 0; i < ids.size(); ++i)
		{
			BITMAP_t* b = Bitmaps.FindTexture(ids[i]);
			if (b != NULL && b->Buffer != NULL && b->Width > 0.f && b->Height > 0.f)
			{
				const int w = static_cast<int>(b->Width);
				if (w > maxSingleWidth)
				{
					maxSingleWidth = w;
				}
			}
		}

		int sheetW = 256;
		while (sheetW < maxSingleWidth + 2 * kAtlasPadding)
		{
			sheetW *= 2;
		}
		int sheetH = 256;
		int cursorX = kAtlasPadding;
		int cursorY = kAtlasPadding;
		int shelfHeight = 0;

		for (size_t i = 0; i < ids.size(); ++i)
		{
			GLuint id = ids[i];
			BITMAP_t* b = Bitmaps.FindTexture(id);
			if (b == NULL || b->Buffer == NULL || b->Width <= 0.f || b->Height <= 0.f)
			{
				// Not loaded (or failed to load) - leave this ID as a normal,
				// non-atlased texture. ResolveUIAtlas no-ops for it.
				continue;
			}

			int w = static_cast<int>(b->Width);
			int h = static_cast<int>(b->Height);

			if (cursorX + w + kAtlasPadding > sheetW)
			{
				cursorX = kAtlasPadding;
				cursorY += shelfHeight + kAtlasPadding;
				shelfHeight = 0;
			}

			PlacedImage p;
			p.id = id;
			p.x = cursorX;
			p.y = cursorY;
			p.w = w;
			p.h = h;
			placed.push_back(p);

			cursorX += w + kAtlasPadding;
			if (h > shelfHeight)
			{
				shelfHeight = h;
			}
		}

		if (placed.empty())
		{
			return;
		}

		int usedHeight = cursorY + shelfHeight + kAtlasPadding;
		while (sheetH < usedHeight)
		{
			sheetH *= 2;
		}

		GLuint atlasTexture = 0;
		glGenTextures(1, &atlasTexture);
		glBindTexture(GL_TEXTURE_2D, atlasTexture);
		// Zero-fill the sheet up front (not NULL) - the padding gaps between
		// packed images are never written by the per-image glTexSubImage2D
		// calls below, and with GL_LINEAR filtering a UV sample right at a
		// packed image's edge bilinear-blends with whatever's in that gap.
		// Left as driver-allocated NULL data, that's uninitialized GPU
		// memory - garbage that reads as a solid-looking but wrong color
		// (black, gray, stray colored specks) and, since the atlas is built
		// once and never changes, shows up as the SAME artifact at the SAME
		// screen position every frame. Zero-filling guarantees the gap is at
		// least defined (transparent black) instead of undefined memory.
		{
			std::vector<unsigned char> zeroFill(static_cast<size_t>(sheetW) * sheetH * 4, 0);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sheetW, sheetH, 0, GL_RGBA, GL_UNSIGNED_BYTE, zeroFill.data());
		}
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		std::vector<unsigned char> rgbaScratch;

		for (size_t i = 0; i < placed.size(); ++i)
		{
			const PlacedImage& p = placed[i];
			BITMAP_t* b = Bitmaps.FindTexture(p.id);
			if (b == NULL || b->Buffer == NULL)
			{
				continue;
			}

			const unsigned char* srcPixels = b->Buffer;
			if (b->Components != 4)
			{
				// glTexSubImage2D needs a format matching the destination
				// (RGBA); widen 3-component source pixels, forcing full
				// opacity, into a scratch buffer first.
				rgbaScratch.resize(static_cast<size_t>(p.w) * p.h * 4);
				const int srcComponents = b->Components;
				for (int px = 0; px < p.w * p.h; ++px)
				{
					rgbaScratch[px * 4 + 0] = b->Buffer[px * srcComponents + 0];
					rgbaScratch[px * 4 + 1] = b->Buffer[px * srcComponents + 1];
					rgbaScratch[px * 4 + 2] = b->Buffer[px * srcComponents + 2];
					rgbaScratch[px * 4 + 3] = 255;
				}
				srcPixels = rgbaScratch.data();
			}

			glTexSubImage2D(GL_TEXTURE_2D, 0, p.x, p.y, p.w, p.h, GL_RGBA, GL_UNSIGNED_BYTE, srcPixels);

			b->bAtlasMember = true;
			b->AtlasTextureNumber = atlasTexture;
			b->AtlasU = static_cast<float>(p.x) / static_cast<float>(sheetW);
			b->AtlasV = static_cast<float>(p.y) / static_cast<float>(sheetH);
			b->AtlasUWidth = static_cast<float>(p.w) / static_cast<float>(sheetW);
			b->AtlasVHeight = static_cast<float>(p.h) / static_cast<float>(sheetH);
		}

		// The atlas build bound textures raw; resync BindTexture's shadow so it
		// cannot skip a later real bind (see GlobalBitmap.cpp for the full note).
		CachTexture = 0x7FFFFFFF;
		glBindTexture(GL_TEXTURE_2D, 0);
	}
}

void MU_BuildUIAtlasPilot()
{
	static bool s_built = false;
	if (s_built)
	{
		return;
	}
	s_built = true;

	// Pilot set: the shared item-table CORNER pieces only (14x14, drawn at
	// or near 1:1 scale). Reused as-is (same numeric IDs) by well over a
	// dozen other window classes via their own aliased enum constants
	// (search CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_ across the source
	// tree). Already identified as a real, measured bottleneck:
	// NewUIGuildInfoWindow.cpp's Render_Guild_History comment traces the
	// per-piece immediate-mode draws for one window's border alone to ~830
	// draw calls pinning the phone at 6 FPS before that particular window
	// switched to fewer, larger draws - this atlas is the general fix,
	// letting the existing batcher coalesce the draws instead.
	//
	// Deliberately NOT included here: IMAGE_ITEM_TABLE_TOP_PIXEL/
	// BOTTOM_PIXEL/LEFT_PIXEL/RIGHT_PIXEL. Those source images are only
	// 1x14 / 14x1 (newui_item_table03(Up/Dw/L/R).tga - a single-texel-wide
	// strip meant to be stretched across a whole window edge, e.g.
	// ~700px for the inventory window's top/bottom frame -
	// NewUIInventoryCtrl.cpp:981-982). That's several-hundred-times
	// horizontal magnification of ONE atlas-packed texel column: any
	// bilinear sample landing a fraction of a texel past this image's own
	// edge (into a packed neighbor or the padding gap) gets stretched
	// across hundreds of screen pixels into an obvious, wide artifact -
	// this is what caused the black/white-bar/green-speck glitch reported
	// in the inventory window's top and bottom frame seams. These four
	// draw only twice per window (not once per grid cell like
	// IMAGE_ITEM_SQUARE below), so atlasing them was never a meaningful
	// batching win to begin with - leaving them as standalone textures
	// costs nothing and removes the only atlas members at real risk of
	// visible edge bleed under extreme stretch.
	std::vector<GLuint> ids;
	ids.push_back(SEASON3B::CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_TOP_LEFT);
	ids.push_back(SEASON3B::CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_TOP_RIGHT);
	ids.push_back(SEASON3B::CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_BOTTOM_LEFT);
	ids.push_back(SEASON3B::CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_BOTTOM_RIGHT);

	// IMAGE_ITEM_SQUARE: the empty-slot background, drawn once per inventory
	// grid cell (NewUIInventoryCtrl.cpp:967, inside the grid's x/y loop) -
	// ~100+ draws for a full inventory grid, interleaved with item icons and
	// the border pieces above whenever a cell is occupied, which is exactly
	// the "different texture every call" pattern that defeats the existing
	// same-texture batcher. Highest single repeat-count candidate found so
	// far, hence first addition past the pilot border set.
	ids.push_back(SEASON3B::CNewUIInventoryCtrl::IMAGE_ITEM_SQUARE);

	PackBitmapsIntoAtlas(ids);
}

void MU_BuildSkillBoxAtlas()
{
	static bool s_built = false;
	if (s_built)
	{
		return;
	}
	s_built = true;

	// IMAGE_SKILLBOX / IMAGE_SKILLBOX_USE: the normal vs active-slot
	// background, drawn once per visible skill slot (NewUIMainFrameWindow.cpp,
	// ~5-6 draws for the hotkey bar, up to MAX_MAGIC when the full picker is
	// open) - mutually exclusive per slot, same "shared, reused, loaded early"
	// character as the inventory border pieces.
	std::vector<GLuint> ids;
	ids.push_back(SEASON3B::CNewUISkillList::IMAGE_SKILLBOX);
	ids.push_back(SEASON3B::CNewUISkillList::IMAGE_SKILLBOX_USE);

	PackBitmapsIntoAtlas(ids);
}

void MU_BuildHudGaugeAtlas()
{
	static bool s_built = false;
	if (s_built)
	{
		return;
	}
	s_built = true;

	// HP/MP/SD/BP/EXP gauge fills - 5-6 RenderBitmap draws every single
	// frame regardless of what UI is open (NewUIMainFrameWindow.cpp's
	// RenderLifeMana/RenderGuageSD/RenderGuageAG/RenderExperience and their
	// SS2-skin equivalents). Smaller draw count than the border-piece case,
	// but a guaranteed background tax on every frame rather than only when a
	// specific window is open.
	std::vector<GLuint> ids;
	ids.push_back(SEASON3B::CNewUIMainFrameWindow::IMAGE_GAUGE_RED);
	ids.push_back(SEASON3B::CNewUIMainFrameWindow::IMAGE_GAUGE_GREEN);
	ids.push_back(SEASON3B::CNewUIMainFrameWindow::IMAGE_GAUGE_BLUE);
	ids.push_back(SEASON3B::CNewUIMainFrameWindow::IMAGE_GAUGE_AG);
	ids.push_back(SEASON3B::CNewUIMainFrameWindow::IMAGE_GAUGE_SD);
	ids.push_back(SEASON3B::CNewUIMainFrameWindow::IMAGE_GAUGE_EXBAR);
	ids.push_back(SEASON3B::CNewUIMainFrameWindow::IMAGE_MASTER_GAUGE_BAR);

	PackBitmapsIntoAtlas(ids);
}

#endif
