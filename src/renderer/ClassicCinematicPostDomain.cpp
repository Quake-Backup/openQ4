// Copyright (C) 2026 DarkMatter Productions
//

#include "tr_local.h"
#include "ClassicCinematicPostDomain.h"

#include <cmath>
#include <cstring>
#include <limits>

typedef struct classicCinematicPostDomainState_s {
	classicCinematicPostDomainView_t	views[
		CLASSIC_CINEMATIC_POST_DOMAIN_MAX_VIEWS ];
	int									viewCount;
	classicCinematicPostDomainStats_t			stats;
} classicCinematicPostDomainState_t;

static classicCinematicPostDomainState_t rg_classicCinematicPostDomain;

static std::uint64_t R_ClassicCinematicPostDomain_HashBytes(
		std::uint64_t hash, const void *data, size_t bytes ) {
	const byte *source = static_cast<const byte *>( data );
	for ( size_t i = 0; i < bytes; ++i ) {
		hash ^= source[ i ];
		hash *= UINT64_C( 1099511628211 );
	}
	return hash;
}

static std::uint64_t R_ClassicCinematicPostDomain_HashView(
		const classicCinematicPostDomainView_t &view ) {
	std::uint64_t hash = UINT64_C( 1469598103934665603 );
	hash = R_ClassicCinematicPostDomain_HashBytes( hash, &view.scope,
		sizeof( view.scope ) );
	hash = R_ClassicCinematicPostDomain_HashBytes( hash, &view.scenePacketIndex,
		sizeof( view.scenePacketIndex ) );
	hash = R_ClassicCinematicPostDomain_HashBytes( hash, &view.passPacketIndex,
		sizeof( view.passPacketIndex ) );
	hash = R_ClassicCinematicPostDomain_HashBytes( hash, &view.firstSourceSurface,
		sizeof( view.firstSourceSurface ) );
	hash = R_ClassicCinematicPostDomain_HashBytes( hash, &view.sourceSurfaceCount,
		sizeof( view.sourceSurfaceCount ) );
	hash = R_ClassicCinematicPostDomain_HashBytes( hash, &view.packetDrawCount,
		sizeof( view.packetDrawCount ) );
	hash = R_ClassicCinematicPostDomain_HashBytes( hash, &view.cinematicStageCount,
		sizeof( view.cinematicStageCount ) );
	hash = R_ClassicCinematicPostDomain_HashBytes( hash, &view.currentRenderStageCount,
		sizeof( view.currentRenderStageCount ) );
	hash = R_ClassicCinematicPostDomain_HashBytes( hash, &view.currentDepthStageCount,
		sizeof( view.currentDepthStageCount ) );
	hash = R_ClassicCinematicPostDomain_HashBytes( hash,
		&view.cinematicTimeMilliseconds, sizeof( view.cinematicTimeMilliseconds ) );
	return hash;
}

static bool R_ClassicCinematicPostDomain_IsCurrentRenderImage( const idImage *image ) {
	if ( image == NULL || globalImages == NULL ) {
		return false;
	}
	if ( image == globalImages->currentRenderImage
			|| image == globalImages->originalCurrentRenderImage ) {
		return true;
	}
	const char *name = image->GetName();
	return name != NULL && idStr::Icmpn( name, "_currentRender", 14 ) == 0;
}

static bool R_ClassicCinematicPostDomain_IsCurrentDepthImage( const idImage *image ) {
	if ( image == NULL || globalImages == NULL ) {
		return false;
	}
	if ( image == globalImages->currentDepthImage ) {
		return true;
	}
	const char *name = image->GetName();
	return name != NULL && idStr::Icmpn( name, "_currentDepth", 13 ) == 0;
}

static bool R_ClassicCinematicPostDomain_IsRootCinematicView(
		const viewDef_t *viewDef ) {
	return viewDef != NULL && viewDef->viewEntitys == NULL
		&& !viewDef->isSubview && !viewDef->isMirror && !viewDef->isXraySubview
		&& !viewDef->isEditor && viewDef->superView == NULL
		&& viewDef->subviewSurface == NULL && viewDef->numClipPlanes == 0
		&& viewDef->renderWorld == NULL && viewDef->viewLights == NULL
		&& viewDef->renderFlags == 0 && viewDef->renderView.globalMaterial == NULL
		&& viewDef->numOutlineDrawSurfs == 0;
}

