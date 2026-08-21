// Copyright (C) 2026 DarkMatter Productions
//

#ifndef __CLASSIC_INTERACTION_DOMAIN_H__
#define __CLASSIC_INTERACTION_DOMAIN_H__

#include "MaterialResourceTable.h"

/*
===============================================================================

	Shared whole-view ownership contract for classic fixed interaction lighting.

	Preparation is transactional per ordinary root 3D view.  A ready view owns
	the complete unshadowed fixed-classic light -> local/global/translucent
	receiver -> primitive stream in source order.  Every value needed by a
	backend is sealed here; the retained drawSurf pointer is a geometry bridge,
	not permission to reread material stages or shader registers.

	Shadows, custom lighting, deforms, skinning, depth hacks, mutable resources,
	and renderer-specific interaction variants are explicit whole-view rollback
	boundaries.  A failed view publishes no light, surface, primitive, or texture
	range, so the unchanged classic renderer can execute it exactly once.

===============================================================================
*/

const int CLASSIC_INTERACTION_DOMAIN_MAX_VIEWS = SCENE_PACKET_MAX_SCENES;
const int CLASSIC_INTERACTION_DOMAIN_MAX_LIGHTS = SCENE_PACKET_MAX_DRAWS;
const int CLASSIC_INTERACTION_DOMAIN_MAX_SURFACES = SCENE_PACKET_MAX_DRAWS;
const int CLASSIC_INTERACTION_DOMAIN_MAX_PRIMITIVES = SCENE_PACKET_MAX_DRAWS * 4;
const int CLASSIC_INTERACTION_DOMAIN_MAX_TEXTURES = SCENE_PACKET_MAX_DRAWS * 2;

enum classicInteractionDomainReceiver_t {
	CLASSIC_INTERACTION_RECEIVER_LOCAL = 0,
	CLASSIC_INTERACTION_RECEIVER_GLOBAL,
	CLASSIC_INTERACTION_RECEIVER_TRANSLUCENT,
	CLASSIC_INTERACTION_RECEIVER_COUNT
};

enum classicInteractionDomainPrimitiveDisposition_t {
	CLASSIC_INTERACTION_PRIMITIVE_DRAW = 0,
	CLASSIC_INTERACTION_PRIMITIVE_NOOP_INACTIVE_LIGHT,
	CLASSIC_INTERACTION_PRIMITIVE_NOOP_INACTIVE_SURFACE,
	CLASSIC_INTERACTION_PRIMITIVE_NOOP_MISSING_BUMP,
	CLASSIC_INTERACTION_PRIMITIVE_NOOP_BLACK,
	CLASSIC_INTERACTION_PRIMITIVE_COUNT
};

enum classicInteractionDomainDepth_t {
	CLASSIC_INTERACTION_DEPTH_EQUAL = 0,
	CLASSIC_INTERACTION_DEPTH_LESS_OR_EQUAL,
	CLASSIC_INTERACTION_DEPTH_COUNT
};

