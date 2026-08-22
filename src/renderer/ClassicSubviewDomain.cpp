// Copyright (C) 2026 DarkMatter Productions
//

#include "tr_local.h"
#include "ClassicSubviewDomain.h"

#include <cstring>

namespace {

const std::uint64_t HASH_OFFSET = 1469598103934665603ull;
const std::uint64_t HASH_PRIME = 1099511628211ull;

typedef struct classicSubviewDomainState_s {
	classicSubviewDomainView_t views[CLASSIC_SUBVIEW_DOMAIN_MAX_VIEWS];
	classicSubviewDomainStats_t stats;
	int viewCount;
} classicSubviewDomainState_t;

classicSubviewDomainState_t domain;

static void HashByte( std::uint64_t &hash, unsigned int value ) {
	hash ^= value & 0xffu;
	hash *= HASH_PRIME;
}

static void HashU32( std::uint64_t &hash, std::uint32_t value ) {
	for ( int i = 0; i < 4; ++i ) {
		HashByte( hash, value >> ( i * 8 ) );
	}
}

static void HashInt( std::uint64_t &hash, int value ) {
	HashU32( hash, static_cast<std::uint32_t>( value ) );
}

static void HashBool( std::uint64_t &hash, bool value ) {
	HashByte( hash, value ? 1u : 0u );
}

static void InitView( classicSubviewDomainView_t &view,
		const viewDef_t *viewDef, int scenePacketIndex ) {
	std::memset( &view, 0, sizeof( view ) );
	view.viewDef = viewDef;
	view.scenePacketIndex = scenePacketIndex;
	view.parentScenePacketIndex = -1;
	view.capturePacketIndex = -1;
	view.firstPassPacket = -1;
	view.firstDrawPacket = -1;
	view.failure = CLASSIC_SUBVIEW_DOMAIN_FAILURE_NONE;
}

static int ViewIndex( const classicSubviewDomainView_t *view ) {
	if ( view == NULL || view < domain.views
			|| view >= domain.views + domain.viewCount ) {
		return -1;
	}
	return static_cast<int>( view - domain.views );
}

static classicSubviewDomainView_t *FindMutableView( const viewDef_t *viewDef ) {
	for ( int i = 0; i < domain.viewCount; ++i ) {
		if ( domain.views[i].viewDef == viewDef ) {
			return &domain.views[i];
		}
	}
	return NULL;
}

static bool FailView( classicSubviewDomainView_t &view,
		classicSubviewDomainFailure_t failure, int detail ) {
	view.ready = false;
	view.failure = failure;
	view.failureDetail = detail;
	view.hash = 0;
	domain.stats.fallbackViews++;
	if ( failure >= CLASSIC_SUBVIEW_DOMAIN_FAILURE_NONE
			&& failure < CLASSIC_SUBVIEW_DOMAIN_FAILURE_COUNT ) {
		domain.stats.failureCounts[failure]++;
	}
	return false;
}

static int FindScenePacket( const idScenePacketFrame &packetFrame,
		const viewDef_t *viewDef ) {
	for ( int i = 0; i < packetFrame.NumScenes(); ++i ) {
		if ( packetFrame.Scene(i).viewDef == viewDef ) {
			return i;
		}
	}
	return -1;
}

static bool ParentContainsSurface( const viewDef_t *parent,
		const drawSurf_t *surface ) {
	if ( parent == NULL || surface == NULL || parent->drawSurfs == NULL
			|| parent->numDrawSurfs <= 0 ) {
		return false;
	}
	for ( int i = 0; i < parent->numDrawSurfs; ++i ) {
		if ( parent->drawSurfs[i] == surface ) {
			return true;
		}
	}
	return false;
}

static bool FindCaptureKind( const drawSurf_t *surface,
		const idImage *captureImage, classicSubviewDomainKind_t &kind ) {
	kind = CLASSIC_SUBVIEW_DOMAIN_KIND_NONE;
	if ( surface == NULL || surface->material == NULL || captureImage == NULL
			|| !surface->material->HasSubview() ) {
		return false;
	}
	int matchingStages = 0;
	const int stageCount = surface->material->GetNumStages();
	for ( int i = 0; i < stageCount; ++i ) {
		const shaderStage_t *stage = surface->material->GetStage(i);
		if ( stage == NULL ) {
			return false;
		}
		classicSubviewDomainKind_t stageKind = CLASSIC_SUBVIEW_DOMAIN_KIND_NONE;
		switch ( stage->texture.dynamic ) {
		case DI_REMOTE_RENDER:
			stageKind = CLASSIC_SUBVIEW_DOMAIN_KIND_REMOTE_CAMERA;
			break;
		case DI_REFRACTION_RENDER:
			stageKind = CLASSIC_SUBVIEW_DOMAIN_KIND_REFRACTION;
			break;
		case DI_STATIC:
			continue;
		default:
			// Mirror/reflection/xray have their own camera or clip semantics.
			return false;
		}
		if ( stage->texture.image != captureImage || matchingStages != 0 ) {
			return false;
		}
		kind = stageKind;
		matchingStages++;
	}
	return matchingStages == 1;
}

static std::uint64_t HashView( const classicSubviewDomainView_t &view ) {
	std::uint64_t hash = HASH_OFFSET;
	HashInt( hash, view.scenePacketIndex );
	HashInt( hash, view.parentScenePacketIndex );
	HashInt( hash, view.capturePacketIndex );
	HashInt( hash, view.firstPassPacket );
	HashInt( hash, view.passPacketCount );
	HashInt( hash, view.firstDrawPacket );
	HashInt( hash, view.drawPacketCount );
	HashInt( hash, view.captureX );
	HashInt( hash, view.captureY );
	HashInt( hash, view.captureWidth );
	HashInt( hash, view.captureHeight );
	HashInt( hash, view.captureCubeFace );
	HashInt( hash, view.kind );
	if ( view.viewDef != NULL ) {
		HashInt( hash, view.viewDef->viewport.x1 );
		HashInt( hash, view.viewDef->viewport.y1 );
		HashInt( hash, view.viewDef->viewport.x2 );
		HashInt( hash, view.viewDef->viewport.y2 );
		HashInt( hash, view.viewDef->scissor.x1 );
		HashInt( hash, view.viewDef->scissor.y1 );
		HashInt( hash, view.viewDef->scissor.x2 );
		HashInt( hash, view.viewDef->scissor.y2 );
		HashBool( hash, view.viewDef->isSubview );
		HashBool( hash, view.viewDef->isMirror );
		HashBool( hash, view.viewDef->isXraySubview );
	}
	return hash;
}

static bool PrepareView( const idScenePacketFrame &packetFrame,
		const scenePacket_t &scene, classicSubviewDomainView_t &view ) {
	const viewDef_t *viewDef = view.viewDef;
	if ( viewDef == NULL || scene.packetCategory != SCENE_PACKET_CATEGORY_SUBVIEW
			|| !viewDef->isSubview || viewDef->viewEntitys == NULL
			|| viewDef->renderWorld == NULL || viewDef->isMirror
			|| viewDef->isXraySubview || viewDef->isEditor
			|| viewDef->numClipPlanes != 0 || viewDef->renderView.viewID < 0
			|| viewDef->renderView.globalMaterial != NULL
			|| viewDef->superView == NULL || viewDef->subviewSurface == NULL
			|| viewDef->superView->isSubview ) {
		return FailView( view, CLASSIC_SUBVIEW_DOMAIN_FAILURE_UNSUPPORTED_VIEW, 0 );
	}
	view.parentViewDef = viewDef->superView;
	view.parentSurface = viewDef->subviewSurface;
	view.parentScenePacketIndex = FindScenePacket( packetFrame, view.parentViewDef );
	if ( view.parentScenePacketIndex < 0 ) {
		return FailView( view, CLASSIC_SUBVIEW_DOMAIN_FAILURE_MISSING_PARENT_SCENE, 0 );
	}
	if ( !ParentContainsSurface( view.parentViewDef, view.parentSurface ) ) {
		return FailView( view, CLASSIC_SUBVIEW_DOMAIN_FAILURE_MISSING_PARENT_SURFACE, 0 );
	}
	int captureIndex = -1;
	for ( int i = 0; i < packetFrame.NumSubviewCaptures(); ++i ) {
		const sceneSubviewCapture_t &candidate = packetFrame.SubviewCapture(i);
		if ( candidate.viewScenePacketIndex != view.scenePacketIndex ) {
			continue;
		}
		if ( captureIndex >= 0 ) {
			return FailView( view, CLASSIC_SUBVIEW_DOMAIN_FAILURE_DUPLICATE_CAPTURE, i );
		}
		captureIndex = i;
	}
	if ( captureIndex < 0 ) {
		return FailView( view, CLASSIC_SUBVIEW_DOMAIN_FAILURE_MISSING_CAPTURE, 0 );
	}
	const sceneSubviewCapture_t &capture = packetFrame.SubviewCapture(captureIndex);
	if ( capture.viewDef != viewDef || capture.image == NULL || capture.copyDepth
			|| capture.cubeFace != 0 || capture.width <= 0 || capture.height <= 0 ) {
		return FailView( view, CLASSIC_SUBVIEW_DOMAIN_FAILURE_INVALID_CAPTURE,
			captureIndex );
	}
	view.captureImage = capture.image;
	view.capturePacketIndex = captureIndex;
	view.captureX = capture.x;
	view.captureY = capture.y;
	view.captureWidth = capture.width;
	view.captureHeight = capture.height;
	view.captureCubeFace = capture.cubeFace;
	if ( capture.x != viewDef->viewport.x1 || capture.y != viewDef->viewport.y1
			|| capture.width != viewDef->viewport.x2 - viewDef->viewport.x1 + 1
			|| capture.height != viewDef->viewport.y2 - viewDef->viewport.y1 + 1 ) {
		return FailView( view,
			CLASSIC_SUBVIEW_DOMAIN_FAILURE_CAPTURE_VIEWPORT_MISMATCH, captureIndex );
	}
	if ( !FindCaptureKind( view.parentSurface, capture.image, view.kind ) ) {
		return FailView( view,
			CLASSIC_SUBVIEW_DOMAIN_FAILURE_CAPTURE_SURFACE_MISMATCH, captureIndex );
	}
	view.firstPassPacket = scene.firstPassPacket;
	view.passPacketCount = scene.passPacketCount;
	view.firstDrawPacket = scene.firstDrawPacket;
	view.drawPacketCount = scene.drawPacketCount;
	view.ready = true;
	view.hash = HashView( view );
	return true;
}

static void RecordFallback( classicSubviewDomainView_t *view,
		classicSubviewDomainBackend_t backend, classicSubviewDomainFailure_t failure,
		int detail ) {
	if ( backend < CLASSIC_SUBVIEW_DOMAIN_BACKEND_GL
			|| backend >= CLASSIC_SUBVIEW_DOMAIN_BACKEND_COUNT ) {
		return;
	}
	classicSubviewDomainBackendCoverage_t &coverage = domain.stats.backend[backend];
	if ( view == NULL ) {
		coverage.untrackedFallbacks++;
		return;
	}
	if ( view->backendOutcome[backend] != CLASSIC_SUBVIEW_DOMAIN_BACKEND_UNRECORDED ) {
		coverage.duplicateReports++;
		return;
	}
	const int index = ViewIndex(view);
	view->backendOutcome[backend] = CLASSIC_SUBVIEW_DOMAIN_BACKEND_FALLBACK;
	view->backendFailure[backend] = failure;
	view->backendFailureDetail[backend] = detail;
	if ( index >= 0 ) {
		coverage.fallbackViewMask |= 1ull << index;
	}
	coverage.fallbackViews++;
}

} // namespace

