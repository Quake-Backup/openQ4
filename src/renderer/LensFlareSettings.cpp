// Copyright (C) 2004 Id Software, Inc.
//

#include "tr_local.h"
#include "LensFlareSettings.h"

static float RendererLensFlareSettings_ClampFinite( float value, float minValue, float maxValue, float fallback ) {
	if ( FLOAT_IS_NAN( value ) || FLOAT_IS_INF( value ) ) {
		value = fallback;
	}
	return idMath::ClampFloat( minValue, maxValue, value );
}

static rendererLensFlareSettings_t RendererLensFlareSettings_Default( void ) {
	rendererLensFlareSettings_t settings;
	memset( &settings, 0, sizeof( settings ) );
	settings.version = RENDERER_LENS_FLARE_SETTINGS_VERSION;
	settings.maxSources = RENDERER_LENS_FLARE_MAX_SOURCES;
	settings.maxGhosts = RENDERER_LENS_FLARE_MAX_GHOSTS;
	settings.maxStreaks = RENDERER_LENS_FLARE_MAX_STREAKS;
	settings.depthAware = true;
	settings.coronaEnabled = true;
	settings.minEyeDistance = RendererLensFlareSettings_ClampFinite( 24.0f, 1.0f, 4096.0f, 24.0f );
	settings.minSourceRadiusPixels = RendererLensFlareSettings_ClampFinite( 2.0f, 1.0f, 64.0f, 2.0f );
	settings.maxSourceOcclusionRadiusPixels = RendererLensFlareSettings_ClampFinite( 12.0f, 2.0f, 64.0f, 12.0f );
	settings.minCoronaRadiusPixels = RendererLensFlareSettings_ClampFinite( 14.0f, 1.0f, 256.0f, 14.0f );
	settings.maxCoronaRadiusPixels = RendererLensFlareSettings_ClampFinite( 96.0f, settings.minCoronaRadiusPixels, 512.0f, 96.0f );
	settings.peakChannelClamp = RendererLensFlareSettings_ClampFinite( 1.5f, 0.01f, 64.0f, 1.5f );
	settings.rejectReason = RENDERER_LENS_FLARE_REJECT_DISABLED;
	return settings;
}

const char *RendererLensFlareRejectReason_Name( rendererLensFlareRejectReason_t reason ) {
	switch ( reason ) {
	case RENDERER_LENS_FLARE_REJECT_NONE:
		return "none";
	case RENDERER_LENS_FLARE_REJECT_DISABLED:
		return "disabled";
	case RENDERER_LENS_FLARE_REJECT_SKIP_POST_PROCESS:
		return "skipPostProcess";
	case RENDERER_LENS_FLARE_REJECT_NOT_MAIN_VIEW:
		return "notMainView";
	case RENDERER_LENS_FLARE_REJECT_NO_GLSL:
		return "noGLSL";
	case RENDERER_LENS_FLARE_REJECT_NO_FBO:
		return "noFBO";
	case RENDERER_LENS_FLARE_REJECT_NO_TEXTURE_COORDS:
		return "noTextureCoords";
	case RENDERER_LENS_FLARE_REJECT_NO_TEXTURE_IMAGE_UNITS:
		return "noTextureImageUnits";
	case RENDERER_LENS_FLARE_REJECT_TEXTURE_TOO_LARGE:
		return "textureTooLarge";
	case RENDERER_LENS_FLARE_REJECT_INVALID_VIEWPORT:
		return "invalidViewport";
	case RENDERER_LENS_FLARE_REJECT_SHADER_UNAVAILABLE:
		return "shaderUnavailable";
	case RENDERER_LENS_FLARE_REJECT_DEPTH_UNAVAILABLE:
		return "depthUnavailable";
	case RENDERER_LENS_FLARE_REJECT_SCENE_UNAVAILABLE:
		return "sceneUnavailable";
	case RENDERER_LENS_FLARE_REJECT_ACCUM_UNAVAILABLE:
		return "accumUnavailable";
	case RENDERER_LENS_FLARE_REJECT_COMPOSITE_UNAVAILABLE:
		return "compositeUnavailable";
	case RENDERER_LENS_FLARE_REJECT_NO_CANDIDATES:
		return "noCandidates";
	case RENDERER_LENS_FLARE_REJECT_ORTHO_UNAVAILABLE:
		return "orthoUnavailable";
	default:
		return "unknown";
	}
}

