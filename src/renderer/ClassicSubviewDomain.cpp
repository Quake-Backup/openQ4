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

static void HashFloat( std::uint64_t &hash, float value ) {
	std::uint32_t bits = 0;
	std::memcpy( &bits, &value, sizeof( bits ) );
	HashU32( hash, bits );
}

static bool SameFloat( float a, float b ) {
	std::uint32_t aBits = 0;
	std::uint32_t bBits = 0;
	std::memcpy( &aBits, &a, sizeof( aBits ) );
	std::memcpy( &bBits, &b, sizeof( bBits ) );
	return aBits == bBits;
}

static void HashVec3( std::uint64_t &hash, const idVec3 &value ) {
	for ( int component = 0; component < 3; ++component ) {
		HashFloat( hash, value[component] );
	}
}

static bool SameVec3( const idVec3 &a, const idVec3 &b ) {
	for ( int component = 0; component < 3; ++component ) {
		if ( !SameFloat( a[component], b[component] ) ) {
			return false;
		}
	}
	return true;
}

static void HashPlane( std::uint64_t &hash, const idPlane &value ) {
	for ( int component = 0; component < 4; ++component ) {
		HashFloat( hash, value[component] );
	}
}

static bool SamePlane( const idPlane &a, const idPlane &b ) {
	for ( int component = 0; component < 4; ++component ) {
		if ( !SameFloat( a[component], b[component] ) ) {
			return false;
		}
	}
	return true;
}

static void HashScreenRect( std::uint64_t &hash, const idScreenRect &value ) {
	HashInt( hash, value.x1 );
	HashInt( hash, value.y1 );
	HashInt( hash, value.x2 );
	HashInt( hash, value.y2 );
}

