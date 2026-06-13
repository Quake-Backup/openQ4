// Copyright (C) 2004 Id Software, Inc.
//

#include "tr_local.h"
#include "ShadowMapClassification.h"

static shadowMapLightClass_t R_ShadowMapLightClassForViewLight( const viewLight_t *vLight ) {
	if ( vLight == NULL ) {
		return SHADOWMAP_LIGHT_PROJECTED;
	}
	if ( vLight->lightDef != NULL && vLight->lightDef->parms.globalLight ) {
		return SHADOWMAP_LIGHT_GLOBAL;
	}
	if ( vLight->parallel ) {
		return SHADOWMAP_LIGHT_PARALLEL;
	}
	if ( vLight->pointLight ) {
		return SHADOWMAP_LIGHT_POINT;
	}
	return SHADOWMAP_LIGHT_PROJECTED;
}

shadowMapLightClassification_t R_ClassifyShadowMapLight( const viewLight_t *vLight ) {
	shadowMapLightClassification_t classification;
	memset( &classification, 0, sizeof( classification ) );

	classification.lightClass = R_ShadowMapLightClassForViewLight( vLight );
	classification.pointLight = vLight != NULL && vLight->pointLight;
	classification.projectedLight = !classification.pointLight;
	classification.ordinaryProjectedLight = classification.lightClass == SHADOWMAP_LIGHT_PROJECTED;
	classification.parallelLight = classification.lightClass == SHADOWMAP_LIGHT_PARALLEL;
	classification.globalLight = classification.lightClass == SHADOWMAP_LIGHT_GLOBAL;
	classification.projectedCSMGateApplies = classification.ordinaryProjectedLight;
	classification.projectedCSMEnabled = classification.projectedCSMGateApplies && r_shadowMapProjectedCSM.GetBool();
	classification.cascadeCount = 1;
	classification.atlasDiv = classification.pointLight ? 3 : 1;
	classification.tileCount = classification.pointLight ? 6 : 1;

	if ( vLight == NULL || classification.pointLight ) {
		return classification;
	}

	const int requestedCascadeCount = idMath::ClampInt( 1, SHADOWMAP_CLASSIFICATION_MAX_CASCADES, r_shadowMapCascadeCount.GetInteger() );
	classification.csmEnabled = r_shadowMapCSM.GetBool()
		&& requestedCascadeCount > 1
		&& ( !classification.projectedCSMGateApplies || classification.projectedCSMEnabled );

	if ( classification.csmEnabled ) {
		classification.cascadeCount = requestedCascadeCount;
		classification.atlasDiv = 2;
		classification.tileCount = requestedCascadeCount;
	}

	return classification;
}

const char *R_ShadowMapLightClassName( shadowMapLightClass_t lightClass ) {
	switch ( lightClass ) {
	case SHADOWMAP_LIGHT_POINT:
		return "point";
	case SHADOWMAP_LIGHT_PARALLEL:
		return "parallel";
	case SHADOWMAP_LIGHT_GLOBAL:
		return "global";
	default:
		return "projected";
	}
}