enum classicInteractionDomainFailure_t {
	CLASSIC_INTERACTION_FAILURE_NONE = 0,
	CLASSIC_INTERACTION_FAILURE_UNAVAILABLE,
	CLASSIC_INTERACTION_FAILURE_SCENE_PACKET_OVERFLOW,
	CLASSIC_INTERACTION_FAILURE_MATERIAL_TABLE_NOT_PREPARED,
	CLASSIC_INTERACTION_FAILURE_MATERIAL_TABLE_OVERFLOW,
	CLASSIC_INTERACTION_FAILURE_VIEW_POOL_OVERFLOW,
	CLASSIC_INTERACTION_FAILURE_LIGHT_POOL_OVERFLOW,
	CLASSIC_INTERACTION_FAILURE_SURFACE_POOL_OVERFLOW,
	CLASSIC_INTERACTION_FAILURE_PRIMITIVE_POOL_OVERFLOW,
	CLASSIC_INTERACTION_FAILURE_TEXTURE_POOL_OVERFLOW,
	CLASSIC_INTERACTION_FAILURE_UNSUPPORTED_VIEW,
	CLASSIC_INTERACTION_FAILURE_INVALID_SCENE_RANGE,
	CLASSIC_INTERACTION_FAILURE_INVALID_INTERACTION_PASS,
	CLASSIC_INTERACTION_FAILURE_INVALID_DRAW_RANGE,
	CLASSIC_INTERACTION_FAILURE_SOURCE_PACKET_MISMATCH,
	CLASSIC_INTERACTION_FAILURE_INVALID_DRAW_PACKET,
	CLASSIC_INTERACTION_FAILURE_MISSING_GEOMETRY_RECORD,
	CLASSIC_INTERACTION_FAILURE_MISSING_INSTANCE_RECORD,
	CLASSIC_INTERACTION_FAILURE_MISSING_MATERIAL_RECORD,
	CLASSIC_INTERACTION_FAILURE_STALE_MATERIAL_RECORD,
	CLASSIC_INTERACTION_FAILURE_MISSING_SHADER_REGISTERS,
	CLASSIC_INTERACTION_FAILURE_REGISTER_OUT_OF_RANGE,
	CLASSIC_INTERACTION_FAILURE_NONFINITE_VALUE,
	CLASSIC_INTERACTION_FAILURE_SHADOWS,
	CLASSIC_INTERACTION_FAILURE_CUSTOM_LIGHTING,
	CLASSIC_INTERACTION_FAILURE_DEFORM,
	CLASSIC_INTERACTION_FAILURE_SKINNING,
	CLASSIC_INTERACTION_FAILURE_SPECIAL_SURFACE,
	CLASSIC_INTERACTION_FAILURE_DEPTH_HACK,
	CLASSIC_INTERACTION_FAILURE_NEGATIVE_SCALE,
	CLASSIC_INTERACTION_FAILURE_FLAT_DIFFUSE,
	CLASSIC_INTERACTION_FAILURE_ENHANCED_MATERIAL,
	CLASSIC_INTERACTION_FAILURE_CEL_SHADING,
	CLASSIC_INTERACTION_FAILURE_ALTERNATE_INTERACTION_PROGRAM,
	CLASSIC_INTERACTION_FAILURE_UNSUPPORTED_MATERIAL,
	CLASSIC_INTERACTION_FAILURE_UNSUPPORTED_STATE,
	CLASSIC_INTERACTION_FAILURE_DYNAMIC_RESOURCE,
	CLASSIC_INTERACTION_FAILURE_MISSING_RESOURCE,
	CLASSIC_INTERACTION_FAILURE_DEFAULTED_RESOURCE,
	CLASSIC_INTERACTION_FAILURE_UNLOADED_RESOURCE,
	CLASSIC_INTERACTION_FAILURE_BACKEND_NOT_READY,
	CLASSIC_INTERACTION_FAILURE_BACKEND_COVERAGE_MISMATCH,
	CLASSIC_INTERACTION_FAILURE_BACKEND_REJECTED,
	CLASSIC_INTERACTION_FAILURE_COUNT
};

enum classicInteractionDomainBackend_t {
	CLASSIC_INTERACTION_BACKEND_GL = 0,
	CLASSIC_INTERACTION_BACKEND_VULKAN,
	CLASSIC_INTERACTION_BACKEND_COUNT
};

enum classicInteractionDomainBackendOutcome_t {
	CLASSIC_INTERACTION_BACKEND_UNRECORDED = 0,
	CLASSIC_INTERACTION_BACKEND_OWNED,
	CLASSIC_INTERACTION_BACKEND_FALLBACK
};

typedef struct classicInteractionDomainTexture_s {
	std::uint64_t		textureResourceId;
	const idImage		*image;
	std::uint64_t		storageGeneration;
	std::uint64_t		nameHash;
	unsigned int		textureHandle;
	textureFilter_t		filter;
	textureRepeat_t		repeat;
	bool			loaded;
	bool			defaulted;
	bool			mutableImage;
} classicInteractionDomainTexture_t;

typedef struct classicInteractionDomainPrimitive_s {
	// Geometry/identity bridges only.  Backends must not follow these to stages
	// or register arrays.
	const drawSurf_t		*legacyDrawSurf;
	const viewLight_t	*legacyViewLight;
	int			lightIndex;
	int			surfaceIndex;
	int			lightStageIndex;
	int			bumpStageIndex;
	int			diffuseStageIndex;
	int			specularStageIndex;
	classicInteractionDomainReceiver_t receiver;
	classicInteractionDomainPrimitiveDisposition_t disposition;
	classicInteractionDomainDepth_t depth;
	rendererCullMode_t	cull;
	rendererVertexColorMode_t vertexColor;
	rendererBlendState_t	blend;
	int			vertexCount;
	int			firstIndex;
	int			indexCount;
	int			vertexOffset;
	int			scissorX1;
	int			scissorY1;
	int			scissorX2;
	int			scissorY2;
	bool			ambientLight;
	bool			polygonOffsetEnabled;
	float			polygonOffsetFactor;
	float			polygonOffsetUnits;
	std::uint64_t		lightImageResourceId;
	std::uint64_t		lightFalloffImageResourceId;
	std::uint64_t		bumpImageResourceId;
	std::uint64_t		diffuseImageResourceId;
	std::uint64_t		specularImageResourceId;
	float			diffuseColor[ 4 ];
	float			specularColor[ 4 ];
	float			flatDiffuseParams[ 4 ];
	float			localLightOrigin[ 4 ];
	float			localViewOrigin[ 4 ];
	float			lightProjection[ 4 ][ 4 ];
	float			bumpMatrix[ 2 ][ 4 ];
	float			diffuseMatrix[ 2 ][ 4 ];
	float			specularMatrix[ 2 ][ 4 ];
	float			modelViewMatrix[ 16 ];
	std::uint64_t		hash;
} classicInteractionDomainPrimitive_t;

