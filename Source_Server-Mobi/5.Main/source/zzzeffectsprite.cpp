///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ZzzOpenglUtil.h"
#include "ZzzBMD.h"
#include "ZzzInfomation.h"
#include "ZzzObject.h"
#include "ZzzCharacter.h"
#include "ZzzLodTerrain.h"
#include "ZzzTexture.h"
#include "ZzzAi.h"
#include "ZzzEffect.h"
#include "DSPlaySound.h"
#include "WSClient.h"
#include "NewUISystem.h"
#if defined(__ANDROID__) || defined(MU_IOS)
#include "Platform/gl_compat.h"
#endif
#include <algorithm>
#include <cstdint>
#include <vector>

namespace
{
constexpr float kSpriteFrameMin = 0.2f;
constexpr float kSpriteFrameMax = 1.0f;
constexpr float kSpriteFrameStep = 0.1f;

float ComputeFrameDelta()
{
    return kSpriteFrameStep * FPS_ANIMATION_FACTOR;
}

float UpdateAnimationFrame(float currentFrame, bool isVisible)
{
    const float delta = ComputeFrameDelta();
    currentFrame += isVisible ? delta : -delta;
    return std::clamp(currentFrame, kSpriteFrameMin, kSpriteFrameMax);
}

#if defined(__ANDROID__) || defined(MU_IOS)
enum class SpriteBatchBlendMode : uint8_t
{
    AlphaBlend,
    AlphaBlendMinus,
    AlphaTest,
    AlphaBlend2,
};

struct SpriteQuadBatch
{
    std::vector<float> vertices;
    int texture = -1;
    SpriteBatchBlendMode blendMode = SpriteBatchBlendMode::AlphaBlend;
    int quadCount = 0;
};

SpriteBatchBlendMode DetermineSpriteBlendMode(const OBJECT* o)
{
    if (o->Type == BITMAP_FORMATION_MARK || o->SubType == 2)
    {
        return SpriteBatchBlendMode::AlphaTest;
    }

    if (o->SubType == 1)
    {
        return SpriteBatchBlendMode::AlphaBlendMinus;
    }

    if (o->SubType == 3)
    {
        return SpriteBatchBlendMode::AlphaBlend2;
    }

    return SpriteBatchBlendMode::AlphaBlend;
}

void ApplySpriteBlendMode(SpriteBatchBlendMode mode)
{
    switch (mode)
    {
    case SpriteBatchBlendMode::AlphaBlend:
        EnableAlphaBlend();
        break;
    case SpriteBatchBlendMode::AlphaBlendMinus:
        EnableAlphaBlendMinus();
        break;
    case SpriteBatchBlendMode::AlphaTest:
        EnableAlphaTest();
        break;
    case SpriteBatchBlendMode::AlphaBlend2:
        EnableAlphaBlend2();
        break;
    }
}

void FlushSpriteQuadBatch(SpriteQuadBatch& batch)
{
    if (batch.quadCount <= 0 || batch.vertices.empty() || batch.texture < 0)
    {
        batch.vertices.clear();
        batch.texture = -1;
        batch.quadCount = 0;
        return;
    }

    ApplySpriteBlendMode(batch.blendMode);
    BindTexture(batch.texture);
    GL_DrawQuadsBulk(batch.vertices.data(), batch.quadCount);

    batch.vertices.clear();
    batch.texture = -1;
    batch.quadCount = 0;
}

// Per-texture buckets for the order-independent sprite kinds: additive
// (glBlendFunc(GL_ONE, GL_ONE)) and darkening (GL_ZERO, GL_ONE_MINUS_SRC_COLOR,
// a product). Same reasoning as the particle buckets in ZzzEffectParticle.cpp:
// walking Sprites[] in slot order cut the batch on every texture change, and
// each cut is a full draw on Mali. Order-dependent kinds still draw in slot
// order; the buckets follow them, darkening then additive.
struct SpriteBucket
{
    SpriteBatchBlendMode mode;
    SpriteQuadBatch batch;
};
std::vector<SpriteBucket> s_spriteBuckets;

SpriteQuadBatch& GetSpriteBucket(SpriteBatchBlendMode mode, int texture)
{
    for (SpriteBucket& b : s_spriteBuckets)
    {
        if (b.mode == mode && b.batch.texture == texture)
        {
            return b.batch;
        }
    }
    // Flushed buckets have texture -1 (FlushSpriteQuadBatch) - reuse one.
    for (SpriteBucket& b : s_spriteBuckets)
    {
        if (b.batch.quadCount == 0)
        {
            b.mode = mode;
            b.batch.texture = texture;
            return b.batch;
        }
    }
    // Bounded like the particle buckets: past the cap, share the first one and
    // pay an extra flush rather than grow.
    if (s_spriteBuckets.size() >= 64)
    {
        SpriteBucket& first = s_spriteBuckets[0];
        if (first.batch.quadCount > 0 && (first.mode != mode || first.batch.texture != texture))
        {
            FlushSpriteQuadBatch(first.batch);
        }
        first.mode = mode;
        first.batch.texture = texture;
        return first.batch;
    }
    SpriteBucket nb;
    nb.mode = mode;
    nb.batch.texture = texture;
    s_spriteBuckets.push_back(std::move(nb));
    return s_spriteBuckets.back().batch;
}

void AppendSpriteBatchVertex(std::vector<float>& vertices, const vec3_t position, const float uv[2], const float color[4])
{
    vertices.push_back(position[0]);
    vertices.push_back(position[1]);
    vertices.push_back(position[2]);
    vertices.push_back(color[0]);
    vertices.push_back(color[1]);
    vertices.push_back(color[2]);
    vertices.push_back(color[3]);
    vertices.push_back(uv[0]);
    vertices.push_back(uv[1]);
}

void QueueSpriteQuadBatch(SpriteQuadBatch& batch, OBJECT* o)
{
    o->AnimationFrame = UpdateAnimationFrame(o->AnimationFrame, o->Visible);
    float scale = o->AnimationFrame * o->Scale;

    BITMAP_t* bitmap = Bitmaps.GetTexture(o->Type);
    float width = bitmap->Width * scale;
    float height = bitmap->Height * scale;

    float u = 0.f;
    float v = 0.f;
    float uWidth = 1.f;
    float vHeight = 1.f;
    if (o->Type == BITMAP_FORMATION_MARK)
    {
        width = 64.f;
        height = 64.f;
        uWidth = 0.33f;
        vHeight = 0.33f;
        switch (o->SubType)
        {
        case 1:
            u = 0.33f;
            break;
        case 2:
            u = 0.66f;
            break;
        case 3:
            v = 0.33f;
            break;
        case 4:
            u = 0.33f;
            v = 0.33f;
            break;
        case 5:
            u = 0.66f;
            v = 0.33f;
            break;
        case 6:
            v = 0.66f;
            break;
        case 7:
            u = 0.33f;
            v = 0.66f;
            break;
        }
    }

    vec3_t transformedPosition;
    VectorTransform(o->Position, CameraMatrix, transformedPosition);
    float x = transformedPosition[0];
    float y = transformedPosition[1];
    float z = transformedPosition[2];
    width *= 0.5f;
    height *= 0.5f;

    vec3_t positions[4];
    if (o->Angle[2] == 0.f)
    {
        Vector(x - width, y - height, z, positions[0]);
        Vector(x + width, y - height, z, positions[1]);
        Vector(x + width, y + height, z, positions[2]);
        Vector(x - width, y + height, z, positions[3]);
    }
    else
    {
        vec3_t localPositions[4];
        Vector(-width, -height, z, localPositions[0]);
        Vector(width, -height, z, localPositions[1]);
        Vector(width, height, z, localPositions[2]);
        Vector(-width, height, z, localPositions[3]);

        vec3_t rotationAngle;
        Vector(0.f, 0.f, o->Angle[2], rotationAngle);
        float rotationMatrix[3][4];
        AngleMatrix(rotationAngle, rotationMatrix);
        for (int i = 0; i < 4; ++i)
        {
            VectorRotate(localPositions[i], rotationMatrix, positions[i]);
            positions[i][0] += x;
            positions[i][1] += y;
        }
    }

    float texCoords[4][2];
    TEXCOORD(texCoords[3], u, v);
    TEXCOORD(texCoords[2], u + uWidth, v);
    TEXCOORD(texCoords[1], u + uWidth, v + vHeight);
    TEXCOORD(texCoords[0], u, v + vHeight);

    float color[4] = { o->Light[0], o->Light[1], o->Light[2], 1.f };

    if (o->Type == BITMAP_BLOOD + 1 || o->Type == BITMAP_FONT_HIT)
    {
        color[3] = 1.f;
    }
    else if (o->SubType == 0)
    {
        color[3] = o->Light[0];
    }

    const SpriteBatchBlendMode blendMode = DetermineSpriteBlendMode(o);
    SpriteQuadBatch* target = &batch;
    if (blendMode == SpriteBatchBlendMode::AlphaBlend || blendMode == SpriteBatchBlendMode::AlphaBlendMinus)
    {
        target = &GetSpriteBucket(blendMode, o->Type);
    }
    else if (batch.quadCount > 0 && (batch.texture != o->Type || batch.blendMode != blendMode))
    {
        FlushSpriteQuadBatch(batch);
    }

    if (target->vertices.empty())
    {
        target->vertices.reserve(4096);
    }

    target->texture = o->Type;
    target->blendMode = blendMode;
    for (int i = 0; i < 4; ++i)
    {
        AppendSpriteBatchVertex(target->vertices, positions[i], texCoords[i], color);
    }
    ++target->quadCount;
}

// Draws what the buckets hold: darkening first, then additive - see
// s_spriteBuckets.
void FlushSpriteBuckets()
{
    for (int pass = 0; pass < 2; ++pass)
    {
        const SpriteBatchBlendMode mode = (pass == 0) ? SpriteBatchBlendMode::AlphaBlendMinus : SpriteBatchBlendMode::AlphaBlend;
        for (SpriteBucket& b : s_spriteBuckets)
        {
            if (b.mode != mode || b.batch.quadCount <= 0)
            {
                continue;
            }
            FlushSpriteQuadBatch(b.batch);
        }
    }
}
#endif
}