static bool R_ClassicCinematicPostDomain_IsAuthoredPostView(
		const viewDef_t *viewDef ) {
	return viewDef != NULL && viewDef->viewEntitys != NULL
		&& !viewDef->isSubview && !viewDef->isMirror && !viewDef->isXraySubview
		&& !viewDef->isEditor && viewDef->superView == NULL
		&& viewDef->subviewSurface == NULL && viewDef->numClipPlanes == 0
		&& viewDef->renderWorld != NULL && viewDef->renderView.viewID >= 0
		&& ( viewDef->renderFlags & RF_PORTAL_SKY ) == 0
		&& viewDef->renderView.globalMaterial == NULL;
}

static bool R_ClassicCinematicPostDomain_StageCounts(
		const drawSurf_t *drawSurf, int &cinematicStages, int &currentRenderStages,
		int &currentDepthStages ) {
	if ( drawSurf == NULL || drawSurf->material == NULL ) {
		return false;
	}
	const idMaterial *material = drawSurf->material;
	const int stageCount = material->GetNumStages();
	for ( int stageIndex = 0; stageIndex < stageCount; ++stageIndex ) {
		const shaderStage_t *stage = material->GetStage( stageIndex );
		if ( stage == NULL ) {
			return false;
		}
		if ( stage->texture.cinematic != NULL ) {
			cinematicStages++;
		}
		if ( material->TestMaterialFlag( MF_NEED_CURRENT_RENDER )
				|| R_ClassicCinematicPostDomain_IsCurrentRenderImage(
					stage->texture.image ) ) {
			currentRenderStages++;
		}
		if ( R_ClassicCinematicPostDomain_IsCurrentDepthImage(
				stage->texture.image ) ) {
			currentDepthStages++;
		}
	}
	return true;
}

// Keep root 2D packet reconciliation in lockstep with the GUI packet builder.
// The dynamic stage walker remains free to retain legacy no-op behavior for
// non-drawable inputs, but every packet-admitted source must retain its exact
// original drawSurf identity and order before this domain can commit.
static bool R_ClassicCinematicPostDomain_IsRootGUIPacketEligible(
		const drawSurf_t *drawSurf ) {
	return drawSurf != NULL && drawSurf->geo != NULL
		&& drawSurf->geo->numIndexes > 0 && drawSurf->space != NULL
		&& drawSurf->material != NULL && drawSurf->material->HasAmbient()
		&& !drawSurf->material->IsPortalSky();
}

static int R_ClassicCinematicPostDomain_FindPass(
		const idScenePacketFrame &packetFrame, const scenePacket_t &scene,
		renderPassCategory_t category ) {
	for ( int passOffset = 0; passOffset < scene.passPacketCount; ++passOffset ) {
		const int passIndex = scene.firstPassPacket + passOffset;
		if ( passIndex < 0 || passIndex >= packetFrame.NumPasses() ) {
			return -1;
		}
		const passPacket_t &pass = packetFrame.Pass( passIndex );
		if ( pass.passCategory == category && !pass.commandOnly ) {
			return passIndex;
		}
	}
	return -1;
}

static void R_ClassicCinematicPostDomain_Fail(
		classicCinematicPostDomainView_t &view,
		classicCinematicPostDomainFailure_t failure, int detail ) {
	view.ready = false;
	view.failure = failure;
	view.failureDetail = detail;
	if ( failure > CLASSIC_CINEMATIC_POST_FAILURE_NONE
			&& failure < CLASSIC_CINEMATIC_POST_FAILURE_COUNT ) {
		rg_classicCinematicPostDomain.stats.failureCounts[ failure ]++;
	}
}

static void R_ClassicCinematicPostDomain_CommitView(
		classicCinematicPostDomainView_t &view ) {
	view.ready = true;
	view.failure = CLASSIC_CINEMATIC_POST_FAILURE_NONE;
	view.failureDetail = 0;
	view.hash = R_ClassicCinematicPostDomain_HashView( view );
	rg_classicCinematicPostDomain.stats.readyViews++;
	rg_classicCinematicPostDomain.stats.cinematicStages += view.cinematicStageCount;
	rg_classicCinematicPostDomain.stats.currentRenderStages += view.currentRenderStageCount;
	rg_classicCinematicPostDomain.stats.currentDepthStages += view.currentDepthStageCount;
}