void R_ClassicSubviewDomain_ResetFrame( void ) {
	std::memset( &domain.stats, 0, sizeof( domain.stats ) );
	domain.viewCount = 0;
	idStr::Copynz( domain.stats.status, "empty", sizeof( domain.stats.status ) );
}

void R_ClassicSubviewDomain_PrepareFrame( const idScenePacketFrame &packetFrame ) {
	R_ClassicSubviewDomain_ResetFrame();
	domain.stats.prepared = true;
	domain.stats.sourceScenes = packetFrame.NumScenes();
	domain.stats.capturePackets = packetFrame.NumSubviewCaptures();
	domain.stats.overflow = packetFrame.Stats().overflow;
	if ( packetFrame.Stats().overflow ) {
		domain.stats.failureCounts[CLASSIC_SUBVIEW_DOMAIN_FAILURE_SCENE_PACKET_OVERFLOW]++;
	}
	for ( int sceneIndex = 0; sceneIndex < packetFrame.NumScenes(); ++sceneIndex ) {
		const scenePacket_t &scene = packetFrame.Scene(sceneIndex);
		if ( scene.packetCategory != SCENE_PACKET_CATEGORY_SUBVIEW ) {
			continue;
		}
		domain.stats.subviewScenes++;
		if ( domain.viewCount >= CLASSIC_SUBVIEW_DOMAIN_MAX_VIEWS ) {
			domain.stats.overflow = true;
			domain.stats.fallbackViews++;
			domain.stats.failureCounts[CLASSIC_SUBVIEW_DOMAIN_FAILURE_CAPTURE_POOL_OVERFLOW]++;
			continue;
		}
		classicSubviewDomainView_t &view = domain.views[domain.viewCount++];
		InitView( view, scene.viewDef, sceneIndex );
		if ( PrepareView( packetFrame, scene, view ) ) {
			domain.stats.readyViews++;
			if ( view.kind == CLASSIC_SUBVIEW_DOMAIN_KIND_REMOTE_CAMERA ) {
				domain.stats.remoteCameraViews++;
			} else if ( view.kind == CLASSIC_SUBVIEW_DOMAIN_KIND_REFRACTION ) {
				domain.stats.refractionViews++;
			}
		}
	}
	domain.stats.frameValid = !domain.stats.overflow
		&& domain.stats.fallbackViews == 0;
	std::uint64_t hash = HASH_OFFSET;
	HashInt( hash, domain.stats.subviewScenes );
	HashInt( hash, domain.stats.capturePackets );
	for ( int i = 0; i < domain.viewCount; ++i ) {
		HashBool( hash, domain.views[i].ready );
		HashInt( hash, domain.views[i].failure );
		HashInt( hash, domain.views[i].failureDetail );
		HashInt( hash, domain.views[i].kind );
		HashInt( hash, static_cast<int>( domain.views[i].hash ) );
		HashInt( hash, static_cast<int>( domain.views[i].hash >> 32 ) );
	}
	domain.stats.hash = hash;
	if ( domain.stats.subviewScenes == 0 ) {
		idStr::Copynz( domain.stats.status, "empty", sizeof( domain.stats.status ) );
	} else if ( domain.stats.frameValid ) {
		idStr::Copynz( domain.stats.status, "ready", sizeof( domain.stats.status ) );
	} else if ( domain.stats.readyViews == 0 ) {
		idStr::Copynz( domain.stats.status, "fallback", sizeof( domain.stats.status ) );
	} else {
		idStr::Copynz( domain.stats.status, "mixed-view-fallback", sizeof( domain.stats.status ) );
	}
}

