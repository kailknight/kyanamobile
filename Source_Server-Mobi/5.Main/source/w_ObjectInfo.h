// w_ObjectInfo.h: interface for the ObjectInfo class.
//////////////////////////////////////////////////////////////////////
#pragma once

#include <vector>
#include "./Math/ZzzMathLib.h"

class	CInterpolateContainer
{
public:
	struct INTERPOLATE_FACTOR
	{
		float	fRateStart;
		float	fRateEnd;
		
		vec3_t	v3Start;
		vec3_t	v3End;
		INTERPOLATE_FACTOR( float fRateStart_, float fRateEnd_,
			vec3_t& v3Start_, vec3_t& v3End_ )
		{
			fRateStart = fRateStart_;
			fRateEnd = fRateEnd_;
			
			VectorCopy(v3Start_, v3Start);
			VectorCopy(v3End_, v3End);
		};
		
		INTERPOLATE_FACTOR()
		{
			
		};
	};
	
	struct INTERPOLATE_FACTOR_F
	{
		float	fRateStart;
		float	fRateEnd;
		
		float	fStart;
		float	fEnd;
		INTERPOLATE_FACTOR_F( float fRateStart_, float fRateEnd_,
			float& fStart_, float& fEnd_ )
		{
			fRateStart = fRateStart_;
			fRateEnd = fRateEnd_;
			
			fStart = fStart_;
			fEnd = fEnd_;
		};
		
		INTERPOLATE_FACTOR_F()
		{
			
		};
	};
	

public:
	typedef std::vector<INTERPOLATE_FACTOR>		VEC_INTERPOLATES;
	typedef std::vector<INTERPOLATE_FACTOR_F>	VEC_INTERPOLATES_F;
	VEC_INTERPOLATES						m_vecInterpolatesAngle;
	VEC_INTERPOLATES						m_vecInterpolatesPos;
	VEC_INTERPOLATES_F						m_vecInterpolatesScale;
	VEC_INTERPOLATES_F						m_vecInterpolatesAlpha;	
		
	public:
		void GetCurrentValue(vec3_t& v3Out, float fCurrentRate, VEC_INTERPOLATES& vecInterpolates );
		void GetAngleCurrent(vec3_t& v3Out, float fCurrentRate )
		{
			GetCurrentValue( v3Out, fCurrentRate, m_vecInterpolatesAngle );
		}
		void GetPosCurrent(vec3_t& v3Out, float fCurrentRate )
		{
			GetCurrentValue( v3Out, fCurrentRate, m_vecInterpolatesPos );
		}

		void GetCurrentValueF(float& fOut, float fCurrentRate, VEC_INTERPOLATES_F& vecInterpolates );
		void GetScaleCurrent(float& fOut, float fCurrentRate )
		{
			GetCurrentValueF( fOut, fCurrentRate, m_vecInterpolatesScale );
		}

		void GetAlphaCurrent(float& fOut, float fCurrentRate )
		{
			GetCurrentValueF( fOut, fCurrentRate, m_vecInterpolatesAlpha );
		}
		
	public:
		void ClearContainer();
		
		CInterpolateContainer(){};
		~CInterpolateContainer(){};
};

typedef struct tagMU_POINTF
{
	float x;
	float y;
} MU_POINTF;

typedef struct tagSIZEF
{
	float cx;
	float cy;
} SIZEF;
	
typedef struct
{
	vec3_t StartPos;
	vec3_t XAxis;
	vec3_t YAxis;
	vec3_t ZAxis;
} OBB_t;

// Snapshot of everything Calc_RenderObject feeds into BMD::Transform for one
// call - see IsTransformCacheEligible/BuildTransformCache/RestoreTransformCache
// in ZzzObject.cpp. If every field here still matches, the object's pose (and
// therefore Transform's vertex/normal/intensity output) is guaranteed
// identical to last time, so the cached output can be memcpy'd back in
// instead of recomputed. Select/Translate/LightEnable/ContrastEnable are
// included because Calc_RenderObject threads them into the same Transform()
// call (Select in particular changes the global BoneScale Transform() reads).
typedef struct
{
	int            Type;
	vec3_t         Position;
	vec3_t         Angle;
	vec3_t         HeadAngle;
	float          Scale;
	unsigned short CurrentAction;
	float          AnimationFrame;
	bool           Translate;
	int            Select;
	bool           LightEnable;
	bool           ContrastEnable;
} OBJECT_TRANSFORM_CACHE_KEY;