OBJECT	Sprites   [MAX_SPRITES];
// Lowest slot that may be free. Sprites are created in order during the frame
// and freed together in RenderSprites, so scanning from 0 walked every sprite
// already alive this frame - quadratic in the sprite count, ~3% of a crowded
// frame on a Helio G85. Every place that frees a slot lowers this hint, so the
// slot handed out is still the lowest free one, exactly as before.
int g_SpriteFreeHint = 0;
inline SpinLock* g_CreateSprite_lock = new SpinLock();
int CreateSprite(int Type,vec3_t Position,float Scale,vec3_t Light,OBJECT *Owner,float Rotation,int SubType)
{
	if (!g_pNewUISystem->GetUI_NewOptionWindow()->OnOffGrap[g_pNewUISystem->GetUI_NewOptionWindow()->eEffectStatic]) return false;

#if defined(__ANDROID__) || defined(MU_IOS)
	if (ShouldThrottleAdaptiveEffectSpawn(ADAPTIVE_EFFECT_SPRITE, Type, Position, SubType, Scale, Owner))
	{
		return false;
	}
#endif

	// Nothing free from the hint up to the recycled tail: rescan from 0 in case
	// a slot below the hint was freed somewhere that does not report it.
	int start = g_SpriteFreeHint;
	if (start < 0 || start > MAX_SPRITES - 2)
	{
		start = 0;
	}
	else
	{
		bool anyFree = false;
		for (int i = start; i < MAX_SPRITES - 2; i++)
		{
			if (!Sprites[i].Live) { anyFree = true; start = i; break; }
		}
		if (!anyFree) start = 0;
	}

	//g_CreateSprite_lock->lock();
	for(int i=start;i<MAX_SPRITES;i++)
	{
		OBJECT *o = &Sprites[i];
		//== Fix Effect
		if (i >= (MAX_SPRITES - 2) && o->Live)
		{
			o->Live = false;
			o->Visible = false;
		}
		if(!o->Live)
		{
			g_SpriteFreeHint = i + 1;
			o->Live           = true;
			o->Type           = Type;
			o->SubType        = SubType;
			o->Owner          = Owner;
			o->AnimationFrame = 1.f;
    		o->Scale          = Scale;
			o->Angle[2]       = Rotation;
			VectorCopy(Position,o->Position);
			VectorCopy(Position,o->StartPosition);
			VectorCopy(Light,o->Light);
			//g_CreateSprite_lock->unlock();
			return i;
		}
	}
	//g_CreateSprite_lock->unlock();
	return false;
}