static bool R_ClassicCinematicPostDomain_AddRootCinematicView(
		const idScenePacketFrame &packetFrame, int sceneIndex,
		const scenePacket_t &scene ) {
	const viewDef_t *viewDef = scene.viewDef;
	if ( !R_ClassicCinematicPostDomain_IsRootCinematicView( viewDef ) ) {
		return true;
	}
	if ( viewDef->numDrawSurfs <= 0 || viewDef->drawSurfs == NULL ) {
		return true;
	}
	int cinematicStages = 0;
	int currentRenderStages = 0;
	int currentDepthStages = 0;
	for ( int sourceIndex = 0; sourceIndex < viewDef->numDrawSurfs; ++sourceIndex ) {
		const drawSurf_t *drawSurf = viewDef->drawSurfs[ sourceIndex ];
		if ( drawSurf == NULL || drawSurf->material == NULL
			|| !R_ClassicCinematicPostDomain_StageCounts( drawSurf,
				cinematicStages, currentRenderStages, currentDepthStages ) ) {
			return true;
		}
		if ( drawSurf->material->GetSort() >= SS_POST_PROCESS ) {
			return true;
		}
	}
	if ( cinematicStages <= 0 ) {
		return true;
	}
	if ( rg_classicCinematicPostDomain.viewCount
			>= CLASSIC_CINEMATIC_POST_DOMAIN_MAX_VIEWS ) {
		rg_classicCinematicPostDomain.stats.overflow = true;
		return false;
	}
	classicCinematicPostDomainView_t &view =
		rg_classicCinematicPostDomain.views[
			rg_classicCinematicPostDomain.viewCount++ ];
	memset( &view, 0, sizeof( view ) );
	view.viewDef = viewDef;
	view.scope = CLASSIC_CINEMATIC_POST_SCOPE_ROOT_CINEMATIC;
	view.scenePacketIndex = sceneIndex;
	view.passPacketIndex = R_ClassicCinematicPostDomain_FindPass( packetFrame,
		scene, RENDER_PASS_GUI );
	view.firstSourceSurface = 0;
	view.sourceSurfaceCount = viewDef->numDrawSurfs;
	view.cinematicStageCount = cinematicStages;
	view.currentRenderStageCount = currentRenderStages;
	view.currentDepthStageCount = currentDepthStages;
	const double cinematicTime = 1000.0 * ( static_cast<double>( viewDef->floatTime )
		+ static_cast<double>( viewDef->renderView.shaderParms[ 11 ] ) );
	if ( !std::isfinite( cinematicTime )
			|| cinematicTime < static_cast<double>( ( std::numeric_limits<int>::min )() )
			|| cinematicTime > static_cast<double>( ( std::numeric_limits<int>::max )() ) ) {
		R_ClassicCinematicPostDomain_Fail( view,
			CLASSIC_CINEMATIC_POST_FAILURE_CINEMATIC_CLOCK, 0 );
		return true;
	}
	view.cinematicTimeMilliseconds = static_cast<int>( cinematicTime );
	if ( view.passPacketIndex < 0 ) {
		R_ClassicCinematicPostDomain_Fail( view,
			CLASSIC_CINEMATIC_POST_FAILURE_INVALID_PASS_RANGE, 0 );
		return true;
	}
	const passPacket_t &pass = packetFrame.Pass( view.passPacketIndex );
	if ( pass.packetCategory != SCENE_PACKET_CATEGORY_GUI
			|| pass.firstDrawPacket < 0 || pass.drawPacketCount < 0 ) {
		R_ClassicCinematicPostDomain_Fail( view,
			CLASSIC_CINEMATIC_POST_FAILURE_INVALID_PASS_RANGE, view.passPacketIndex );
		return true;
	}
	view.packetDrawCount = pass.drawPacketCount;
	int packetCursor = pass.firstDrawPacket;
	const int packetEnd = packetCursor + pass.drawPacketCount;
	for ( int sourceIndex = 0; sourceIndex < viewDef->numDrawSurfs;
			sourceIndex++ ) {
		const drawSurf_t *drawSurf = viewDef->drawSurfs[ sourceIndex ];
		if ( !R_ClassicCinematicPostDomain_IsRootGUIPacketEligible( drawSurf ) ) {
			continue;
		}
		if ( packetCursor >= packetEnd ) {
			R_ClassicCinematicPostDomain_Fail( view,
				CLASSIC_CINEMATIC_POST_FAILURE_SOURCE_PACKET_MISMATCH,
				sourceIndex );
			return true;
		}
		const drawPacket_t &packet = packetFrame.DrawPacket( packetCursor++ );
		if ( packet.legacyDrawSurf != drawSurf
				|| packet.passCategory != RENDER_PASS_GUI
				|| packet.packetCategory != SCENE_PACKET_CATEGORY_GUI ) {
			R_ClassicCinematicPostDomain_Fail( view,
				CLASSIC_CINEMATIC_POST_FAILURE_SOURCE_PACKET_MISMATCH,
				sourceIndex );
			return true;
		}
	}
	if ( packetCursor != packetEnd ) {
		R_ClassicCinematicPostDomain_Fail( view,
			CLASSIC_CINEMATIC_POST_FAILURE_SOURCE_PACKET_MISMATCH,
			packetEnd - packetCursor );
		return true;
	}
	R_ClassicCinematicPostDomain_CommitView( view );
	rg_classicCinematicPostDomain.stats.rootCinematicViews++;
	return true;
}

