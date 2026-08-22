// Copyright (C) 2026 DarkMatter Productions
//

#ifndef __CLASSIC_SUBVIEW_DOMAIN_H__
#define __CLASSIC_SUBVIEW_DOMAIN_H__

#include "ScenePackets.h"

/*
===============================================================================

	Shared capture-backed subview transaction.

	A view is published only after its parent source surface, child scene packet,
	and exact RC_COPY_RENDER destination agree.  The current first corridor is
	the ordinary color capture used by remote-camera and refraction surfaces;
	mirrors, reflections, x-ray, cubemaps, and depth captures deliberately retain
	the established path until their clip/camera semantics have dedicated records.

===============================================================================
*/

const int CLASSIC_SUBVIEW_DOMAIN_MAX_VIEWS = SCENE_PACKET_MAX_SUBVIEW_CAPTURES;

enum classicSubviewDomainKind_t {
	CLASSIC_SUBVIEW_DOMAIN_KIND_NONE = 0,
	CLASSIC_SUBVIEW_DOMAIN_KIND_REMOTE_CAMERA,
	CLASSIC_SUBVIEW_DOMAIN_KIND_REFRACTION,
	CLASSIC_SUBVIEW_DOMAIN_KIND_COUNT
};

enum classicSubviewDomainFailure_t {
	CLASSIC_SUBVIEW_DOMAIN_FAILURE_NONE = 0,
	CLASSIC_SUBVIEW_DOMAIN_FAILURE_UNAVAILABLE,
	CLASSIC_SUBVIEW_DOMAIN_FAILURE_SCENE_PACKET_OVERFLOW,
	CLASSIC_SUBVIEW_DOMAIN_FAILURE_CAPTURE_POOL_OVERFLOW,
	CLASSIC_SUBVIEW_DOMAIN_FAILURE_UNSUPPORTED_VIEW,
	CLASSIC_SUBVIEW_DOMAIN_FAILURE_MISSING_PARENT_SCENE,
	CLASSIC_SUBVIEW_DOMAIN_FAILURE_MISSING_PARENT_SURFACE,
	CLASSIC_SUBVIEW_DOMAIN_FAILURE_UNSUPPORTED_PARENT_MATERIAL,
	CLASSIC_SUBVIEW_DOMAIN_FAILURE_MISSING_CAPTURE,
	CLASSIC_SUBVIEW_DOMAIN_FAILURE_DUPLICATE_CAPTURE,
	CLASSIC_SUBVIEW_DOMAIN_FAILURE_INVALID_CAPTURE,
	CLASSIC_SUBVIEW_DOMAIN_FAILURE_CAPTURE_SURFACE_MISMATCH,
	CLASSIC_SUBVIEW_DOMAIN_FAILURE_CAPTURE_VIEWPORT_MISMATCH,
	CLASSIC_SUBVIEW_DOMAIN_FAILURE_BACKEND_NOT_READY,
	CLASSIC_SUBVIEW_DOMAIN_FAILURE_BACKEND_REJECTED,
	CLASSIC_SUBVIEW_DOMAIN_FAILURE_BACKEND_CAPTURE_MISMATCH,
	CLASSIC_SUBVIEW_DOMAIN_FAILURE_COUNT
};

enum classicSubviewDomainBackend_t {
	CLASSIC_SUBVIEW_DOMAIN_BACKEND_GL = 0,
	CLASSIC_SUBVIEW_DOMAIN_BACKEND_VULKAN,
	CLASSIC_SUBVIEW_DOMAIN_BACKEND_COUNT
};

enum classicSubviewDomainBackendOutcome_t {
	CLASSIC_SUBVIEW_DOMAIN_BACKEND_UNRECORDED = 0,
	CLASSIC_SUBVIEW_DOMAIN_BACKEND_OWNED,
	CLASSIC_SUBVIEW_DOMAIN_BACKEND_FALLBACK
};