const classicSubviewDomainStats_t &R_ClassicSubviewDomain_Stats( void ) {
	return domain.stats;
}

int R_ClassicSubviewDomain_NumViews( void ) {
	return domain.viewCount;
}

const classicSubviewDomainView_t *R_ClassicSubviewDomain_ViewByIndex( int index ) {
	return index >= 0 && index < domain.viewCount ? &domain.views[index] : NULL;
}

const classicSubviewDomainView_t *R_ClassicSubviewDomain_FindView(
		const viewDef_t *viewDef ) {
	return FindMutableView(viewDef);
}

bool R_ClassicSubviewDomain_CaptureMatches(
		const classicSubviewDomainView_t &view, const idImage *image,
		int x, int y, int width, int height, int cubeFace, bool copyDepth ) {
	return view.ready && image == view.captureImage && x == view.captureX
		&& y == view.captureY && width == view.captureWidth
		&& height == view.captureHeight && cubeFace == view.captureCubeFace
		&& !copyDepth;
}

bool R_ClassicSubviewDomain_RecordOwned( const viewDef_t *viewDef,
		classicSubviewDomainBackend_t backend, const idImage *image,
		int x, int y, int width, int height, int cubeFace, bool copyDepth ) {
	if ( backend < CLASSIC_SUBVIEW_DOMAIN_BACKEND_GL
			|| backend >= CLASSIC_SUBVIEW_DOMAIN_BACKEND_COUNT ) {
		return false;
	}
	classicSubviewDomainView_t *view = FindMutableView(viewDef);
	if ( view == NULL || !view->ready ) {
		RecordFallback( view, backend, CLASSIC_SUBVIEW_DOMAIN_FAILURE_BACKEND_NOT_READY, 0 );
		return false;
	}
	if ( view->backendOutcome[backend] != CLASSIC_SUBVIEW_DOMAIN_BACKEND_UNRECORDED ) {
		domain.stats.backend[backend].duplicateReports++;
		return view->backendOutcome[backend] == CLASSIC_SUBVIEW_DOMAIN_BACKEND_OWNED
			&& R_ClassicSubviewDomain_CaptureMatches( *view, image, x, y, width,
				height, cubeFace, copyDepth );
	}
	if ( !R_ClassicSubviewDomain_CaptureMatches( *view, image, x, y, width,
			height, cubeFace, copyDepth ) ) {
		domain.stats.backend[backend].coverageMismatches++;
		RecordFallback( view, backend,
			CLASSIC_SUBVIEW_DOMAIN_FAILURE_BACKEND_CAPTURE_MISMATCH, 0 );
		return false;
	}
	const int index = ViewIndex(view);
	view->backendOutcome[backend] = CLASSIC_SUBVIEW_DOMAIN_BACKEND_OWNED;
	classicSubviewDomainBackendCoverage_t &coverage = domain.stats.backend[backend];
	if ( index >= 0 ) {
		coverage.ownedViewMask |= 1ull << index;
	}
	coverage.ownedViews++;
	if ( view->kind == CLASSIC_SUBVIEW_DOMAIN_KIND_REMOTE_CAMERA ) {
		coverage.ownedRemoteCameraViews++;
	} else if ( view->kind == CLASSIC_SUBVIEW_DOMAIN_KIND_REFRACTION ) {
		coverage.ownedRefractionViews++;
	}
	return true;
}