rendererLensFlareSettings_t RendererLensFlareSettings_Build( int requestedQuality, const renderBackendCaps_t &caps, bool skipPostProcess, bool mainView, int viewportWidth, int viewportHeight ) {
	rendererLensFlareSettings_t settings = RendererLensFlareSettings_Default();
	settings.requestedQuality = idMath::ClampInt( 0, 2, requestedQuality );
	settings.quality = settings.requestedQuality;
	settings.requested = settings.requestedQuality > 0 && !skipPostProcess;

	if ( settings.requestedQuality <= 0 ) {
		settings.rejectReason = RENDERER_LENS_FLARE_REJECT_DISABLED;
		return settings;
	}
	if ( skipPostProcess ) {
		settings.rejectReason = RENDERER_LENS_FLARE_REJECT_SKIP_POST_PROCESS;
		return settings;
	}
	if ( !mainView ) {
		settings.rejectReason = RENDERER_LENS_FLARE_REJECT_NOT_MAIN_VIEW;
		return settings;
	}
	if ( !caps.hasGLSL ) {
		settings.rejectReason = RENDERER_LENS_FLARE_REJECT_NO_GLSL;
		return settings;
	}
	if ( !caps.hasFBO ) {
		settings.rejectReason = RENDERER_LENS_FLARE_REJECT_NO_FBO;
		return settings;
	}
	if ( caps.maxTextureCoords < 2 ) {
		settings.rejectReason = RENDERER_LENS_FLARE_REJECT_NO_TEXTURE_COORDS;
		return settings;
	}
	if ( caps.maxTextureImageUnits < 2 ) {
		settings.rejectReason = RENDERER_LENS_FLARE_REJECT_NO_TEXTURE_IMAGE_UNITS;
		return settings;
	}
	if ( viewportWidth <= 0 || viewportHeight <= 0 ) {
		settings.rejectReason = RENDERER_LENS_FLARE_REJECT_INVALID_VIEWPORT;
		return settings;
	}
	if ( caps.maxTextureSize > 0 && ( viewportWidth > caps.maxTextureSize || viewportHeight > caps.maxTextureSize ) ) {
		settings.rejectReason = RENDERER_LENS_FLARE_REJECT_TEXTURE_TOO_LARGE;
		return settings;
	}

	settings.enabled = true;
	settings.rejectReason = RENDERER_LENS_FLARE_REJECT_NONE;
	settings.ghostChainEnabled = settings.quality >= 2;
	settings.streakEnabled = settings.quality >= 2;
	settings.maxGhosts = settings.ghostChainEnabled ? RENDERER_LENS_FLARE_MAX_GHOSTS : 0;
	settings.maxStreaks = settings.streakEnabled ? RENDERER_LENS_FLARE_MAX_STREAKS : 0;
	settings.maxQuadsPerSource = 2 + settings.maxGhosts + settings.maxStreaks;
	return settings;
}

static renderBackendCaps_t RendererLensFlareSettings_TestCaps( void ) {
	renderBackendCaps_t caps;
	memset( &caps, 0, sizeof( caps ) );
	caps.contextCreated = true;
	caps.hasGLSL = true;
	caps.hasFBO = true;
	caps.maxTextureSize = 4096;
	caps.maxTextureCoords = 2;
	caps.maxTextureImageUnits = 2;
	return caps;
}

static bool RendererLensFlareSettings_Expect( const char *name, const rendererLensFlareSettings_t &settings, bool enabled, rendererLensFlareRejectReason_t reason, int quality, int maxGhosts, int maxQuadsPerSource ) {
	if ( settings.version != RENDERER_LENS_FLARE_SETTINGS_VERSION
		|| settings.enabled != enabled
		|| settings.rejectReason != reason
		|| settings.quality != quality
		|| settings.maxSources != RENDERER_LENS_FLARE_MAX_SOURCES
		|| settings.maxGhosts != maxGhosts
		|| settings.maxQuadsPerSource != maxQuadsPerSource
		|| FLOAT_IS_NAN( settings.minEyeDistance ) || FLOAT_IS_INF( settings.minEyeDistance )
		|| FLOAT_IS_NAN( settings.minSourceRadiusPixels ) || FLOAT_IS_INF( settings.minSourceRadiusPixels )
		|| FLOAT_IS_NAN( settings.maxSourceOcclusionRadiusPixels ) || FLOAT_IS_INF( settings.maxSourceOcclusionRadiusPixels )
		|| FLOAT_IS_NAN( settings.minCoronaRadiusPixels ) || FLOAT_IS_INF( settings.minCoronaRadiusPixels )
		|| FLOAT_IS_NAN( settings.maxCoronaRadiusPixels ) || FLOAT_IS_INF( settings.maxCoronaRadiusPixels )
		|| FLOAT_IS_NAN( settings.peakChannelClamp ) || FLOAT_IS_INF( settings.peakChannelClamp ) ) {
		common->Printf(
			"RendererLensFlareSettings self-test failed: %s enabled=%d reason=%s quality=%d maxSources=%d maxGhosts=%d maxQuads=%d\n",
			name,
			settings.enabled ? 1 : 0,
			RendererLensFlareRejectReason_Name( settings.rejectReason ),
			settings.quality,
			settings.maxSources,
			settings.maxGhosts,
			settings.maxQuadsPerSource );
		return false;
	}
	return true;
}

