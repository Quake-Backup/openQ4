// Copyright (C) 2004 Id Software, Inc.
//

#ifndef __SHADOWMAP_CLASSIFICATION_H__
#define __SHADOWMAP_CLASSIFICATION_H__

typedef struct viewLight_s viewLight_t;

static const int SHADOWMAP_CLASSIFICATION_MAX_CASCADES = 4;

typedef enum {
	SHADOWMAP_LIGHT_PROJECTED = 0,
	SHADOWMAP_LIGHT_POINT,
	SHADOWMAP_LIGHT_PARALLEL,
	SHADOWMAP_LIGHT_GLOBAL
} shadowMapLightClass_t;

typedef struct shadowMapLightClassification_s {
	shadowMapLightClass_t	lightClass;
	bool					projectedLight;
	bool					pointLight;
	bool					ordinaryProjectedLight;
	bool					parallelLight;
	bool					globalLight;
	bool					projectedCSMGateApplies;
	bool					projectedCSMEnabled;
	bool					csmEnabled;
	int						cascadeCount;
	int						atlasDiv;
	int						tileCount;
} shadowMapLightClassification_t;

shadowMapLightClassification_t R_ClassifyShadowMapLight( const viewLight_t *vLight );
const char *R_ShadowMapLightClassName( shadowMapLightClass_t lightClass );

#endif /* !__SHADOWMAP_CLASSIFICATION_H__ */