void RenderSprite(OBJECT *o,OBJECT *Owner)
{
	if (GetRenderEffect() == false) return;

	if (o->Visible)
	{
		o->AnimationFrame += 0.1f * static_cast<float>(FPS_ANIMATION_FACTOR);
		if (o->AnimationFrame > 1.f)
		{
			o->AnimationFrame = 1.f;
		}
	}
	else
	{
		o->AnimationFrame -= 0.1f * static_cast<float>(FPS_ANIMATION_FACTOR);
		if (o->AnimationFrame < 0.2f)
		{
			o->AnimationFrame = 0.2f;
		}
	}
	float Scale = o->AnimationFrame*o->Scale;
	
	BITMAP_t* pBitmap = Bitmaps.GetTexture(o->Type);
	float Width  = pBitmap->Width * Scale;
	float Height = pBitmap->Height * Scale;

    if ( o->Type==BITMAP_FORMATION_MARK )
    {
		float u = 0.0f, v = 0.0f, uw, vw;
		uw=0.33f; vw=0.33f;
        switch ( o->SubType )
        {
        case 0:
            u=0.f; v=0.f;
            break;

        case 1:
            u=0.33f; v=0.f;
            break;

        case 2:
            u=0.66f; v=0.f;
            break;

        case 3:
            u=0.f; v=0.33f;
            break;

        case 4:
            u=0.33f; v=0.33f;
            break;

        case 5:
            u=0.66f; v=0.33f;
            break;

        case 6:
            u=0.f; v=0.66f;
            break;

        case 7:
            u=0.33f; v=0.66f;
            break;
        }

        RenderSprite( o->Type, o->Position, 64, 64, o->Light, o->Angle[2], u, v, uw, vw );
    }
    else
    {
        RenderSprite(o->Type,o->Position,Width,Height,o->Light,o->Angle[2]);
    }
}

