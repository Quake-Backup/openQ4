// Copyright (C) 2004 Id Software, Inc.
//

#ifndef __LENS_FLARE_SETTINGS_H__
#define __LENS_FLARE_SETTINGS_H__

#include "RendererCaps.h"

static const int RENDERER_LENS_FLARE_SETTINGS_VERSION = 1;
static const int RENDERER_LENS_FLARE_MAX_SOURCES = 8;
static const int RENDERER_LENS_FLARE_MAX_GHOSTS = 3;
static const int RENDERER_LENS_FLARE_MAX_STREAKS = 1;

enum rendererLensFlareRejectReason_t {
	RENDERER_LENS_FLARE_REJECT_NONE = 0,
	RENDERER_LENS_FLARE_REJECT_DISABLED,
	RENDERER_LENS_FLARE_REJECT_SKIP_POST_PROCESS,
	RENDERER_LENS_FLARE_REJECT_NOT_MAIN_VIEW,
	RENDERER_LENS_FLARE_REJECT_NO_GLSL,
	RENDERER_LENS_FLARE_REJECT_NO_FBO,
	RENDERER_LENS_FLARE_REJECT_NO_TEXTURE_COORDS,
	RENDERER_LENS_FLARE_REJECT_NO_TEXTURE_IMAGE_UNITS,
	RENDERER_LENS_FLARE_REJECT_TEXTURE_TOO_LARGE,
	RENDERER_LENS_FLARE_REJECT_INVALID_VIEWPORT,
	RENDERER_LENS_FLARE_REJECT_SHADER_UNAVAILABLE,
	RENDERER_LENS_FLARE_REJECT_DEPTH_UNAVAILABLE,
	RENDERER_LENS_FLARE_REJECT_SCENE_UNAVAILABLE,
	RENDERER_LENS_FLARE_REJECT_ACCUM_UNAVAILABLE,
	RENDERER_LENS_FLARE_REJECT_COMPOSITE_UNAVAILABLE,
	RENDERER_LENS_FLARE_REJECT_NO_CANDIDATES,
	RENDERER_LENS_FLARE_REJECT_ORTHO_UNAVAILABLE
};

typedef struct rendererLensFlareSettings_s {
	int									version;
	int									requestedQuality;
	int									quality;
	int									maxSources;
	int									maxGhosts;
	int									maxStreaks;
	int									maxQuadsPerSource;
	bool								requested;
	bool								enabled;
	bool								depthAware;
	bool								coronaEnabled;
	bool								ghostChainEnabled;
	bool								streakEnabled;
	float								minEyeDistance;
	float								minSourceRadiusPixels;
	float								maxSourceOcclusionRadiusPixels;
	float								minCoronaRadiusPixels;
	float								maxCoronaRadiusPixels;
	float								peakChannelClamp;
	rendererLensFlareRejectReason_t		rejectReason;
} rendererLensFlareSettings_t;

const char *RendererLensFlareRejectReason_Name( rendererLensFlareRejectReason_t reason );
rendererLensFlareSettings_t RendererLensFlareSettings_Build( int requestedQuality, const renderBackendCaps_t &caps, bool skipPostProcess, bool mainView, int viewportWidth, int viewportHeight );
bool RendererLensFlareSettings_RunSelfTest( void );

#endif /* !__LENS_FLARE_SETTINGS_H__ */