void R_ClassicSubviewDomain_RecordBackendFallback( const viewDef_t *viewDef,
		classicSubviewDomainBackend_t backend, classicSubviewDomainFailure_t failure,
		int detail ) {
	RecordFallback( FindMutableView(viewDef), backend, failure, detail );
}

const classicSubviewDomainBackendCoverage_t &
		R_ClassicSubviewDomain_BackendCoverage( classicSubviewDomainBackend_t backend ) {
	static const classicSubviewDomainBackendCoverage_t empty = {};
	return backend >= CLASSIC_SUBVIEW_DOMAIN_BACKEND_GL
		&& backend < CLASSIC_SUBVIEW_DOMAIN_BACKEND_COUNT
		? domain.stats.backend[backend] : empty;
}

const char *ClassicSubviewDomainKind_Name( classicSubviewDomainKind_t kind ) {
	switch ( kind ) {
	case CLASSIC_SUBVIEW_DOMAIN_KIND_REMOTE_CAMERA: return "remoteCamera";
	case CLASSIC_SUBVIEW_DOMAIN_KIND_REFRACTION: return "refraction";
	case CLASSIC_SUBVIEW_DOMAIN_KIND_NONE:
	case CLASSIC_SUBVIEW_DOMAIN_KIND_COUNT:
	default: return "none";
	}
}