static bool R_ClassicCinematicPostDomain_AddAuthoredPostView(
		const idScenePacketFrame &packetFrame, int sceneIndex,
		const scenePacket_t &scene ) {
	const viewDef_t *viewDef = scene.viewDef;
	if ( !R_ClassicCinematicPostDomain_IsAuthoredPostView( viewDef )
			|| viewDef->numDrawSurfs <= 0 || viewDef->drawSurfs == NULL ) {
		return true;
	}
	int firstSource = viewDef->numDrawSurfs;
	for ( int sourceIndex = 0; sourceIndex < viewDef->numDrawSurfs; ++sourceIndex ) {
		const drawSurf_t *drawSurf = viewDef->drawSurfs[ sourceIndex ];
		if ( drawSurf != NULL && drawSurf->material != NULL
				&& drawSurf->material->GetSort() >= SS_POST_PROCESS ) {
			firstSource = sourceIndex;
			break;
		}
	}
	if ( firstSource >= viewDef->numDrawSurfs ) {
		return true;
	}
	if ( rg_classicCinematicPostDomain.viewCount
			>= CLASSIC_CINEMATIC_POST_DOMAIN_MAX_VIEWS ) {
		rg_classicCinematicPostDomain.stats.overflow = true;
		return false;
	}
	classicCinematicPostDomainView_t &view =
		rg_classicCinematicPostDomain.views[
			rg_classicCinematicPostDomain.viewCount++ ];
	memset( &view, 0, sizeof( view ) );
	view.viewDef = viewDef;
	view.scope = CLASSIC_CINEMATIC_POST_SCOPE_AUTHORED_POST;
	view.scenePacketIndex = sceneIndex;
	view.passPacketIndex = R_ClassicCinematicPostDomain_FindPass( packetFrame,
		scene, RENDER_PASS_AUTHORED_POST );
	view.firstSourceSurface = firstSource;
	view.sourceSurfaceCount = viewDef->numDrawSurfs - firstSource;
	if ( view.passPacketIndex < 0 ) {
		R_ClassicCinematicPostDomain_Fail( view,
			CLASSIC_CINEMATIC_POST_FAILURE_INVALID_PASS_RANGE, 0 );
		return true;
	}
	const passPacket_t &pass = packetFrame.Pass( view.passPacketIndex );
	if ( pass.packetCategory != SCENE_PACKET_CATEGORY_POST_PROCESS
			|| pass.firstDrawPacket < 0 || pass.drawPacketCount < 0 ) {
		R_ClassicCinematicPostDomain_Fail( view,
			CLASSIC_CINEMATIC_POST_FAILURE_INVALID_PASS_RANGE, view.passPacketIndex );
		return true;
	}
	view.packetDrawCount = pass.drawPacketCount;
	int packetCursor = pass.firstDrawPacket;
	const int packetEnd = packetCursor + pass.drawPacketCount;
	for ( int sourceIndex = firstSource; sourceIndex < viewDef->numDrawSurfs;
			++sourceIndex ) {
		const drawSurf_t *drawSurf = viewDef->drawSurfs[ sourceIndex ];
		if ( drawSurf == NULL || drawSurf->material == NULL
				|| drawSurf->material->GetSort() < SS_POST_PROCESS ) {
			R_ClassicCinematicPostDomain_Fail( view,
				CLASSIC_CINEMATIC_POST_FAILURE_INVALID_SOURCE_SURFACE, sourceIndex );
			return true;
		}
		if ( !R_ClassicCinematicPostDomain_StageCounts( drawSurf,
				view.cinematicStageCount, view.currentRenderStageCount,
				view.currentDepthStageCount ) ) {
			R_ClassicCinematicPostDomain_Fail( view,
				CLASSIC_CINEMATIC_POST_FAILURE_INVALID_SOURCE_SURFACE, sourceIndex );
			return true;
		}
		if ( drawSurf->material->HasAmbient() && !drawSurf->material->IsPortalSky()
				&& !drawSurf->material->SuppressInSubview() ) {
			if ( packetCursor >= packetEnd ) {
				R_ClassicCinematicPostDomain_Fail( view,
					CLASSIC_CINEMATIC_POST_FAILURE_SOURCE_PACKET_MISMATCH,
					sourceIndex );
				return true;
			}
			const drawPacket_t &packet = packetFrame.DrawPacket( packetCursor++ );
			if ( packet.legacyDrawSurf != drawSurf
					|| packet.passCategory != RENDER_PASS_AUTHORED_POST
					|| packet.packetCategory != SCENE_PACKET_CATEGORY_POST_PROCESS ) {
				R_ClassicCinematicPostDomain_Fail( view,
					CLASSIC_CINEMATIC_POST_FAILURE_SOURCE_PACKET_MISMATCH,
					sourceIndex );
				return true;
			}
		}
	}
	if ( packetCursor != packetEnd ) {
		R_ClassicCinematicPostDomain_Fail( view,
			CLASSIC_CINEMATIC_POST_FAILURE_SOURCE_PACKET_MISMATCH,
			packetEnd - packetCursor );
		return true;
	}
	R_ClassicCinematicPostDomain_CommitView( view );
	rg_classicCinematicPostDomain.stats.authoredPostViews++;
	return true;
}

