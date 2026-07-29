// Copyright (C) 2004 Id Software, Inc.
//

#include "tr_local.h"
#include "CelShading.h"

/*
===============================================================================

	Cel shading policy.

	See CelShading.h for the shape of the feature. This translation unit owns
	every rule that decides whether a surface is cel shaded and what its
	outline looks like, so the backend passes never have to re-derive it.

===============================================================================
*/

/*
==================
R_CelBandCount
==================
*/
int R_CelBandCount( void ) {
	return idMath::ClampInt( CEL_MIN_BANDS, CEL_MAX_BANDS, r_celShadingSteps.GetInteger() );
}

/*
==================
R_CelQuantizeUnitValue

Snaps an intensity onto the band ladder. Both endpoints are preserved exactly
so unlit surfaces stay black and fully lit ones keep their peak value; only the
ramp between them is stepped.
==================
*/
float R_CelQuantizeUnitValue( float value ) {
	if ( value <= 0.0f ) {
		return 0.0f;
	}
	if ( value >= 1.0f ) {
		return 1.0f;
	}

	const float steps = (float)R_CelBandCount() - 1.0f;
	if ( steps <= 0.0f ) {
		return value;
	}

	float band = idMath::Floor( value * steps + 0.5f );
	band = idMath::ClampFloat( 0.0f, steps, band );

	return band / steps;
}

/*
==================
R_CelShadingEnabled
==================
*/
bool R_CelShadingEnabled( void ) {
	return r_celShading.GetBool();
}

/*
==================
R_CelShadingWorldEnabled
==================
*/
bool R_CelShadingWorldEnabled( void ) {
	return r_celShadingWorld.GetBool();
}

/*
==================
R_CelShadingAnyEnabled
==================
*/
bool R_CelShadingAnyEnabled( void ) {
	return R_CelShadingEnabled() || R_CelShadingWorldEnabled();
}

/*
==================
R_CelOutlineEnabled
==================
*/
bool R_CelOutlineEnabled( void ) {
	return R_CelShadingEnabled() && r_celOutline.GetBool();
}

/*
==================
R_CelWorldOutlineEnabled
==================
*/
bool R_CelWorldOutlineEnabled( void ) {
	return R_CelShadingWorldEnabled() && R_CelWorldOutlineAlpha() > 0.0f;
}

/*
==================
R_CelSurfaceIsWorld

BSP geometry arrives either through the synthetic world space (no entityDef at
all) or through the entity that owns the static world model.
==================
*/
bool R_CelSurfaceIsWorld( const drawSurf_t *surf ) {
	if ( surf == NULL || surf->space == NULL ) {
		return false;
	}

	const idRenderEntityLocal *entityDef = surf->space->entityDef;
	if ( entityDef == NULL || entityDef->parms.hModel == NULL ) {
		return true;
	}

	return entityDef->parms.hModel->IsStaticWorldModel();
}

/*
==================
R_CelSurfaceIsViewWeapon

The first-person weapon and the arms that carry it are the surfaces squashed
into the near depth range or restricted to the local player's view.
==================
*/
bool R_CelSurfaceIsViewWeapon( const drawSurf_t *surf ) {
	if ( surf == NULL || surf->space == NULL ) {
		return false;
	}
	if ( surf->space->weaponDepthHack ) {
		return true;
	}

	const idRenderEntityLocal *entityDef = surf->space->entityDef;
	if ( entityDef == NULL ) {
		return false;
	}

	const renderEntity_t &renderEntity = entityDef->parms;
	return renderEntity.weaponDepthHackInViewID != 0 || renderEntity.allowSurfaceInViewID != 0;
}

/*
==================
R_CelSurfaceParticipates

Shared front half of the per-surface gates: is this drawable geometry that the
cel treatment is allowed to touch at all, and is its class enabled?
==================
*/
static bool R_CelSurfaceParticipates( const drawSurf_t *surf ) {
	if ( surf == NULL || surf->space == NULL || surf->geo == NULL || surf->material == NULL ) {
		return false;
	}
	if ( ( surf->dsFlags & DSF_BSE_EFFECT ) != 0 ) {
		return false;
	}

	if ( R_CelSurfaceIsWorld( surf ) ) {
		return R_CelShadingWorldEnabled();
	}
	if ( R_CelSurfaceIsViewWeapon( surf ) ) {
		return R_CelShadingEnabled() && r_celViewWeapon.GetBool();
	}

	return R_CelShadingEnabled();
}

/*
==================
R_CelShadingSurfaceActive
==================
*/
bool R_CelShadingSurfaceActive( const drawSurf_t *surf ) {
	if ( !r_celShadingBands.GetBool() || !R_CelSurfaceParticipates( surf ) ) {
		return false;
	}

	// Banding is a lighting effect; surfaces that never take a light pass, and
	// translucent ones whose ramp is carrying the blend, are left alone.
	const idMaterial *material = surf->material;
	if ( !material->IsDrawn() || material->Coverage() == MC_TRANSLUCENT ) {
		return false;
	}

	return true;
}