class OBJECT
{
public:
	OBJECT();
	virtual ~OBJECT();

public:
	void Initialize();
	void Destroy();

public:
	bool          Live;
	bool          bBillBoard;
	bool          m_bCollisionCheck;
	bool          m_bRenderShadow;
	bool          EnableShadow;
	bool		  LightEnable;
	bool		  m_bActionStart;
	bool		  m_bRenderAfterCharacter;
	bool	      Visible;
	bool	      AlphaEnable;
	bool          EnableBoneMatrix;
	bool		  ContrastEnable;
	bool          ChromeEnable;

public:
	unsigned char AI;
#ifdef PBG_ADD_NEWCHAR_MONK_ANI	
	unsigned short CurrentAction;
	unsigned short PriorAction;
#else //PBG_ADD_NEWCHAR_MONK_ANI
	unsigned char CurrentAction;
	unsigned char PriorAction;
#endif //PBG_ADD_NEWCHAR_MONK_ANI

public:
	BYTE          ExtState;
	BYTE          Teleport;
	BYTE          Kind;
	WORD		  Skill;
	BYTE		  m_byNumCloth;
	BYTE		  m_byHurtByOneToOne;
	BYTE          WeaponLevel;
	BYTE          DamageTime;
	BYTE          m_byBuildTime;
	BYTE		  m_bySkillCount;
	BYTE		  m_bySkillSerialNum;
	BYTE		  Block;
	void*		  m_pCloth;

public:
	short         ScreenX;
	short         ScreenY;
	short         Weapon;

public:
	int			  Type;
	int           SubType;
	int			  m_iAnimation;
	int           HiddenMesh;
	float           LifeTime;
	int           BlendMesh;
	int           AttackPoint[2];
	int           RenderType;
	int			  InitialSceneFrame;
	int           LinkBone;

public:
	DWORD		  m_dwTime;
	
public:
	float         Scale;
	float         BlendMeshLight;
	float         BlendMeshTexCoordU;
	float         BlendMeshTexCoordV;
	float         Timer;
	float         m_fEdgeScale;
	float         Velocity;
	float		  CollisionRange;
	float         ShadowScale;
	float         Gravity;
	float         Distance;
	float         AnimationFrame;
	float         PriorAnimationFrame;
	float	      AlphaTarget;
	float         Alpha;

	float       LastHorseWaveEffect;
	bool        HorseQuakeSpawned;	// dust ring already fired for this stomp
	bool        HorseFurySpawned;	// fury strike already fired for this stomp
	float         PKKey;
	
public:
	vec3_t        Light;
	vec3_t        Direction;
	vec3_t		  m_vPosSword;
	vec3_t		  StartPosition;
	vec3_t        BoundingBoxMin;
	vec3_t        BoundingBoxMax;	
	vec3_t		  m_vDownAngle;
	vec3_t		  m_vDeadPosition;
	vec3_t        Position;
	vec3_t	 	  Angle;
	vec3_t	 	  HeadAngle;
	vec3_t	   	  HeadTargetAngle;
	vec3_t  	  EyeLeft;
	vec3_t  	  EyeRight;
	vec3_t		  EyeLeft2;
	vec3_t		  EyeRight2;
	vec3_t		  EyeLeft3;
	vec3_t		  EyeRight3;
public:
	vec34_t	 	  Matrix;
	vec34_t       *BoneTransform;

public:
	OBB_t		  OBB;
	
public:
	OBJECT        *Owner;
	OBJECT		  *Prior;
	OBJECT		  *Next;

public:
	Buff		  m_BuffMap;

public:
	short int	  m_sTargetIndex;

public:
	BOOL		m_bpcroom;
	bool		m_bAdaptivePoseReady;
	int			m_iAdaptivePoseAction;

	// BMD::Transform() output cache - see IsTransformCacheEligible in
	// ZzzObject.cpp. v1 scope: map-decoration objects only (see
	// m_bTransformCacheProvenanceOk below). One flat allocation per array,
	// sized to the sum of this object's model's Meshs[i].NumVertices/
	// NumNormals (not MAX_VERTICES, which the shared VertexTransform/
	// NormalTransform/IntensityTransform buffers waste per mesh slot).
	vec3_t*		m_pTransformCacheVertices;
	vec3_t*		m_pTransformCacheNormals;
	float*		m_pTransformCacheIntensity;
	int			m_iTransformCacheVertexCapacity;
	int			m_iTransformCacheNormalCapacity;
	OBB_t		m_TransformCacheOBB;
	OBJECT_TRANSFORM_CACHE_KEY m_TransformCacheKey;
	bool		m_bTransformCacheReady;
	// Set true ONLY inside CreateObject() (the map-decoration factory).
	// Reset false on every Initialize() call, including the in-place
	// re-Initialize() that recycles Boids[]/pet slots for an unrelated new
	// spawn - this is what excludes those populations structurally, rather
	// than by an enumerable list, since a recycled slot's stale cache would
	// otherwise show the PREVIOUS occupant's geometry.
	bool		m_bTransformCacheProvenanceOk;
	vec3_t		m_v3PrePos1;
	vec3_t		m_v3PrePos2;
#if(CB_ATTACK_HIDEN_PET)
	DWORD		CacheTimeAttack;
#endif
	CInterpolateContainer	m_Interpolates;
};