void R_ClassicCinematicPostDomain_ResetFrame( void ) {
	memset( &rg_classicCinematicPostDomain, 0,
		sizeof( rg_classicCinematicPostDomain ) );
	idStr::Copynz( rg_classicCinematicPostDomain.stats.status, "reset",
		sizeof( rg_classicCinematicPostDomain.stats.status ) );
}

void R_ClassicCinematicPostDomain_PrepareFrame(
		const idScenePacketFrame &packetFrame ) {
	R_ClassicCinematicPostDomain_ResetFrame();
	classicCinematicPostDomainStats_t &stats = rg_classicCinematicPostDomain.stats;
	stats.prepared = true;
	stats.frameValid = packetFrame.Stats().frontEndDerived
		|| packetFrame.Stats().backendDerived;
	stats.sourceScenes = packetFrame.NumScenes();
	if ( !stats.frameValid || packetFrame.Stats().overflow ) {
		stats.overflow = packetFrame.Stats().overflow;
		idStr::Copynz( stats.status, "packet frame unavailable", sizeof( stats.status ) );
		return;
	}
	for ( int sceneIndex = 0; sceneIndex < packetFrame.NumScenes(); ++sceneIndex ) {
		const scenePacket_t &scene = packetFrame.Scene( sceneIndex );
		if ( scene.viewDef == NULL ) {
			continue;
		}
		if ( !R_ClassicCinematicPostDomain_AddRootCinematicView( packetFrame,
				sceneIndex, scene )
				|| !R_ClassicCinematicPostDomain_AddAuthoredPostView( packetFrame,
				sceneIndex, scene ) ) {
			stats.overflow = true;
			break;
		}
	}
	stats.views = rg_classicCinematicPostDomain.viewCount;
	for ( int viewIndex = 0; viewIndex < rg_classicCinematicPostDomain.viewCount;
			++viewIndex ) {
		const classicCinematicPostDomainView_t &view =
			rg_classicCinematicPostDomain.views[ viewIndex ];
		if ( !view.ready ) {
			stats.fallbackViews++;
		}
		stats.hash ^= view.hash;
	}
	idStr::Copynz( stats.status, stats.overflow ? "overflow" : "prepared",
		sizeof( stats.status ) );
}