/*
==================
R_CelOutlineSurfaceActive

Outline shells are a model-entity effect. World geometry is inked by the
screen-space pass instead, which is why world surfaces are rejected here even
when r_celShadingWorld is on.
==================
*/
bool R_CelOutlineSurfaceActive( const drawSurf_t *surf ) {
	if ( !R_CelOutlineEnabled() || surf == NULL ) {
		return false;
	}
	if ( R_CelSurfaceIsWorld( surf ) ) {
		return false;
	}
	if ( !R_CelSurfaceParticipates( surf ) ) {
		return false;
	}
	if ( R_CelOutlineAlphaForSurface( surf ) <= 0.0f ) {
		return false;
	}

	const idMaterial *material = surf->material;
	if ( !material->IsDrawn() || material->Coverage() == MC_TRANSLUCENT ) {
		return false;
	}
	if ( material->GetSort() >= SS_POST_PROCESS || material->GetSort() == SS_SUBVIEW ) {
		return false;
	}
	if ( material->HasGui() || material->SuppressInSubview() ) {
		return false;
	}

	return surf->geo->numIndexes > 0;
}

/*
==================
R_CelOutlineWidthForSurface
==================
*/
float R_CelOutlineWidthForSurface( const drawSurf_t *surf ) {
	const idCVar &width = R_CelSurfaceIsViewWeapon( surf ) ? r_celViewWeaponOutlineWidth : r_celOutlineWidth;

	return idMath::ClampFloat( CEL_MIN_OUTLINE_WIDTH, CEL_MAX_OUTLINE_WIDTH, width.GetFloat() );
}

/*
==================
R_CelOutlineAlphaForSurface
==================
*/
float R_CelOutlineAlphaForSurface( const drawSurf_t *surf ) {
	const idCVar &alpha = R_CelSurfaceIsViewWeapon( surf ) ? r_celViewWeaponOutlineAlpha : r_celOutlineAlpha;

	return idMath::ClampFloat( 0.0f, 1.0f, alpha.GetFloat() );
}

/*
==================
R_CelParsedOutlineColor

Parses r_celOutlineColor ("r g b a", 0-255) into normalized components. The
result is cached against the raw string so the common case costs a compare
rather than a parse, without disturbing the cvar's modified flag for anyone
else watching it.
==================
*/
static void R_CelParsedOutlineColor( idVec4 &color ) {
	static idStr	cachedSource;
	static idVec4	cachedColor( 0.0f, 0.0f, 0.0f, 1.0f );
	static bool		cachedValid = false;

	const char *source = r_celOutlineColor.GetString();
	if ( source == NULL ) {
		source = "";
	}

	if ( cachedValid && cachedSource.Cmp( source ) == 0 ) {
		color = cachedColor;
		return;
	}

	// An unparsable value keeps the classic ink colour rather than blanking
	// the outline, so a typo in a config never silently disables the effect.
	idVec4 parsed( 0.0f, 0.0f, 0.0f, 255.0f );
	const int parsedCount = sscanf( source, "%f %f %f %f", &parsed.x, &parsed.y, &parsed.z, &parsed.w );
	if ( parsedCount < 3 ) {
		parsed.Set( 0.0f, 0.0f, 0.0f, 255.0f );
	} else if ( parsedCount < 4 ) {
		parsed.w = 255.0f;
	}

	for ( int i = 0; i < 4; i++ ) {
		parsed[i] = idMath::ClampFloat( 0.0f, 255.0f, parsed[i] ) * ( 1.0f / 255.0f );
	}

	cachedSource = source;
	cachedColor = parsed;
	cachedValid = true;
	color = cachedColor;
}

/*
==================
R_CelOutlineColorForSurface
==================
*/
void R_CelOutlineColorForSurface( const drawSurf_t *surf, idVec4 &color ) {
	R_CelParsedOutlineColor( color );
	color.w *= R_CelOutlineAlphaForSurface( surf );
}

/*
==================
R_CelWorldOutlineColor

The world edge shares the ink colour with the model shells but takes its opacity
from r_celShadingWorldAlpha alone, so the two outline kinds can be balanced
against each other.
==================
*/
void R_CelWorldOutlineColor( idVec4 &color ) {
	R_CelParsedOutlineColor( color );
	color.w *= R_CelWorldOutlineAlpha();
}

/*
==================
R_CelWorldOutlineWidth
==================
*/
float R_CelWorldOutlineWidth( void ) {
	return idMath::ClampFloat( CEL_MIN_WORLD_OUTLINE_WIDTH, CEL_MAX_WORLD_OUTLINE_WIDTH,
		r_celShadingWorldWidth.GetFloat() );
}

/*
==================
R_CelWorldOutlineAlpha
==================
*/
float R_CelWorldOutlineAlpha( void ) {
	return idMath::ClampFloat( 0.0f, 1.0f, r_celShadingWorldAlpha.GetFloat() );
}

/*
==================
R_CelWorldOutlineDepthThreshold

Expressed as a fraction of view distance, so a crease reads the same whether it
is at the player's feet or across the room.
==================
*/
float R_CelWorldOutlineDepthThreshold( void ) {
	return idMath::ClampFloat( 0.0001f, 0.02f, r_celShadingWorldDepthThreshold.GetFloat() );
}

/*
==================
R_CelWorldOutlineNormalThreshold
==================
*/
float R_CelWorldOutlineNormalThreshold( void ) {
	return idMath::ClampFloat( 0.0f, 1.0f, r_celShadingWorldNormalThreshold.GetFloat() );
}