bool RendererLensFlareSettings_RunSelfTest( void ) {
	renderBackendCaps_t caps = RendererLensFlareSettings_TestCaps();
	if ( !RendererLensFlareSettings_Expect( "disabled cvar", RendererLensFlareSettings_Build( 0, caps, false, true, 1280, 720 ), false, RENDERER_LENS_FLARE_REJECT_DISABLED, 0, RENDERER_LENS_FLARE_MAX_GHOSTS, 0 ) ) {
		return false;
	}
	if ( !RendererLensFlareSettings_Expect( "corona tier", RendererLensFlareSettings_Build( 1, caps, false, true, 1280, 720 ), true, RENDERER_LENS_FLARE_REJECT_NONE, 1, 0, 2 ) ) {
		return false;
	}
	if ( !RendererLensFlareSettings_Expect( "high tier clamps", RendererLensFlareSettings_Build( 99, caps, false, true, 1280, 720 ), true, RENDERER_LENS_FLARE_REJECT_NONE, 2, RENDERER_LENS_FLARE_MAX_GHOSTS, 2 + RENDERER_LENS_FLARE_MAX_GHOSTS + RENDERER_LENS_FLARE_MAX_STREAKS ) ) {
		return false;
	}
	if ( !RendererLensFlareSettings_Expect( "post skipped", RendererLensFlareSettings_Build( 2, caps, true, true, 1280, 720 ), false, RENDERER_LENS_FLARE_REJECT_SKIP_POST_PROCESS, 2, RENDERER_LENS_FLARE_MAX_GHOSTS, 0 ) ) {
		return false;
	}
	if ( !RendererLensFlareSettings_Expect( "not main view", RendererLensFlareSettings_Build( 2, caps, false, false, 1280, 720 ), false, RENDERER_LENS_FLARE_REJECT_NOT_MAIN_VIEW, 2, RENDERER_LENS_FLARE_MAX_GHOSTS, 0 ) ) {
		return false;
	}

	caps = RendererLensFlareSettings_TestCaps();
	caps.hasGLSL = false;
	if ( !RendererLensFlareSettings_Expect( "missing glsl", RendererLensFlareSettings_Build( 2, caps, false, true, 1280, 720 ), false, RENDERER_LENS_FLARE_REJECT_NO_GLSL, 2, RENDERER_LENS_FLARE_MAX_GHOSTS, 0 ) ) {
		return false;
	}

	caps = RendererLensFlareSettings_TestCaps();
	caps.hasFBO = false;
	if ( !RendererLensFlareSettings_Expect( "missing fbo", RendererLensFlareSettings_Build( 2, caps, false, true, 1280, 720 ), false, RENDERER_LENS_FLARE_REJECT_NO_FBO, 2, RENDERER_LENS_FLARE_MAX_GHOSTS, 0 ) ) {
		return false;
	}

	caps = RendererLensFlareSettings_TestCaps();
	caps.maxTextureCoords = 1;
	if ( !RendererLensFlareSettings_Expect( "missing texcoords", RendererLensFlareSettings_Build( 2, caps, false, true, 1280, 720 ), false, RENDERER_LENS_FLARE_REJECT_NO_TEXTURE_COORDS, 2, RENDERER_LENS_FLARE_MAX_GHOSTS, 0 ) ) {
		return false;
	}

	caps = RendererLensFlareSettings_TestCaps();
	caps.maxTextureImageUnits = 1;
	if ( !RendererLensFlareSettings_Expect( "missing texture image units", RendererLensFlareSettings_Build( 2, caps, false, true, 1280, 720 ), false, RENDERER_LENS_FLARE_REJECT_NO_TEXTURE_IMAGE_UNITS, 2, RENDERER_LENS_FLARE_MAX_GHOSTS, 0 ) ) {
		return false;
	}

	caps = RendererLensFlareSettings_TestCaps();
	caps.maxTextureSize = 256;
	if ( !RendererLensFlareSettings_Expect( "viewport too large", RendererLensFlareSettings_Build( 2, caps, false, true, 1280, 720 ), false, RENDERER_LENS_FLARE_REJECT_TEXTURE_TOO_LARGE, 2, RENDERER_LENS_FLARE_MAX_GHOSTS, 0 ) ) {
		return false;
	}

	common->Printf( "RendererLensFlareSettings self-test passed (settings contract, 10 cases)\n" );
	return true;
}