const classicCinematicPostDomainStats_t &R_ClassicCinematicPostDomain_Stats( void ) {
	return rg_classicCinematicPostDomain.stats;
}

static const classicCinematicPostDomainView_t *R_ClassicCinematicPostDomain_Find(
		const viewDef_t *viewDef, classicCinematicPostDomainScope_t scope ) {
	for ( int viewIndex = 0; viewIndex < rg_classicCinematicPostDomain.viewCount;
			++viewIndex ) {
		const classicCinematicPostDomainView_t &view =
			rg_classicCinematicPostDomain.views[ viewIndex ];
		if ( view.viewDef == viewDef && view.scope == scope ) {
			return &view;
		}
	}
	return NULL;
}

const classicCinematicPostDomainView_t *R_ClassicCinematicPostDomain_FindRootCinematicView(
		const viewDef_t *viewDef ) {
	return R_ClassicCinematicPostDomain_Find( viewDef,
		CLASSIC_CINEMATIC_POST_SCOPE_ROOT_CINEMATIC );
}

const classicCinematicPostDomainView_t *R_ClassicCinematicPostDomain_FindAuthoredPostView(
		const viewDef_t *viewDef ) {
	return R_ClassicCinematicPostDomain_Find( viewDef,
		CLASSIC_CINEMATIC_POST_SCOPE_AUTHORED_POST );
}

bool R_ClassicCinematicPostDomain_ReadyForBackend( const viewDef_t *viewDef,
		classicCinematicPostDomainScope_t scope,
		classicCinematicPostDomainBackend_t backend ) {
	const classicCinematicPostDomainView_t *view =
		R_ClassicCinematicPostDomain_Find( viewDef, scope );
	return view != NULL && view->ready
		&& backend >= CLASSIC_CINEMATIC_POST_BACKEND_GL
		&& backend < CLASSIC_CINEMATIC_POST_BACKEND_COUNT
		&& view->backendOutcome[ backend ]
			!= CLASSIC_CINEMATIC_POST_BACKEND_FALLBACK;
}

void R_ClassicCinematicPostDomain_RecordBackendFallback(
		const viewDef_t *viewDef, classicCinematicPostDomainScope_t scope,
		classicCinematicPostDomainBackend_t backend,
		classicCinematicPostDomainFailure_t failure, int detail ) {
	classicCinematicPostDomainView_t *view = const_cast<classicCinematicPostDomainView_t *>(
		R_ClassicCinematicPostDomain_Find( viewDef, scope ) );
	if ( view == NULL || backend < CLASSIC_CINEMATIC_POST_BACKEND_GL
			|| backend >= CLASSIC_CINEMATIC_POST_BACKEND_COUNT ) {
		return;
	}
	classicCinematicPostDomainBackendCoverage_t &coverage =
		rg_classicCinematicPostDomain.stats.backend[ backend ];
	if ( view->backendOutcome[ backend ] == CLASSIC_CINEMATIC_POST_BACKEND_OWNED ) {
		coverage.duplicateReports++;
		return;
	}
	if ( view->backendOutcome[ backend ] == CLASSIC_CINEMATIC_POST_BACKEND_FALLBACK ) {
		coverage.duplicateReports++;
		return;
	}
	view->backendOutcome[ backend ] = CLASSIC_CINEMATIC_POST_BACKEND_FALLBACK;
	view->backendFailure[ backend ] = failure;
	view->backendFailureDetail[ backend ] = detail;
	coverage.fallbackViews++;
	coverage.fallbackSurfaces += view->sourceSurfaceCount;
}