typedef struct classicSubviewDomainBackendCoverage_s {
	std::uint64_t	ownedViewMask;
	std::uint64_t	fallbackViewMask;
	int				ownedViews;
	int				fallbackViews;
	int				ownedRemoteCameraViews;
	int				ownedRefractionViews;
	int				coverageMismatches;
	int				duplicateReports;
	int				untrackedFallbacks;
} classicSubviewDomainBackendCoverage_t;

typedef struct classicSubviewDomainView_s {
	const viewDef_t			*viewDef;
	const viewDef_t			*parentViewDef;
	const drawSurf_t		*parentSurface;
	idImage					*captureImage;
	int					scenePacketIndex;
	int					parentScenePacketIndex;
	int					capturePacketIndex;
	int					firstPassPacket;
	int					passPacketCount;
	int					firstDrawPacket;
	int					drawPacketCount;
	int					captureX;
	int					captureY;
	int					captureWidth;
	int					captureHeight;
	int					captureCubeFace;
	classicSubviewDomainKind_t	kind;
	bool					ready;
	classicSubviewDomainFailure_t	failure;
	int					failureDetail;
	std::uint64_t			hash;
	classicSubviewDomainBackendOutcome_t backendOutcome[
		CLASSIC_SUBVIEW_DOMAIN_BACKEND_COUNT];
	classicSubviewDomainFailure_t backendFailure[
		CLASSIC_SUBVIEW_DOMAIN_BACKEND_COUNT];
	int					backendFailureDetail[
		CLASSIC_SUBVIEW_DOMAIN_BACKEND_COUNT];
} classicSubviewDomainView_t;

typedef struct classicSubviewDomainStats_s {
	bool					prepared;
	bool					frameValid;
	bool					overflow;
	int					sourceScenes;
	int					subviewScenes;
	int					capturePackets;
	int					readyViews;
	int					fallbackViews;
	int					remoteCameraViews;
	int					refractionViews;
	int					failureCounts[CLASSIC_SUBVIEW_DOMAIN_FAILURE_COUNT];
	std::uint64_t			hash;
	classicSubviewDomainBackendCoverage_t backend[
		CLASSIC_SUBVIEW_DOMAIN_BACKEND_COUNT];
	char					status[96];
} classicSubviewDomainStats_t;

void R_ClassicSubviewDomain_ResetFrame( void );
void R_ClassicSubviewDomain_PrepareFrame( const idScenePacketFrame &packetFrame );
const classicSubviewDomainStats_t &R_ClassicSubviewDomain_Stats( void );
int R_ClassicSubviewDomain_NumViews( void );
const classicSubviewDomainView_t *R_ClassicSubviewDomain_ViewByIndex( int index );
const classicSubviewDomainView_t *R_ClassicSubviewDomain_FindView(
	const viewDef_t *viewDef );
bool R_ClassicSubviewDomain_CaptureMatches(
	const classicSubviewDomainView_t &view, const idImage *image,
	int x, int y, int width, int height, int cubeFace, bool copyDepth );
bool R_ClassicSubviewDomain_RecordOwned( const viewDef_t *viewDef,
	classicSubviewDomainBackend_t backend, const idImage *image,
	int x, int y, int width, int height, int cubeFace, bool copyDepth );
void R_ClassicSubviewDomain_RecordBackendFallback( const viewDef_t *viewDef,
	classicSubviewDomainBackend_t backend, classicSubviewDomainFailure_t failure,
	int detail );
const classicSubviewDomainBackendCoverage_t &
	R_ClassicSubviewDomain_BackendCoverage( classicSubviewDomainBackend_t backend );
const char *ClassicSubviewDomainKind_Name( classicSubviewDomainKind_t kind );
const char *ClassicSubviewDomainFailure_Name( classicSubviewDomainFailure_t failure );
const char *ClassicSubviewDomainBackend_Name( classicSubviewDomainBackend_t backend );
bool RendererClassicSubviewDomain_RunSelfTest( void );

#endif /* !__CLASSIC_SUBVIEW_DOMAIN_H__ */