typedef struct classicInteractionDomainSurface_s {
	const drawSurf_t		*legacyDrawSurf;
	const viewLight_t	*legacyViewLight;
	int			drawPacketIndex;
	int			lightIndex;
	int			sourceOrdinal;
	int			receiverOrdinal;
	classicInteractionDomainReceiver_t receiver;
	int			materialTableRecordIndex;
	int			materialId;
	std::uint32_t		tableGeneration;
	int			firstPrimitive;
	int			primitiveCount;
	int			drawablePrimitiveCount;
	int			noopPrimitiveCount;
	int			surfaceStageCount;
	int			activeSurfaceStageCount;
	int			inactiveSurfaceStageCount;
	int			vertexCount;
	int			firstIndex;
	int			indexCount;
	int			vertexOffset;
	int			scissorX1;
	int			scissorY1;
	int			scissorX2;
	int			scissorY2;
	std::uint64_t		hash;
} classicInteractionDomainSurface_t;

typedef struct classicInteractionDomainLight_s {
	const viewLight_t	*legacyViewLight;
	int			sourceOrdinal;
	int			firstSurface;
	int			surfaceCount;
	int			firstPrimitive;
	int			primitiveCount;
	int			drawablePrimitiveCount;
	int			noopPrimitiveCount;
	int			lightStageCount;
	int			activeLightStageCount;
	int			inactiveLightStageCount;
	int			receiverSurfaceCount[ CLASSIC_INTERACTION_RECEIVER_COUNT ];
	int			receiverPrimitiveCount[ CLASSIC_INTERACTION_RECEIVER_COUNT ];
	int			scissorX1;
	int			scissorY1;
	int			scissorX2;
	int			scissorY2;
	float			globalLightOrigin[ 4 ];
	float			lightRadius[ 4 ];
	float			lightProject[ 4 ][ 4 ];
	bool			pointLight;
	bool			parallel;
	bool			ambientLight;
	bool			shadowClassified;
	std::uint64_t		hash;
} classicInteractionDomainLight_t;

typedef struct classicInteractionDomainBackendCoverage_s {
	std::uint64_t	ownedViewMask;
	std::uint64_t	fallbackViewMask;
	int		ownedViews;
	int		fallbackViews;
	int		ownedLights;
	int		fallbackLights;
	int		ownedSurfaces;
	int		fallbackSurfaces;
	int		ownedDrawablePrimitives;
	int		fallbackDrawablePrimitives;
	int		ownedNoopPrimitives;
	int		fallbackNoopPrimitives;
	int		coverageMismatches;
	int		duplicateReports;
	int		untrackedFallbacks;
} classicInteractionDomainBackendCoverage_t;

typedef struct classicInteractionDomainView_s {
	const viewDef_t		*viewDef;
	int			scenePacketIndex;
	int			interactionPassPacketIndex;
	std::uint32_t		tableGeneration;
	int			firstLight;
	int			lightCount;
	int			firstSurface;
	int			surfaceCount;
	int			firstPrimitive;
	int			primitiveCount;
	int			drawablePrimitiveCount;
	int			noopPrimitiveCount;
	int			packetDrawCount;
	int			activeLightStageCount;
	int			inactiveLightStageCount;
	int			activeSurfaceStageCount;
	int			inactiveSurfaceStageCount;
	int			receiverSurfaceCount[ CLASSIC_INTERACTION_RECEIVER_COUNT ];
	int			receiverPrimitiveCount[ CLASSIC_INTERACTION_RECEIVER_COUNT ];
	int			viewportX1;
	int			viewportY1;
	int			viewportX2;
	int			viewportY2;
	int			scissorX1;
	int			scissorY1;
	int			scissorX2;
	int			scissorY2;
	float			projectionMatrix[ 16 ];
	float			maxLightValue;
	float			lightScale;
	float			overBright;
	bool			ready;
	classicInteractionDomainFailure_t failure;
	int			failureDetail;
	int			failurePassPacketIndex;
	int			failureDrawPacketIndex;
	int			failureLightOrdinal;
	int			failureReceiverOrdinal;
	int			failureStageIndex;
	std::uint64_t		hash;
	classicInteractionDomainBackendOutcome_t backendOutcome[ CLASSIC_INTERACTION_BACKEND_COUNT ];
	classicInteractionDomainFailure_t backendFailure[ CLASSIC_INTERACTION_BACKEND_COUNT ];
	int			backendFailureDetail[ CLASSIC_INTERACTION_BACKEND_COUNT ];
	int			backendDrawnPrimitives[ CLASSIC_INTERACTION_BACKEND_COUNT ];
	int			backendNoopPrimitives[ CLASSIC_INTERACTION_BACKEND_COUNT ];
} classicInteractionDomainView_t;