bool R_ClassicCinematicPostDomain_RecordOwned( const viewDef_t *viewDef,
		classicCinematicPostDomainScope_t scope,
		classicCinematicPostDomainBackend_t backend, int drawnSurfaces ) {
	classicCinematicPostDomainView_t *view = const_cast<classicCinematicPostDomainView_t *>(
		R_ClassicCinematicPostDomain_Find( viewDef, scope ) );
	if ( view == NULL || !view->ready || backend < CLASSIC_CINEMATIC_POST_BACKEND_GL
			|| backend >= CLASSIC_CINEMATIC_POST_BACKEND_COUNT
			|| drawnSurfaces != view->sourceSurfaceCount ) {
		if ( view != NULL && backend >= CLASSIC_CINEMATIC_POST_BACKEND_GL
				&& backend < CLASSIC_CINEMATIC_POST_BACKEND_COUNT ) {
			rg_classicCinematicPostDomain.stats.backend[ backend ].coverageMismatches++;
		}
		return false;
	}
	classicCinematicPostDomainBackendCoverage_t &coverage =
		rg_classicCinematicPostDomain.stats.backend[ backend ];
	if ( view->backendOutcome[ backend ] == CLASSIC_CINEMATIC_POST_BACKEND_FALLBACK ) {
		coverage.coverageMismatches++;
		return false;
	}
	if ( view->backendOutcome[ backend ] == CLASSIC_CINEMATIC_POST_BACKEND_OWNED ) {
		coverage.duplicateReports++;
		return true;
	}
	view->backendOutcome[ backend ] = CLASSIC_CINEMATIC_POST_BACKEND_OWNED;
	view->backendDrawnSurfaces[ backend ] = drawnSurfaces;
	coverage.ownedViews++;
	coverage.ownedSurfaces += drawnSurfaces;
	return true;
}

const char *ClassicCinematicPostDomainFailure_Name(
		classicCinematicPostDomainFailure_t failure ) {
	switch ( failure ) {
	case CLASSIC_CINEMATIC_POST_FAILURE_NONE: return "none";
	case CLASSIC_CINEMATIC_POST_FAILURE_UNAVAILABLE: return "unavailable";
	case CLASSIC_CINEMATIC_POST_FAILURE_SCENE_PACKET_OVERFLOW: return "scenePacketOverflow";
	case CLASSIC_CINEMATIC_POST_FAILURE_VIEW_POOL_OVERFLOW: return "viewPoolOverflow";
	case CLASSIC_CINEMATIC_POST_FAILURE_UNSUPPORTED_VIEW: return "unsupportedView";
	case CLASSIC_CINEMATIC_POST_FAILURE_INVALID_SCENE_RANGE: return "invalidSceneRange";
	case CLASSIC_CINEMATIC_POST_FAILURE_INVALID_PASS_RANGE: return "invalidPassRange";
	case CLASSIC_CINEMATIC_POST_FAILURE_SOURCE_PACKET_MISMATCH: return "sourcePacketMismatch";
	case CLASSIC_CINEMATIC_POST_FAILURE_INVALID_SOURCE_SURFACE: return "invalidSourceSurface";
	case CLASSIC_CINEMATIC_POST_FAILURE_CINEMATIC_CLOCK: return "cinematicClock";
	case CLASSIC_CINEMATIC_POST_FAILURE_BACKEND_NOT_READY: return "backendNotReady";
	case CLASSIC_CINEMATIC_POST_FAILURE_BACKEND_REJECTED: return "backendRejected";
	case CLASSIC_CINEMATIC_POST_FAILURE_BACKEND_COVERAGE_MISMATCH: return "backendCoverageMismatch";
	default: return "unknown";
	}
}