const char *ClassicSubviewDomainFailure_Name( classicSubviewDomainFailure_t failure ) {
	switch ( failure ) {
	case CLASSIC_SUBVIEW_DOMAIN_FAILURE_NONE: return "none";
	case CLASSIC_SUBVIEW_DOMAIN_FAILURE_UNAVAILABLE: return "unavailable";
	case CLASSIC_SUBVIEW_DOMAIN_FAILURE_SCENE_PACKET_OVERFLOW: return "scenePacketOverflow";
	case CLASSIC_SUBVIEW_DOMAIN_FAILURE_CAPTURE_POOL_OVERFLOW: return "capturePoolOverflow";
	case CLASSIC_SUBVIEW_DOMAIN_FAILURE_UNSUPPORTED_VIEW: return "unsupportedView";
	case CLASSIC_SUBVIEW_DOMAIN_FAILURE_MISSING_PARENT_SCENE: return "missingParentScene";
	case CLASSIC_SUBVIEW_DOMAIN_FAILURE_MISSING_PARENT_SURFACE: return "missingParentSurface";
	case CLASSIC_SUBVIEW_DOMAIN_FAILURE_UNSUPPORTED_PARENT_MATERIAL: return "unsupportedParentMaterial";
	case CLASSIC_SUBVIEW_DOMAIN_FAILURE_MISSING_CAPTURE: return "missingCapture";
	case CLASSIC_SUBVIEW_DOMAIN_FAILURE_DUPLICATE_CAPTURE: return "duplicateCapture";
	case CLASSIC_SUBVIEW_DOMAIN_FAILURE_INVALID_CAPTURE: return "invalidCapture";
	case CLASSIC_SUBVIEW_DOMAIN_FAILURE_CAPTURE_SURFACE_MISMATCH: return "captureSurfaceMismatch";
	case CLASSIC_SUBVIEW_DOMAIN_FAILURE_CAPTURE_VIEWPORT_MISMATCH: return "captureViewportMismatch";
	case CLASSIC_SUBVIEW_DOMAIN_FAILURE_BACKEND_NOT_READY: return "backendNotReady";
	case CLASSIC_SUBVIEW_DOMAIN_FAILURE_BACKEND_REJECTED: return "backendRejected";
	case CLASSIC_SUBVIEW_DOMAIN_FAILURE_BACKEND_CAPTURE_MISMATCH: return "backendCaptureMismatch";
	case CLASSIC_SUBVIEW_DOMAIN_FAILURE_COUNT:
	default: return "unknown";
	}
}