typedef struct classicInteractionDomainStats_s {
	bool		prepared;
	bool		frameValid;
	bool		overflow;
	int		sourceScenes;
	int		interactionViews;
	int		readyViews;
	int		fallbackViews;
	int		lights;
	int		surfaces;
	int		primitives;
	int		drawablePrimitives;
	int		noopPrimitives;
	int		textures;
	int		activeLightStages;
	int		inactiveLightStages;
	int		activeSurfaceStages;
	int		inactiveSurfaceStages;
	int		receiverSurfaces[ CLASSIC_INTERACTION_RECEIVER_COUNT ];
	int		receiverPrimitives[ CLASSIC_INTERACTION_RECEIVER_COUNT ];
	int		failureCounts[ CLASSIC_INTERACTION_FAILURE_COUNT ];
	std::uint64_t	hash;
	classicInteractionDomainBackendCoverage_t backend[ CLASSIC_INTERACTION_BACKEND_COUNT ];
	char		status[ 96 ];
} classicInteractionDomainStats_t;

void R_ClassicInteractionDomain_ResetFrame( void );
void R_ClassicInteractionDomain_PrepareFrame( const idScenePacketFrame &packetFrame );
const classicInteractionDomainStats_t &R_ClassicInteractionDomain_Stats( void );
int R_ClassicInteractionDomain_NumViews( void );
const classicInteractionDomainView_t *R_ClassicInteractionDomain_ViewByIndex( int index );
const classicInteractionDomainView_t *R_ClassicInteractionDomain_ViewForScenePacket( int scenePacketIndex );
const classicInteractionDomainView_t *R_ClassicInteractionDomain_FindView( const viewDef_t *viewDef );
const classicInteractionDomainLight_t *R_ClassicInteractionDomain_ViewLight(
	const classicInteractionDomainView_t &view, int lightIndex );
const classicInteractionDomainSurface_t *R_ClassicInteractionDomain_ViewSurface(
	const classicInteractionDomainView_t &view, int surfaceIndex );
const classicInteractionDomainPrimitive_t *R_ClassicInteractionDomain_ViewPrimitive(
	const classicInteractionDomainView_t &view, int primitiveIndex );
const classicInteractionDomainSurface_t *R_ClassicInteractionDomain_LightSurface(
	const classicInteractionDomainLight_t &light, int surfaceIndex );
const classicInteractionDomainPrimitive_t *R_ClassicInteractionDomain_SurfacePrimitive(
	const classicInteractionDomainSurface_t &surface, int primitiveIndex );
const classicInteractionDomainTexture_t *R_ClassicInteractionDomain_ResolveTexture(
	std::uint64_t textureResourceId );
bool R_ClassicInteractionDomain_RecordOwned( const viewDef_t *viewDef,
	classicInteractionDomainBackend_t backend, int drawnPrimitives,
	int noopPrimitives );
void R_ClassicInteractionDomain_RecordBackendFallback( const viewDef_t *viewDef,
	classicInteractionDomainBackend_t backend,
	classicInteractionDomainFailure_t failure, int detail );
const classicInteractionDomainBackendCoverage_t &R_ClassicInteractionDomain_BackendCoverage(
	classicInteractionDomainBackend_t backend );
const char *ClassicInteractionDomainReceiver_Name( classicInteractionDomainReceiver_t receiver );
const char *ClassicInteractionDomainPrimitiveDisposition_Name(
	classicInteractionDomainPrimitiveDisposition_t disposition );
const char *ClassicInteractionDomainDepth_Name( classicInteractionDomainDepth_t depth );
const char *ClassicInteractionDomainFailure_Name( classicInteractionDomainFailure_t failure );
const char *ClassicInteractionDomainBackend_Name( classicInteractionDomainBackend_t backend );
bool RendererClassicInteractionDomain_RunSelfTest( void );

#endif /* !__CLASSIC_INTERACTION_DOMAIN_H__ */