bool RendererClassicCinematicPostDomain_RunSelfTest( void ) {
	R_ClassicCinematicPostDomain_ResetFrame();
	const classicCinematicPostDomainStats_t &resetStats =
		R_ClassicCinematicPostDomain_Stats();
	if ( CLASSIC_CINEMATIC_POST_DOMAIN_MAX_VIEWS != SCENE_PACKET_MAX_SCENES
			|| idStr::Cmp( ClassicCinematicPostDomainFailure_Name(
				CLASSIC_CINEMATIC_POST_FAILURE_SOURCE_PACKET_MISMATCH ),
				"sourcePacketMismatch" )
			|| idStr::Cmp( ClassicCinematicPostDomainFailure_Name(
				CLASSIC_CINEMATIC_POST_FAILURE_CINEMATIC_CLOCK ),
				"cinematicClock" )
			|| resetStats.prepared || resetStats.frameValid || resetStats.overflow
			|| resetStats.views != 0 || resetStats.readyViews != 0
			|| R_ClassicCinematicPostDomain_ReadyForBackend( NULL,
				CLASSIC_CINEMATIC_POST_SCOPE_ROOT_CINEMATIC,
				CLASSIC_CINEMATIC_POST_BACKEND_GL )
			|| R_ClassicCinematicPostDomain_RecordOwned( NULL,
				CLASSIC_CINEMATIC_POST_SCOPE_AUTHORED_POST,
				CLASSIC_CINEMATIC_POST_BACKEND_VULKAN, 0 ) ) {
		return false;
	}

	viewDef_t viewDef;
	memset( &viewDef, 0, sizeof( viewDef ) );
	classicCinematicPostDomainView_t &view =
		rg_classicCinematicPostDomain.views[ 0 ];
	memset( &view, 0, sizeof( view ) );
	view.viewDef = &viewDef;
	view.scope = CLASSIC_CINEMATIC_POST_SCOPE_ROOT_CINEMATIC;
	view.scenePacketIndex = 3;
	view.passPacketIndex = 7;
	view.firstSourceSurface = 0;
	view.sourceSurfaceCount = 3;
	view.packetDrawCount = 2;
	view.cinematicStageCount = 1;
	view.cinematicTimeMilliseconds = 1000;
	view.ready = true;
	const std::uint64_t firstHash = R_ClassicCinematicPostDomain_HashView( view );
	view.cinematicTimeMilliseconds++;
	const bool hashSensitive = R_ClassicCinematicPostDomain_HashView( view )
		!= firstHash;
	view.cinematicTimeMilliseconds--;
	view.hash = firstHash;
	rg_classicCinematicPostDomain.viewCount = 1;
	memset( &rg_classicCinematicPostDomain.stats, 0,
		sizeof( rg_classicCinematicPostDomain.stats ) );

	const bool owned = R_ClassicCinematicPostDomain_RecordOwned( &viewDef,
		CLASSIC_CINEMATIC_POST_SCOPE_ROOT_CINEMATIC,
		CLASSIC_CINEMATIC_POST_BACKEND_GL, 3 );
	const bool duplicate = R_ClassicCinematicPostDomain_RecordOwned( &viewDef,
		CLASSIC_CINEMATIC_POST_SCOPE_ROOT_CINEMATIC,
		CLASSIC_CINEMATIC_POST_BACKEND_GL, 3 );
	const bool mismatch = !R_ClassicCinematicPostDomain_RecordOwned( &viewDef,
		CLASSIC_CINEMATIC_POST_SCOPE_ROOT_CINEMATIC,
		CLASSIC_CINEMATIC_POST_BACKEND_VULKAN, 2 );
	R_ClassicCinematicPostDomain_RecordBackendFallback( &viewDef,
		CLASSIC_CINEMATIC_POST_SCOPE_ROOT_CINEMATIC,
		CLASSIC_CINEMATIC_POST_BACKEND_VULKAN,
		CLASSIC_CINEMATIC_POST_FAILURE_BACKEND_COVERAGE_MISMATCH, 2 );
	const classicCinematicPostDomainStats_t &stats =
		R_ClassicCinematicPostDomain_Stats();
	const bool coverageContract = owned && duplicate && mismatch
		&& view.backendOutcome[ CLASSIC_CINEMATIC_POST_BACKEND_GL ]
			== CLASSIC_CINEMATIC_POST_BACKEND_OWNED
		&& view.backendOutcome[ CLASSIC_CINEMATIC_POST_BACKEND_VULKAN ]
			== CLASSIC_CINEMATIC_POST_BACKEND_FALLBACK
		&& !R_ClassicCinematicPostDomain_ReadyForBackend( &viewDef,
			CLASSIC_CINEMATIC_POST_SCOPE_ROOT_CINEMATIC,
			CLASSIC_CINEMATIC_POST_BACKEND_VULKAN )
		&& stats.backend[ CLASSIC_CINEMATIC_POST_BACKEND_GL ].ownedViews == 1
		&& stats.backend[ CLASSIC_CINEMATIC_POST_BACKEND_GL ].duplicateReports == 1
		&& stats.backend[ CLASSIC_CINEMATIC_POST_BACKEND_VULKAN ]
			.coverageMismatches == 1
		&& stats.backend[ CLASSIC_CINEMATIC_POST_BACKEND_VULKAN ].fallbackViews == 1;
	R_ClassicCinematicPostDomain_ResetFrame();
	return hashSensitive && coverageContract;
}
