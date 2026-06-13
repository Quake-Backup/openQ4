// Copyright (C) 2004 Id Software, Inc.
//

#ifndef __SHADOWMAP_ARB2_PARITY_H__
#define __SHADOWMAP_ARB2_PARITY_H__

#include "ShadowMapProjected.h"

typedef struct shadowMapArb2ParityState_s {
	bool							valid;
	bool							pointLight;
	bool							projectedLight;
	bool							csmEnabled;
	int								requestedCascadeCount;
	int								cascadeCount;
	int								tileCount;
	int								atlasDiv;
	int								tileSize;
	bool							projectedStateReady;
	bool							projectedCascadeFallback;
	int								projectedFallbackCascade;
	shadowMapProjectedLightState_t	projectedState;
} shadowMapArb2ParityState_t;

enum shadowMapArb2CachePassMask_t {
	SHADOWMAP_ARB2_CACHE_PASS_LOCAL = 1u << 0,
	SHADOWMAP_ARB2_CACHE_PASS_GLOBAL = 1u << 1
};

typedef struct shadowMapArb2CacheEstimate_s {
	bool							valid;
	bool							pointLight;
	bool							staticCacheEnabled;
	int								lightIndex;
	int								shadowPasses;
	int								cacheablePasses;
	int								cacheHitPasses;
	int								cacheMissPasses;
	int								freshUpdatePasses;
	int								budgetFallbackPasses;
	int								stencilOnlyPasses;
	int								receiverFallbackPasses;
	int								unshadowedPasses;
	unsigned int					cacheablePassMask;
	unsigned int					cacheHitPassMask;
	unsigned int					cacheMissPassMask;
	unsigned int					freshUpdatePassMask;
	unsigned int					budgetFallbackPassMask;
	int								projectedCacheSlotsUsed;
	int								projectedCacheSlotsTotal;
	int								pointCacheSlotsUsed;
	int								pointCacheSlotsTotal;
} shadowMapArb2CacheEstimate_t;

bool RB_ShadowMapBuildArb2ParityState( const viewLight_t *vLight, const viewDef_t *viewDef, int shadowMapSize, shadowMapArb2ParityState_t &state );
bool RB_ShadowMapEstimateArb2CacheOwnership( const viewLight_t *vLight, const viewDef_t *viewDef, shadowMapArb2CacheEstimate_t &estimate );
bool RB_ShadowMapArb2ReceiverFallbackSelfTest( void );

#endif /* !__SHADOWMAP_ARB2_PARITY_H__ */