static bool SameScreenRect( const idScreenRect &a, const idScreenRect &b ) {
	return a.x1 == b.x1 && a.y1 == b.y1 && a.x2 == b.x2 && a.y2 == b.y2;
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

static void CaptureViewSemantics( classicSubviewDomainView_t &view,
		const viewDef_t &viewDef ) {
	view.semanticViewID = viewDef.renderView.viewID;
	view.semanticRenderTime = viewDef.renderView.time;
	view.semanticFloatTime = viewDef.floatTime;
	view.semanticViewOrigin = viewDef.renderView.vieworg;
	view.semanticViewAxis = viewDef.renderView.viewaxis;
	view.semanticInitialViewAreaOrigin = viewDef.initialViewAreaOrigin;
	view.semanticViewport = viewDef.viewport;
	view.semanticScissor = viewDef.scissor;
	view.semanticClipPlaneCount = viewDef.numClipPlanes;
	view.semanticIsMirror = viewDef.isMirror;
	view.semanticIsXraySubview = viewDef.isXraySubview;
	for ( int plane = 0; plane < MAX_CLIP_PLANES; ++plane ) {
		view.semanticClipPlanes[plane] = viewDef.clipPlanes[plane];
	}
}

static bool ViewSemanticsMatch( const classicSubviewDomainView_t &view,
		const viewDef_t &viewDef ) {
	if ( view.semanticViewID != viewDef.renderView.viewID
			|| view.semanticRenderTime != viewDef.renderView.time
			|| !SameFloat( view.semanticFloatTime, viewDef.floatTime )
			|| !SameVec3( view.semanticViewOrigin, viewDef.renderView.vieworg )
			|| !SameVec3( view.semanticInitialViewAreaOrigin,
				viewDef.initialViewAreaOrigin )
			|| !SameScreenRect( view.semanticViewport, viewDef.viewport )
			|| !SameScreenRect( view.semanticScissor, viewDef.scissor )
			|| view.semanticClipPlaneCount != viewDef.numClipPlanes
			|| view.semanticIsMirror != viewDef.isMirror
			|| view.semanticIsXraySubview != viewDef.isXraySubview ) {
		return false;
	}
	for ( int axis = 0; axis < 3; ++axis ) {
		if ( !SameVec3( view.semanticViewAxis[axis],
				viewDef.renderView.viewaxis[axis] ) ) {
			return false;
		}
	}
	for ( int plane = 0; plane < MAX_CLIP_PLANES; ++plane ) {
		if ( !SamePlane( view.semanticClipPlanes[plane], viewDef.clipPlanes[plane] ) ) {
			return false;
		}
	}
	return true;
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

static bool FindDirectKind( const drawSurf_t *surface,
		classicSubviewDomainKind_t &kind ) {
	kind = CLASSIC_SUBVIEW_DOMAIN_KIND_NONE;
	if ( surface == NULL || surface->material == NULL
			|| !surface->material->HasSubview()
			|| surface->material->GetSort() != SS_SUBVIEW ) {
		return false;
	}
	// SS_SUBVIEW is the classic direct mirror path: it publishes its child view
	// straight into the parent target and deliberately has no copy command.
	kind = CLASSIC_SUBVIEW_DOMAIN_KIND_DIRECT_MIRROR;
	return true;
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
		case DI_MIRROR_RENDER:
			stageKind = CLASSIC_SUBVIEW_DOMAIN_KIND_MIRROR;
			break;
		case DI_REFLECTION_RENDER:
			stageKind = CLASSIC_SUBVIEW_DOMAIN_KIND_REFLECTION;
			break;
		case DI_REFRACTION_RENDER:
			stageKind = CLASSIC_SUBVIEW_DOMAIN_KIND_REFRACTION;
			break;
		case DI_XRAY_RENDER:
			stageKind = CLASSIC_SUBVIEW_DOMAIN_KIND_XRAY;
			break;
		case DI_STATIC:
			continue;
		default:
			// Other dynamic images are not subview capture producers.
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

static bool HasSupportedSpecialSemantics( const viewDef_t &viewDef,
		classicSubviewDomainKind_t kind ) {
	if ( viewDef.numClipPlanes < 0 || viewDef.numClipPlanes > MAX_CLIP_PLANES ) {
		return false;
	}
	switch ( kind ) {
	case CLASSIC_SUBVIEW_DOMAIN_KIND_DIRECT_MIRROR:
	case CLASSIC_SUBVIEW_DOMAIN_KIND_MIRROR:
	case CLASSIC_SUBVIEW_DOMAIN_KIND_REFLECTION:
		// The final cull parity may be false for a nested mirror, so the
		// captured isMirror bit is authoritative rather than an eligibility
		// requirement. The single portal plane is the required camera boundary.
		return !viewDef.isXraySubview && viewDef.numClipPlanes == 1;
	case CLASSIC_SUBVIEW_DOMAIN_KIND_XRAY:
		return viewDef.isXraySubview && viewDef.numClipPlanes == 0;
	case CLASSIC_SUBVIEW_DOMAIN_KIND_REMOTE_CAMERA:
	case CLASSIC_SUBVIEW_DOMAIN_KIND_REFRACTION:
		return !viewDef.isMirror && !viewDef.isXraySubview
			&& viewDef.numClipPlanes == 0;
	case CLASSIC_SUBVIEW_DOMAIN_KIND_NONE:
	case CLASSIC_SUBVIEW_DOMAIN_KIND_COUNT:
	default:
		return false;
	}
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
	HashInt( hash, view.semanticViewID );
	HashInt( hash, view.semanticRenderTime );
	HashFloat( hash, view.semanticFloatTime );
	HashVec3( hash, view.semanticViewOrigin );
	for ( int axis = 0; axis < 3; ++axis ) {
		HashVec3( hash, view.semanticViewAxis[axis] );
	}
	HashVec3( hash, view.semanticInitialViewAreaOrigin );
	HashScreenRect( hash, view.semanticViewport );
	HashScreenRect( hash, view.semanticScissor );
	HashInt( hash, view.semanticClipPlaneCount );
	HashBool( hash, view.semanticIsMirror );
	HashBool( hash, view.semanticIsXraySubview );
	for ( int plane = 0; plane < MAX_CLIP_PLANES; ++plane ) {
		HashPlane( hash, view.semanticClipPlanes[plane] );
	}
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
			|| viewDef->renderWorld == NULL || viewDef->isEditor
			|| viewDef->renderView.viewID < 0
			|| viewDef->renderView.globalMaterial != NULL
			|| viewDef->superView == NULL || viewDef->subviewSurface == NULL ) {
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
	classicSubviewDomainKind_t directKind = CLASSIC_SUBVIEW_DOMAIN_KIND_NONE;
	const bool isDirect = FindDirectKind( view.parentSurface, directKind );
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
	if ( isDirect ) {
		if ( captureIndex >= 0 ) {
			return FailView( view,
				CLASSIC_SUBVIEW_DOMAIN_FAILURE_UNEXPECTED_CAPTURE, captureIndex );
		}
		view.kind = directKind;
	} else {
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
	}
	if ( !HasSupportedSpecialSemantics( *viewDef, view.kind ) ) {
		return FailView( view,
			CLASSIC_SUBVIEW_DOMAIN_FAILURE_UNSUPPORTED_SPECIAL_SEMANTICS,
			static_cast<int>( view.kind ) );
	}
	CaptureViewSemantics( view, *viewDef );
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
			switch ( view.kind ) {
			case CLASSIC_SUBVIEW_DOMAIN_KIND_DIRECT_MIRROR:
				domain.stats.directMirrorViews++;
				break;
			case CLASSIC_SUBVIEW_DOMAIN_KIND_REMOTE_CAMERA:
				domain.stats.remoteCameraViews++;
				break;
			case CLASSIC_SUBVIEW_DOMAIN_KIND_MIRROR:
				domain.stats.mirrorViews++;
				break;
			case CLASSIC_SUBVIEW_DOMAIN_KIND_REFLECTION:
				domain.stats.reflectionViews++;
				break;
			case CLASSIC_SUBVIEW_DOMAIN_KIND_REFRACTION:
				domain.stats.refractionViews++;
				break;
			case CLASSIC_SUBVIEW_DOMAIN_KIND_XRAY:
				domain.stats.xrayViews++;
				break;
			case CLASSIC_SUBVIEW_DOMAIN_KIND_NONE:
			case CLASSIC_SUBVIEW_DOMAIN_KIND_COUNT:
			default:
				break;
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

bool R_ClassicSubviewDomain_IsCaptureBacked(
		const classicSubviewDomainView_t &view ) {
	return view.kind == CLASSIC_SUBVIEW_DOMAIN_KIND_REMOTE_CAMERA
		|| view.kind == CLASSIC_SUBVIEW_DOMAIN_KIND_MIRROR
		|| view.kind == CLASSIC_SUBVIEW_DOMAIN_KIND_REFLECTION
		|| view.kind == CLASSIC_SUBVIEW_DOMAIN_KIND_REFRACTION
		|| view.kind == CLASSIC_SUBVIEW_DOMAIN_KIND_XRAY;
}

bool R_ClassicSubviewDomain_IsDirect( const classicSubviewDomainView_t &view ) {
	return view.kind == CLASSIC_SUBVIEW_DOMAIN_KIND_DIRECT_MIRROR;
}

bool R_ClassicSubviewDomain_ViewSemanticsMatch(
		const classicSubviewDomainView_t &view ) {
	return view.ready && view.viewDef != NULL
		&& ViewSemanticsMatch( view, *view.viewDef );
}

bool R_ClassicSubviewDomain_CaptureMatches(
		const classicSubviewDomainView_t &view, const idImage *image,
		int x, int y, int width, int height, int cubeFace, bool copyDepth ) {
	return R_ClassicSubviewDomain_IsCaptureBacked( view )
		&& R_ClassicSubviewDomain_ViewSemanticsMatch( view )
		&& image == view.captureImage && x == view.captureX
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
	if ( !R_ClassicSubviewDomain_ViewSemanticsMatch( *view ) ) {
		domain.stats.backend[backend].coverageMismatches++;
		RecordFallback( view, backend,
			CLASSIC_SUBVIEW_DOMAIN_FAILURE_VIEW_SEMANTICS_MISMATCH, 0 );
		return false;
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
	} else if ( view->kind == CLASSIC_SUBVIEW_DOMAIN_KIND_MIRROR ) {
		coverage.ownedMirrorViews++;
	} else if ( view->kind == CLASSIC_SUBVIEW_DOMAIN_KIND_REFLECTION ) {
		coverage.ownedReflectionViews++;
	} else if ( view->kind == CLASSIC_SUBVIEW_DOMAIN_KIND_REFRACTION ) {
		coverage.ownedRefractionViews++;
	} else if ( view->kind == CLASSIC_SUBVIEW_DOMAIN_KIND_XRAY ) {
		coverage.ownedXrayViews++;
	}
	return true;
}

bool R_ClassicSubviewDomain_RecordDirectOwned( const viewDef_t *viewDef,
		classicSubviewDomainBackend_t backend ) {
	if ( backend < CLASSIC_SUBVIEW_DOMAIN_BACKEND_GL
			|| backend >= CLASSIC_SUBVIEW_DOMAIN_BACKEND_COUNT ) {
		return false;
	}
	classicSubviewDomainView_t *view = FindMutableView(viewDef);
	if ( view == NULL || !view->ready || !R_ClassicSubviewDomain_IsDirect( *view ) ) {
		RecordFallback( view, backend, CLASSIC_SUBVIEW_DOMAIN_FAILURE_BACKEND_NOT_READY, 0 );
		return false;
	}
	if ( view->backendOutcome[backend] != CLASSIC_SUBVIEW_DOMAIN_BACKEND_UNRECORDED ) {
		domain.stats.backend[backend].duplicateReports++;
		return view->backendOutcome[backend] == CLASSIC_SUBVIEW_DOMAIN_BACKEND_OWNED
			&& R_ClassicSubviewDomain_ViewSemanticsMatch( *view );
	}
	if ( !R_ClassicSubviewDomain_ViewSemanticsMatch( *view ) ) {
		domain.stats.backend[backend].coverageMismatches++;
		RecordFallback( view, backend,
			CLASSIC_SUBVIEW_DOMAIN_FAILURE_VIEW_SEMANTICS_MISMATCH, 0 );
		return false;
	}
	const int index = ViewIndex(view);
	view->backendOutcome[backend] = CLASSIC_SUBVIEW_DOMAIN_BACKEND_OWNED;
	classicSubviewDomainBackendCoverage_t &coverage = domain.stats.backend[backend];
	if ( index >= 0 ) {
		coverage.ownedViewMask |= 1ull << index;
	}
	coverage.ownedViews++;
	coverage.ownedDirectMirrorViews++;
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
	case CLASSIC_SUBVIEW_DOMAIN_KIND_DIRECT_MIRROR: return "directMirror";
	case CLASSIC_SUBVIEW_DOMAIN_KIND_REMOTE_CAMERA: return "remoteCamera";
	case CLASSIC_SUBVIEW_DOMAIN_KIND_MIRROR: return "mirror";
	case CLASSIC_SUBVIEW_DOMAIN_KIND_REFLECTION: return "reflection";
	case CLASSIC_SUBVIEW_DOMAIN_KIND_REFRACTION: return "refraction";
	case CLASSIC_SUBVIEW_DOMAIN_KIND_XRAY: return "xray";
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
	case CLASSIC_SUBVIEW_DOMAIN_FAILURE_UNEXPECTED_CAPTURE: return "unexpectedCapture";
	case CLASSIC_SUBVIEW_DOMAIN_FAILURE_UNSUPPORTED_SPECIAL_SEMANTICS: return "unsupportedSpecialSemantics";
	case CLASSIC_SUBVIEW_DOMAIN_FAILURE_VIEW_SEMANTICS_MISMATCH: return "viewSemanticsMismatch";
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
			|| idStr::Cmp( ClassicSubviewDomainKind_Name(
				CLASSIC_SUBVIEW_DOMAIN_KIND_DIRECT_MIRROR ), "directMirror" ) != 0
			|| idStr::Cmp( ClassicSubviewDomainFailure_Name(
				CLASSIC_SUBVIEW_DOMAIN_FAILURE_VIEW_SEMANTICS_MISMATCH ),
				"viewSemanticsMismatch" ) != 0
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
	const bool capturePassed = hashSensitive && owned && duplicate && mismatch
		&& view.backendOutcome[CLASSIC_SUBVIEW_DOMAIN_BACKEND_GL]
			== CLASSIC_SUBVIEW_DOMAIN_BACKEND_OWNED
		&& view.backendOutcome[CLASSIC_SUBVIEW_DOMAIN_BACKEND_VULKAN]
			== CLASSIC_SUBVIEW_DOMAIN_BACKEND_FALLBACK
		&& domain.stats.backend[CLASSIC_SUBVIEW_DOMAIN_BACKEND_GL].ownedViews == 1
		&& domain.stats.backend[CLASSIC_SUBVIEW_DOMAIN_BACKEND_GL].duplicateReports == 1
		&& domain.stats.backend[CLASSIC_SUBVIEW_DOMAIN_BACKEND_VULKAN].coverageMismatches == 1;
	R_ClassicSubviewDomain_ResetFrame();
	std::memset( &viewDef, 0, sizeof(viewDef) );
	viewDef.isSubview = true;
	viewDef.numClipPlanes = 1;
	InitView( domain.views[0], &viewDef, 11 );
	domain.viewCount = 1;
	classicSubviewDomainView_t &directView = domain.views[0];
	directView.ready = true;
	directView.kind = CLASSIC_SUBVIEW_DOMAIN_KIND_DIRECT_MIRROR;
	CaptureViewSemantics( directView, viewDef );
	const bool directOwned = R_ClassicSubviewDomain_RecordDirectOwned( &viewDef,
		CLASSIC_SUBVIEW_DOMAIN_BACKEND_GL );
	viewDef.scissor.x1++;
	const bool semanticMismatch = !R_ClassicSubviewDomain_ViewSemanticsMatch(
		directView );
	const bool passed = capturePassed && directOwned && semanticMismatch;
	R_ClassicSubviewDomain_ResetFrame();
	return passed;
}
