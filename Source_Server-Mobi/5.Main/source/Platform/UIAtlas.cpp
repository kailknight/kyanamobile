#include "stdafx.h"
#include "Platform/UIAtlas.h"

#if defined(__ANDROID__) || defined(MU_IOS)

#include "GlobalBitmap.h"
#include "ZzzTexture.h"
#include "NewUIInventoryCtrl.h"
#include <GLES3/gl32.h>
#include <vector>

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

		int sheetW = 256;
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
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sheetW, sheetH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
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

	// Pilot set: the shared item-table border pieces. Small (14x14 corners,
	// thin edge strips) and drawn many times per frame whenever any
	// inventory-style window is open - reused as-is (same numeric IDs) by
	// well over a dozen other window classes via their own aliased enum
	// constants (search CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_ across the
	// source tree). Already identified as a real, measured bottleneck:
	// NewUIGuildInfoWindow.cpp's Render_Guild_History comment traces the
	// per-piece immediate-mode draws for one window's border alone to ~830
	// draw calls pinning the phone at 6 FPS before that particular window
	// switched to fewer, larger draws - this atlas is the general fix,
	// letting the existing batcher coalesce the draws instead.
	std::vector<GLuint> ids;
	ids.push_back(SEASON3B::CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_TOP_LEFT);
	ids.push_back(SEASON3B::CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_TOP_RIGHT);
	ids.push_back(SEASON3B::CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_BOTTOM_LEFT);
	ids.push_back(SEASON3B::CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_BOTTOM_RIGHT);
	ids.push_back(SEASON3B::CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_TOP_PIXEL);
	ids.push_back(SEASON3B::CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_BOTTOM_PIXEL);
	ids.push_back(SEASON3B::CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_LEFT_PIXEL);
	ids.push_back(SEASON3B::CNewUIInventoryCtrl::IMAGE_ITEM_TABLE_RIGHT_PIXEL);

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

#endif