void RenderSprites ( BYTE byRenderOneMore )
{
#if defined(__ANDROID__) || defined(MU_IOS)
    SpriteQuadBatch spriteBatch;
    spriteBatch.vertices.reserve(4096);
    const bool restoreDepthTest = DepthTestEnable;
#endif
	for(int i=0;i<MAX_SPRITES;i++)
	{
		OBJECT *o = &Sprites[i];
        if( byRenderOneMore == 1 )
        {
            if ( o->Position[2] > 150.f ) 
			{
				continue;
			}
        }
        else if( byRenderOneMore == 2 )
        {
            if( o->Position[2] <= 100.f )
            {
                o->Live = false;
                if (i < g_SpriteFreeHint) g_SpriteFreeHint = i;
                continue;
            }
        }

		if(o->Live)
		{
#if defined(__ANDROID__) || defined(MU_IOS)
            QueueSpriteQuadBatch(spriteBatch, o);
#else
            if( o->Type == BITMAP_FORMATION_MARK )
            {
                EnableAlphaTest ();
            }
            else if(o->SubType == 0)
			{
          	    EnableAlphaBlend();
			}
			else if( o->SubType==1 )
			{
               	EnableAlphaBlendMinus();
			}
            else if( o->SubType==2 )
			{
                EnableAlphaTest();
			}
            else if( o->SubType==3 )
			{
                EnableAlphaBlend2();
			}
    		RenderSprite(o,o->Owner);
#endif

            if( byRenderOneMore == 0 || byRenderOneMore == 2 )
            {
                o->Live = false;
                if (i < g_SpriteFreeHint) g_SpriteFreeHint = i;
            }
		}
	}
#if defined(__ANDROID__) || defined(MU_IOS)
    FlushSpriteQuadBatch(spriteBatch);
    FlushSpriteBuckets();
    DisableAlphaBlend();
    if (restoreDepthTest)
    {
        EnableDepthTest();
    }
    else
    {
        DisableDepthTest();
    }
#endif
}

void CheckSprites()
{
	for(int i=0; i<MAX_SPRITES; i++)
	{
		OBJECT *o = &Sprites[i];
		if(o->Live)
		{
         	o->Visible = true;
		}
	}
}