const char *ClassicSubviewDomainBackend_Name( classicSubviewDomainBackend_t backend ) {
	switch ( backend ) {
	case CLASSIC_SUBVIEW_DOMAIN_BACKEND_GL: return "GL";
	case CLASSIC_SUBVIEW_DOMAIN_BACKEND_VULKAN: return "Vulkan";
	case CLASSIC_SUBVIEW_DOMAIN_BACKEND_COUNT:
	default: return "unknown";
	}
}

bool RendererClassicSubviewDomain_RunSelfTest( void ) {
	if ( CLASSIC_SUBVIEW_DOMAIN_MAX_VIEWS != SCENE_PACKET_MAX_SUBVIEW_CAPTURES
			|| idStr::Cmp( ClassicSubviewDomainKind_Name(
				CLASSIC_SUBVIEW_DOMAIN_KIND_REMOTE_CAMERA ), "remoteCamera" ) != 0
			|| idStr::Cmp( ClassicSubviewDomainFailure_Name(
				CLASSIC_SUBVIEW_DOMAIN_FAILURE_CAPTURE_VIEWPORT_MISMATCH ),
				"captureViewportMismatch" ) != 0
			|| idStr::Cmp( ClassicSubviewDomainBackend_Name(
				CLASSIC_SUBVIEW_DOMAIN_BACKEND_VULKAN ), "Vulkan" ) != 0 ) {
		return false;
	}
	R_ClassicSubviewDomain_ResetFrame();
	viewDef_t viewDef;
	std::memset( &viewDef, 0, sizeof(viewDef) );
	InitView( domain.views[0], &viewDef, 7 );
	domain.viewCount = 1;
	classicSubviewDomainView_t &view = domain.views[0];
	view.ready = true;
	view.kind = CLASSIC_SUBVIEW_DOMAIN_KIND_REMOTE_CAMERA;
	view.captureImage = reinterpret_cast<idImage *>( static_cast<size_t>(0x1000) );
	view.captureX = 3;
	view.captureY = 4;
	view.captureWidth = 64;
	view.captureHeight = 32;
	view.captureCubeFace = 0;
	const std::uint64_t firstHash = HashView(view);
	view.captureWidth++;
	const bool hashSensitive = HashView(view) != firstHash;
	view.captureWidth--;
	std::memset( &domain.stats, 0, sizeof(domain.stats) );
	const bool owned = R_ClassicSubviewDomain_RecordOwned( &viewDef,
		CLASSIC_SUBVIEW_DOMAIN_BACKEND_GL, view.captureImage, 3, 4, 64, 32,
		0, false );
	const bool duplicate = R_ClassicSubviewDomain_RecordOwned( &viewDef,
		CLASSIC_SUBVIEW_DOMAIN_BACKEND_GL, view.captureImage, 3, 4, 64, 32,
		0, false );
	const bool mismatch = !R_ClassicSubviewDomain_RecordOwned( &viewDef,
		CLASSIC_SUBVIEW_DOMAIN_BACKEND_VULKAN, view.captureImage, 3, 4, 63, 32,
		0, false );
	const bool passed = hashSensitive && owned && duplicate && mismatch
		&& view.backendOutcome[CLASSIC_SUBVIEW_DOMAIN_BACKEND_GL]
			== CLASSIC_SUBVIEW_DOMAIN_BACKEND_OWNED
		&& view.backendOutcome[CLASSIC_SUBVIEW_DOMAIN_BACKEND_VULKAN]
			== CLASSIC_SUBVIEW_DOMAIN_BACKEND_FALLBACK
		&& domain.stats.backend[CLASSIC_SUBVIEW_DOMAIN_BACKEND_GL].ownedViews == 1
		&& domain.stats.backend[CLASSIC_SUBVIEW_DOMAIN_BACKEND_GL].duplicateReports == 1
		&& domain.stats.backend[CLASSIC_SUBVIEW_DOMAIN_BACKEND_VULKAN].coverageMismatches == 1;
	R_ClassicSubviewDomain_ResetFrame();
	return passed;
}
