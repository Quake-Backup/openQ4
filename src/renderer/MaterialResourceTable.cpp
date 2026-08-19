// Copyright (C) 2004 Id Software, Inc.
//

#include "tr_local.h"
#include "MaterialResourceTable.h"
#include "GLDebugScope.h"

// open-addressed material-pointer hash so per-draw record lookups stay O(1);
// entries store record index + 1 with 0 meaning empty so a memset clears it
const int MATERIAL_RESOURCE_TABLE_HASH_SIZE = MATERIAL_RESOURCE_TABLE_MAX_RECORDS * 2;

typedef struct materialResourceTableState_s {
	materialResourceTableRecord_t	records[MATERIAL_RESOURCE_TABLE_MAX_RECORDS];
	materialResourceTableStats_t		stats;
	renderBackendCaps_t				caps;
	renderFeatureSet_t				features;
	int								maxClassicTextureUnits;
	unsigned int					textureArrayTable[MATERIAL_RESOURCE_TABLE_TEXTURE_ARRAY_CAPACITY];
	int								textureArrayTableCount;
	short							materialHash[MATERIAL_RESOURCE_TABLE_HASH_SIZE];
} materialResourceTableState_t;

static materialResourceTableState_t rg_materialResourceTable;

static int R_MaterialResourceTable_HashMaterial( const idMaterial *material ) {
	size_t key = reinterpret_cast<size_t>( material ) >> 4;
	key *= 2654435761u;
	return static_cast<int>( key & ( MATERIAL_RESOURCE_TABLE_HASH_SIZE - 1 ) );
}

static void R_MaterialResourceTable_HashInsert( const idMaterial *material, int recordIndex ) {
	if ( material == NULL ) {
		return;
	}
	int slot = R_MaterialResourceTable_HashMaterial( material );
	for ( int probe = 0; probe < MATERIAL_RESOURCE_TABLE_HASH_SIZE; ++probe ) {
		short &entry = rg_materialResourceTable.materialHash[slot];
		if ( entry == 0 ) {
			entry = static_cast<short>( recordIndex + 1 );
			return;
		}
		if ( rg_materialResourceTable.records[entry - 1].material == material ) {
			// the first record added for a material keeps lookup priority
			return;
		}
		slot = ( slot + 1 ) & ( MATERIAL_RESOURCE_TABLE_HASH_SIZE - 1 );
	}
}

const char *MaterialResourceBlendMode_Name( materialResourceBlendMode_t blendMode ) {
	switch ( blendMode ) {
	case MATERIAL_RESOURCE_BLEND_OPAQUE:
		return "opaque";
	case MATERIAL_RESOURCE_BLEND_ALPHA_TEST:
		return "alphaTest";
	case MATERIAL_RESOURCE_BLEND_BLEND:
		return "blend";
	case MATERIAL_RESOURCE_BLEND_ADD:
		return "add";
	case MATERIAL_RESOURCE_BLEND_FILTER:
		return "filter";
	case MATERIAL_RESOURCE_BLEND_GUI:
		return "gui";
	case MATERIAL_RESOURCE_BLEND_POST_PROCESS:
		return "postProcess";
	default:
		return "unknown";
	}
}

const char *MaterialResourceTextureSemantic_Name( materialResourceTextureSemantic_t semantic ) {
	switch ( semantic ) {
	case MATERIAL_RESOURCE_TEXTURE_BUMP:
		return "bump";
	case MATERIAL_RESOURCE_TEXTURE_DIFFUSE:
		return "diffuse";
	case MATERIAL_RESOURCE_TEXTURE_SPECULAR:
		return "specular";
	case MATERIAL_RESOURCE_TEXTURE_EMISSIVE:
		return "emissive";
	case MATERIAL_RESOURCE_TEXTURE_GUI:
		return "gui";
	case MATERIAL_RESOURCE_TEXTURE_POST_PROCESS:
		return "post";
	case MATERIAL_RESOURCE_TEXTURE_ALBEDO:
		return "pbrAlbedo";
	case MATERIAL_RESOURCE_TEXTURE_NORMAL:
		return "pbrNormal";
	case MATERIAL_RESOURCE_TEXTURE_ORM:
		return "pbrORM";
	case MATERIAL_RESOURCE_TEXTURE_METALLIC:
		return "pbrMetallic";
	case MATERIAL_RESOURCE_TEXTURE_ROUGHNESS:
		return "pbrRoughness";
	case MATERIAL_RESOURCE_TEXTURE_AO:
		return "pbrAO";
	case MATERIAL_RESOURCE_TEXTURE_EMISSIVE_PBR:
		return "pbrEmissive";
	case MATERIAL_RESOURCE_TEXTURE_NONE:
	default:
		return "none";
	}
}

const char *MaterialResourcePBRFallbackReason_Name( materialResourcePBRFallbackReason_t reason ) {
	switch ( reason ) {
	case MATERIAL_RESOURCE_PBR_FALLBACK_NONE:
		return "none";
	case MATERIAL_RESOURCE_PBR_FALLBACK_DISABLED:
		return "disabled";
	case MATERIAL_RESOURCE_PBR_FALLBACK_UNSUPPORTED_WORKFLOW:
		return "unsupportedWorkflow";
	case MATERIAL_RESOURCE_PBR_FALLBACK_UNSUPPORTED_MATERIAL_CLASS:
		return "unsupportedMaterialClass";
	case MATERIAL_RESOURCE_PBR_FALLBACK_MISSING_ALBEDO:
		return "missingAlbedo";
	case MATERIAL_RESOURCE_PBR_FALLBACK_MISSING_NORMAL_FORMAT:
		return "missingNormalFormat";
	case MATERIAL_RESOURCE_PBR_FALLBACK_CONFLICTING_LAYOUT:
		return "conflictingLayout";
	case MATERIAL_RESOURCE_PBR_FALLBACK_MISSING_IMAGE:
		return "missingImage";
	case MATERIAL_RESOURCE_PBR_FALLBACK_CLASSIC_FEATURE:
		return "classicFeature";
	case MATERIAL_RESOURCE_PBR_FALLBACK_TOO_MANY_TEXTURES:
		return "tooManyTextures";
	case MATERIAL_RESOURCE_PBR_FALLBACK_SHADER_PATH_UNAVAILABLE:
		return "shaderPathUnavailable";
	default:
		return "unknown";
	}
}

unsigned int MaterialResourceTextureSemantic_Bit( materialResourceTextureSemantic_t semantic ) {
	if ( semantic <= MATERIAL_RESOURCE_TEXTURE_NONE || semantic >= MATERIAL_RESOURCE_TEXTURE_COUNT ) {
		return 0;
	}
	return 1u << static_cast<unsigned int>( semantic );
}

const char *MaterialResourceFallbackReason_Name( materialResourceFallbackReason_t reason ) {
	switch ( reason ) {
	case MATERIAL_RESOURCE_FALLBACK_NONE:
		return "none";
	case MATERIAL_RESOURCE_FALLBACK_MISSING_MATERIAL:
		return "missingMaterial";
	case MATERIAL_RESOURCE_FALLBACK_NO_DRAW_STAGES:
		return "noDrawStages";
	case MATERIAL_RESOURCE_FALLBACK_MISSING_IMAGE:
		return "missingImage";
	case MATERIAL_RESOURCE_FALLBACK_CUSTOM_PROGRAM:
		return "customProgram";
	case MATERIAL_RESOURCE_FALLBACK_CUSTOM_GLSL:
		return "customGLSL";
	case MATERIAL_RESOURCE_FALLBACK_DYNAMIC_IMAGE:
		return "dynamicImage";
	case MATERIAL_RESOURCE_FALLBACK_CURRENT_RENDER_IMAGE:
		return "currentRenderImage";
	case MATERIAL_RESOURCE_FALLBACK_SCREEN_TEXGEN:
		return "screenTexgen";
	case MATERIAL_RESOURCE_FALLBACK_SKY_TEXGEN:
		return "skyTexgen";
	case MATERIAL_RESOURCE_FALLBACK_UNSUPPORTED_TEXGEN:
		return "unsupportedTexgen";
	case MATERIAL_RESOURCE_FALLBACK_NEEDS_CURRENT_RENDER:
		return "needsCurrentRender";
	case MATERIAL_RESOURCE_FALLBACK_STAGE_CONDITION:
		return "stageCondition";
	case MATERIAL_RESOURCE_FALLBACK_STAGE_COLOR:
		return "stageColor";
	case MATERIAL_RESOURCE_FALLBACK_TEXTURE_MATRIX:
		return "textureMatrix";
	case MATERIAL_RESOURCE_FALLBACK_VERTEX_COLOR:
		return "vertexColor";
	case MATERIAL_RESOURCE_FALLBACK_POLYGON_OFFSET:
		return "polygonOffset";
	case MATERIAL_RESOURCE_FALLBACK_TOO_MANY_TEXTURES:
		return "tooManyTextures";
	default:
		return "unknown";
	}
}

static const char *R_MaterialResourceTable_SortGroupName( materialResourceSortGroup_t sortGroup ) {
	switch ( sortGroup ) {
	case MATERIAL_RESOURCE_SORT_SUBVIEW:
		return "subview";
	case MATERIAL_RESOURCE_SORT_GUI:
		return "gui";
	case MATERIAL_RESOURCE_SORT_OPAQUE:
		return "opaque";
	case MATERIAL_RESOURCE_SORT_DECAL:
		return "decal";
	case MATERIAL_RESOURCE_SORT_TRANSLUCENT:
		return "translucent";
	case MATERIAL_RESOURCE_SORT_POST_PROCESS:
		return "postProcess";
	case MATERIAL_RESOURCE_SORT_UNKNOWN:
	default:
		return "unknown";
	}
}

static void R_MaterialResourceTable_SetStatus( const char *status ) {
	idStr::Copynz( rg_materialResourceTable.stats.lastFailure, status ? status : "unknown", sizeof( rg_materialResourceTable.stats.lastFailure ) );
}

static void R_MaterialResourceTable_RecordDebugStringTruncation( const char *source ) {
	rg_materialResourceTable.stats.debugStringTruncations++;
	if ( rg_materialResourceTable.stats.debugStringTruncationSource[0] == '\0' ) {
		idStr::Copynz( rg_materialResourceTable.stats.debugStringTruncationSource, source ? source : "unknown", sizeof( rg_materialResourceTable.stats.debugStringTruncationSource ) );
	}
}

static bool R_MaterialResourceTable_CopyDebugString( char *dest, int destSize, const char *source ) {
	const char *text = source != NULL ? source : "";
	const int sourceLength = static_cast<int>( strlen( text ) );
	idStr::Copynz( dest, text, destSize );
	return sourceLength < destSize;
}

static bool R_MaterialResourceTable_FormatDebugString( char *dest, int destSize, const char *fmt, ... ) {
	va_list argptr;
	va_start( argptr, fmt );
	const int result = idStr::vsnPrintf( dest, destSize, fmt, argptr );
	va_end( argptr );
	return result >= 0;
}

static void R_MaterialResourceTable_AddFallback( materialResourceTableRecord_t &record, materialResourceFallbackReason_t reason, unsigned int flag ) {
	record.fallbackFlags |= flag;
	if ( record.fallbackReason == MATERIAL_RESOURCE_FALLBACK_NONE ) {
		record.fallbackReason = reason;
	}
}

static void R_MaterialResourceTable_AddPBRFallback( materialResourceTableRecord_t &record, materialResourcePBRFallbackReason_t reason ) {
	if ( record.pbrFallbackReason == MATERIAL_RESOURCE_PBR_FALLBACK_NONE ) {
		record.pbrFallbackReason = reason;
	}
}

static bool R_MaterialResourceTable_ImageIsPostProcess( const idImage *image ) {
	if ( image == NULL ) {
		return false;
	}
	const char *name = image->GetName();
	if ( name == NULL ) {
		return false;
	}
	return !idStr::Icmp( name, "_currentRender" )
		|| !idStr::Icmp( name, "BlurTexture1" )
		|| !idStr::Icmp( name, "_currentDepth" )
		|| !idStr::Icmp( name, "DepthTexture" );
}

static bool R_MaterialResourceTable_ImageIsSceneCapture( const idImage *image ) {
	return R_IsMutableRenderImage( image );
}

static bool R_MaterialResourceTable_TexgenIsScreenSpace( texgen_t texgen ) {
	return texgen == TG_SCREEN
		|| texgen == TG_SCREEN2
		|| texgen == TG_GLASSWARP
		|| texgen == TG_POT_CORRECTION;
}

static bool R_MaterialResourceTable_TexgenIsCubeOrSky( texgen_t texgen ) {
	return texgen == TG_DIFFUSE_CUBE
		|| texgen == TG_REFLECT_CUBE
		|| texgen == TG_SKYBOX_CUBE
		|| texgen == TG_WOBBLESKY_CUBE;
}

static materialResourceTextureSemantic_t R_MaterialResourceTable_StageSemantic( const shaderStage_t &stage, rendererMaterialClass_t materialClass, bool needsCurrentRender ) {
	if ( R_MaterialResourceTable_ImageIsPostProcess( stage.texture.image ) || needsCurrentRender ) {
		return MATERIAL_RESOURCE_TEXTURE_POST_PROCESS;
	}
	switch ( stage.lighting ) {
	case SL_BUMP:
		return MATERIAL_RESOURCE_TEXTURE_BUMP;
	case SL_DIFFUSE:
		return MATERIAL_RESOURCE_TEXTURE_DIFFUSE;
	case SL_SPECULAR:
		return MATERIAL_RESOURCE_TEXTURE_SPECULAR;
	case SL_AMBIENT:
	default:
		if ( materialClass == RENDER_MATERIAL_GUI ) {
			return MATERIAL_RESOURCE_TEXTURE_GUI;
		}
		if ( materialClass == RENDER_MATERIAL_POST_PROCESS ) {
			return MATERIAL_RESOURCE_TEXTURE_POST_PROCESS;
		}
		return MATERIAL_RESOURCE_TEXTURE_EMISSIVE;
	}
}

static materialResourceSortGroup_t R_MaterialResourceTable_SortGroupForMaterial( const idMaterial *material, rendererMaterialClass_t materialClass ) {
	if ( materialClass == RENDER_MATERIAL_SUBVIEW ) {
		return MATERIAL_RESOURCE_SORT_SUBVIEW;
	}
	if ( materialClass == RENDER_MATERIAL_GUI ) {
		return MATERIAL_RESOURCE_SORT_GUI;
	}
	if ( materialClass == RENDER_MATERIAL_POST_PROCESS ) {
		return MATERIAL_RESOURCE_SORT_POST_PROCESS;
	}
	if ( material == NULL ) {
		return MATERIAL_RESOURCE_SORT_UNKNOWN;
	}
	const float sort = material->GetSort();
	if ( sort == SS_SUBVIEW ) {
		return MATERIAL_RESOURCE_SORT_SUBVIEW;
	}
	if ( sort == SS_GUI || sort == SS_PREGUI ) {
		return MATERIAL_RESOURCE_SORT_GUI;
	}
	if ( sort >= SS_POST_PROCESS ) {
		return MATERIAL_RESOURCE_SORT_POST_PROCESS;
	}
	if ( sort >= SS_DECAL && sort < SS_FAR ) {
		return MATERIAL_RESOURCE_SORT_DECAL;
	}
	if ( sort >= SS_FAR ) {
		return MATERIAL_RESOURCE_SORT_TRANSLUCENT;
	}
	if ( sort >= SS_OPAQUE ) {
		return MATERIAL_RESOURCE_SORT_OPAQUE;
	}
	return MATERIAL_RESOURCE_SORT_UNKNOWN;
}

static int R_MaterialResourceTable_BlendBits( int drawStateBits );
static bool R_MaterialResourceTable_IsAdditiveBlend( int drawStateBits );
static bool R_MaterialResourceTable_IsFilterBlend( int drawStateBits );
static bool R_MaterialResourceTable_IsAlphaBlend( int drawStateBits );

static materialResourceBlendMode_t R_MaterialResourceTable_BlendModeForMaterial( const idMaterial *material, rendererMaterialClass_t materialClass ) {
	if ( materialClass == RENDER_MATERIAL_GUI ) {
		return MATERIAL_RESOURCE_BLEND_GUI;
	}
	if ( materialClass == RENDER_MATERIAL_POST_PROCESS ) {
		return MATERIAL_RESOURCE_BLEND_POST_PROCESS;
	}
	if ( materialClass == RENDER_MATERIAL_PERFORATED ) {
		return MATERIAL_RESOURCE_BLEND_ALPHA_TEST;
	}
	const int stageCount = material != NULL ? material->GetNumStages() : 0;
	if ( stageCount <= 0 ) {
		return materialClass == RENDER_MATERIAL_TRANSLUCENT ? MATERIAL_RESOURCE_BLEND_BLEND : MATERIAL_RESOURCE_BLEND_OPAQUE;
	}

	bool sawAlphaTest = false;
	for ( int i = 0; i < stageCount; ++i ) {
		const shaderStage_t *stage = material->GetStage( i );
		if ( stage != NULL && stage->hasAlphaTest ) {
			sawAlphaTest = true;
			break;
		}
	}
	if ( sawAlphaTest ) {
		return MATERIAL_RESOURCE_BLEND_ALPHA_TEST;
	}

	const shaderStage_t *firstStage = material->GetStage( 0 );
	const int blendBits = firstStage != NULL ? ( firstStage->drawStateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) : 0;
	if ( materialClass == RENDER_MATERIAL_TRANSLUCENT ) {
		if ( blendBits == ( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE ) ) {
			return MATERIAL_RESOURCE_BLEND_ADD;
		}
		if ( firstStage != NULL && R_MaterialResourceTable_IsFilterBlend( firstStage->drawStateBits ) ) {
			return MATERIAL_RESOURCE_BLEND_FILTER;
		}
		return MATERIAL_RESOURCE_BLEND_BLEND;
	}
	if ( blendBits == ( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE ) ) {
		return MATERIAL_RESOURCE_BLEND_ADD;
	}
	if ( firstStage != NULL && R_MaterialResourceTable_IsFilterBlend( firstStage->drawStateBits ) ) {
		return MATERIAL_RESOURCE_BLEND_FILTER;
	}
	if ( blendBits == ( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA ) ) {
		return MATERIAL_RESOURCE_BLEND_BLEND;
	}
	return MATERIAL_RESOURCE_BLEND_OPAQUE;
}

static bool R_MaterialResourceTable_RecordNeedsSurfaceImage( const materialResourceTableRecord_t &record ) {
	return record.materialClass == RENDER_MATERIAL_OPAQUE
		|| record.materialClass == RENDER_MATERIAL_PERFORATED
		|| record.materialClass == RENDER_MATERIAL_TRANSLUCENT
		|| record.materialClass == RENDER_MATERIAL_GUI
		|| record.materialClass == RENDER_MATERIAL_POST_PROCESS;
}

static int R_MaterialResourceTable_SemanticSlot( materialResourceTextureSemantic_t semantic ) {
	switch ( semantic ) {
	case MATERIAL_RESOURCE_TEXTURE_BUMP:
		return 0;
	case MATERIAL_RESOURCE_TEXTURE_DIFFUSE:
		return 1;
	case MATERIAL_RESOURCE_TEXTURE_SPECULAR:
		return 2;
	case MATERIAL_RESOURCE_TEXTURE_EMISSIVE:
		return 3;
	case MATERIAL_RESOURCE_TEXTURE_GUI:
		return 4;
	case MATERIAL_RESOURCE_TEXTURE_POST_PROCESS:
		return 5;
	case MATERIAL_RESOURCE_TEXTURE_ALBEDO:
	case MATERIAL_RESOURCE_TEXTURE_NORMAL:
	case MATERIAL_RESOURCE_TEXTURE_ORM:
	case MATERIAL_RESOURCE_TEXTURE_METALLIC:
	case MATERIAL_RESOURCE_TEXTURE_ROUGHNESS:
	case MATERIAL_RESOURCE_TEXTURE_AO:
	case MATERIAL_RESOURCE_TEXTURE_EMISSIVE_PBR:
		// Phase 3 records PBR handles but does not allocate visible shader units.
		// The existing classic and shadow bindings therefore remain untouched.
		return -1;
	default:
		return -1;
	}
}

static const materialResourceTextureBinding_t *R_MaterialResourceTable_FindStageBinding( const materialResourceTableRecord_t &record, materialResourceTextureSemantic_t semantic ) {
	const int bindingIndex = ( semantic > MATERIAL_RESOURCE_TEXTURE_NONE && semantic < MATERIAL_RESOURCE_TEXTURE_COUNT ) ? record.semanticBindingIndex[semantic] : -1;
	if ( bindingIndex >= 0 && bindingIndex < record.textureBindingCount && record.textures[bindingIndex].stageIndex >= 0 ) {
		return &record.textures[bindingIndex];
	}
	return NULL;
}

static bool R_MaterialResourceTable_ColorRegistersMatch( const materialResourceTextureBinding_t &a, const materialResourceTextureBinding_t &b ) {
	for ( int i = 0; i < 4; ++i ) {
		if ( a.colorRegisters[i] != b.colorRegisters[i] ) {
			return false;
		}
	}
	return true;
}

static void R_MaterialResourceTable_AddStageColorFallback( materialResourceTableRecord_t &record ) {
	const bool hadFallback = ( record.fallbackFlags & MATERIAL_RESOURCE_FALLBACK_FLAG_STAGE_COLOR ) != 0;
	R_MaterialResourceTable_AddFallback( record, MATERIAL_RESOURCE_FALLBACK_STAGE_COLOR, MATERIAL_RESOURCE_FALLBACK_FLAG_STAGE_COLOR );
	if ( !hadFallback ) {
		rg_materialResourceTable.stats.unsupportedFeatures++;
	}
}

static void R_MaterialResourceTable_ValidateStageColorContract( materialResourceTableRecord_t &record ) {
	const materialResourceTextureBinding_t *diffuse = R_MaterialResourceTable_FindStageBinding( record, MATERIAL_RESOURCE_TEXTURE_DIFFUSE );
	const materialResourceTextureBinding_t *emissive = R_MaterialResourceTable_FindStageBinding( record, MATERIAL_RESOURCE_TEXTURE_EMISSIVE );
	if ( diffuse == NULL || emissive == NULL || R_MaterialResourceTable_ColorRegistersMatch( *diffuse, *emissive ) ) {
		return;
	}

	// The modern material programs currently carry one material tint. Stock light
	// cards often combine a neutral diffuse base with a parm-colored additive
	// glow, so let the legacy path preserve the separate glow color/off state.
	R_MaterialResourceTable_AddStageColorFallback( record );
}

static bool R_MaterialResourceTable_RecordHasLitTextureStages( const materialResourceTableRecord_t &record ) {
	return record.hasBump || record.hasDiffuse || record.hasSpecular;
}

static bool R_MaterialResourceTable_AmbientOverlayNeedsLegacyStage( const materialResourceTextureBinding_t &emissive ) {
	if ( emissive.semantic != MATERIAL_RESOURCE_TEXTURE_EMISSIVE || emissive.stageIndex < 0 ) {
		return false;
	}
	if ( R_MaterialResourceTable_BlendBits( emissive.drawStateBits ) == 0 ) {
		return false;
	}
	return R_MaterialResourceTable_IsAdditiveBlend( emissive.drawStateBits )
		|| R_MaterialResourceTable_IsFilterBlend( emissive.drawStateBits )
		|| R_MaterialResourceTable_IsAlphaBlend( emissive.drawStateBits )
		|| emissive.colorMasked
		|| !emissive.depthWrite;
}

static void R_MaterialResourceTable_ValidateAmbientOverlayContract( materialResourceTableRecord_t &record ) {
	if ( record.materialClass != RENDER_MATERIAL_OPAQUE && record.materialClass != RENDER_MATERIAL_PERFORATED ) {
		return;
	}
	if ( !R_MaterialResourceTable_RecordHasLitTextureStages( record ) ) {
		return;
	}
	const materialResourceTextureBinding_t *emissive = R_MaterialResourceTable_FindStageBinding( record, MATERIAL_RESOURCE_TEXTURE_EMISSIVE );
	if ( emissive == NULL || !R_MaterialResourceTable_AmbientOverlayNeedsLegacyStage( *emissive ) ) {
		return;
	}

	// The modern opaque programs only carry a single emissive texture channel.
	// Authored overlay stages such as multiplayer bright-skin glows still need
	// their own blend/depth/color-mask state to avoid tinting the base material.
	R_MaterialResourceTable_AddStageColorFallback( record );
}

static void R_MaterialResourceTable_InitSelfTestRecord( materialResourceTableRecord_t &record ) {
	memset( &record, 0, sizeof( record ) );
	for ( int i = 0; i < MATERIAL_RESOURCE_TEXTURE_COUNT; ++i ) {
		record.semanticBindingIndex[i] = -1;
	}
	record.fallbackReason = MATERIAL_RESOURCE_FALLBACK_NONE;
}

static bool R_MaterialResourceTable_RunAmbientOverlayContractSelfTest( void ) {
	const int unsupportedStart = rg_materialResourceTable.stats.unsupportedFeatures;

	materialResourceTableRecord_t overlay;
	R_MaterialResourceTable_InitSelfTestRecord( overlay );
	overlay.materialClass = RENDER_MATERIAL_OPAQUE;
	overlay.hasDiffuse = true;
	overlay.textureBindingCount = 1;
	overlay.semanticBindingIndex[MATERIAL_RESOURCE_TEXTURE_EMISSIVE] = 0;
	overlay.textures[0].semantic = MATERIAL_RESOURCE_TEXTURE_EMISSIVE;
	overlay.textures[0].stageIndex = 3;
	overlay.textures[0].drawStateBits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
	R_MaterialResourceTable_ValidateAmbientOverlayContract( overlay );
	if ( overlay.fallbackReason != MATERIAL_RESOURCE_FALLBACK_STAGE_COLOR
		|| ( overlay.fallbackFlags & MATERIAL_RESOURCE_FALLBACK_FLAG_STAGE_COLOR ) == 0
		|| rg_materialResourceTable.stats.unsupportedFeatures != unsupportedStart + 1 ) {
		common->Printf( "RendererMaterialResourceTable self-test failed: additive overlay fallback mismatch\n" );
		return false;
	}

	materialResourceTableRecord_t ambientOnly;
	R_MaterialResourceTable_InitSelfTestRecord( ambientOnly );
	ambientOnly.materialClass = RENDER_MATERIAL_OPAQUE;
	ambientOnly.textureBindingCount = 1;
	ambientOnly.semanticBindingIndex[MATERIAL_RESOURCE_TEXTURE_EMISSIVE] = 0;
	ambientOnly.textures[0].semantic = MATERIAL_RESOURCE_TEXTURE_EMISSIVE;
	ambientOnly.textures[0].stageIndex = 1;
	ambientOnly.textures[0].drawStateBits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
	R_MaterialResourceTable_ValidateAmbientOverlayContract( ambientOnly );
	if ( ambientOnly.fallbackReason != MATERIAL_RESOURCE_FALLBACK_NONE ) {
		common->Printf( "RendererMaterialResourceTable self-test failed: ambient-only overlay should not fallback\n" );
		return false;
	}

	materialResourceTableRecord_t unblended;
	R_MaterialResourceTable_InitSelfTestRecord( unblended );
	unblended.materialClass = RENDER_MATERIAL_OPAQUE;
	unblended.hasDiffuse = true;
	unblended.textureBindingCount = 1;
	unblended.semanticBindingIndex[MATERIAL_RESOURCE_TEXTURE_EMISSIVE] = 0;
	unblended.textures[0].semantic = MATERIAL_RESOURCE_TEXTURE_EMISSIVE;
	unblended.textures[0].stageIndex = 2;
	R_MaterialResourceTable_ValidateAmbientOverlayContract( unblended );
	if ( unblended.fallbackReason != MATERIAL_RESOURCE_FALLBACK_NONE
		|| rg_materialResourceTable.stats.unsupportedFeatures != unsupportedStart + 1 ) {
		common->Printf( "RendererMaterialResourceTable self-test failed: unblended emissive fallback mismatch\n" );
		return false;
	}

	return true;
}

static int R_MaterialResourceTable_BlendBits( int drawStateBits ) {
	return drawStateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS );
}

static bool R_MaterialResourceTable_IsAdditiveBlend( int drawStateBits ) {
	return R_MaterialResourceTable_BlendBits( drawStateBits ) == ( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE );
}

static bool R_MaterialResourceTable_IsFilterBlend( int drawStateBits ) {
	const int blendBits = R_MaterialResourceTable_BlendBits( drawStateBits );
	return blendBits == ( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO )
		|| blendBits == ( GLS_SRCBLEND_ZERO | GLS_DSTBLEND_SRC_COLOR )
		|| blendBits == ( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_SRC_COLOR )
		|| blendBits == ( GLS_SRCBLEND_ZERO | GLS_DSTBLEND_ONE_MINUS_SRC_COLOR );
}

static bool R_MaterialResourceTable_IsAlphaBlend( int drawStateBits ) {
	return R_MaterialResourceTable_BlendBits( drawStateBits ) == ( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );
}

static void R_MaterialResourceTable_UpdateRecordSemanticFlags( materialResourceTableRecord_t &record, materialResourceTextureSemantic_t semantic, const idImage *image ) {
	const bool present = image != NULL;
	switch ( semantic ) {
	case MATERIAL_RESOURCE_TEXTURE_BUMP:
		record.hasBump |= present;
		break;
	case MATERIAL_RESOURCE_TEXTURE_DIFFUSE:
		record.hasDiffuse |= present;
		break;
	case MATERIAL_RESOURCE_TEXTURE_SPECULAR:
		record.hasSpecular |= present;
		break;
	case MATERIAL_RESOURCE_TEXTURE_EMISSIVE:
		record.hasEmissive |= present;
		break;
	case MATERIAL_RESOURCE_TEXTURE_GUI:
		record.hasGui |= present;
		break;
	case MATERIAL_RESOURCE_TEXTURE_POST_PROCESS:
		record.hasPostProcess |= present;
		break;
	case MATERIAL_RESOURCE_TEXTURE_ALBEDO:
		record.hasPBRAlbedo |= present;
		break;
	case MATERIAL_RESOURCE_TEXTURE_NORMAL:
		record.hasPBRNormal |= present;
		break;
	case MATERIAL_RESOURCE_TEXTURE_ORM:
		record.hasPBRORM |= present;
		break;
	case MATERIAL_RESOURCE_TEXTURE_METALLIC:
		record.hasPBRMetallic |= present;
		break;
	case MATERIAL_RESOURCE_TEXTURE_ROUGHNESS:
		record.hasPBRRoughness |= present;
		break;
	case MATERIAL_RESOURCE_TEXTURE_AO:
		record.hasPBRAO |= present;
		break;
	case MATERIAL_RESOURCE_TEXTURE_EMISSIVE_PBR:
		record.hasPBREmissive |= present;
		break;
	default:
		break;
	}
}

static unsigned int R_MaterialResourceTable_ImageHandleOrZero( const idImage *image ) {
	if ( image != NULL && image->IsLoaded() ) {
		return const_cast<idImage *>( image )->GetDeviceHandle();
	}
	return 0;
}

static int R_MaterialResourceTable_TextureArrayCapacity( void ) {
	if ( !rg_materialResourceTable.stats.textureArraysSupported ) {
		return 0;
	}
	if ( rg_materialResourceTable.caps.maxTextureImageUnits > 0 ) {
		return Min( rg_materialResourceTable.caps.maxTextureImageUnits, MATERIAL_RESOURCE_TABLE_TEXTURE_ARRAY_CAPACITY );
	}
	return MATERIAL_RESOURCE_TABLE_TEXTURE_ARRAY_CAPACITY;
}

static int R_MaterialResourceTable_FindTextureArrayTableIndexInternal( unsigned int textureHandle ) {
	if ( textureHandle == 0 ) {
		return -1;
	}
	for ( int i = 0; i < rg_materialResourceTable.textureArrayTableCount; ++i ) {
		if ( rg_materialResourceTable.textureArrayTable[i] == textureHandle ) {
			return i;
		}
	}
	return -1;
}

static int R_MaterialResourceTable_AddTextureArrayTableHandle( unsigned int textureHandle ) {
	if ( textureHandle == 0 ) {
		return -1;
	}
	const int existing = R_MaterialResourceTable_FindTextureArrayTableIndexInternal( textureHandle );
	if ( existing >= 0 ) {
		return existing;
	}
	const int capacity = R_MaterialResourceTable_TextureArrayCapacity();
	if ( rg_materialResourceTable.textureArrayTableCount >= capacity ) {
		rg_materialResourceTable.stats.textureArrayTableOverflows++;
		return -1;
	}
	const int index = rg_materialResourceTable.textureArrayTableCount++;
	rg_materialResourceTable.textureArrayTable[index] = textureHandle;
	return index;
}

static void R_MaterialResourceTable_AddTextureArrayFallbackImage( const idImage *image ) {
	R_MaterialResourceTable_AddTextureArrayTableHandle( R_MaterialResourceTable_ImageHandleOrZero( image ) );
}

static void R_MaterialResourceTable_SeedTextureArrayFallbacks( void ) {
	if ( globalImages == NULL ) {
		return;
	}
	R_MaterialResourceTable_AddTextureArrayFallbackImage( globalImages->defaultImage );
	R_MaterialResourceTable_AddTextureArrayFallbackImage( globalImages->whiteImage );
	R_MaterialResourceTable_AddTextureArrayFallbackImage( globalImages->blackImage );
	R_MaterialResourceTable_AddTextureArrayFallbackImage( globalImages->flatNormalMap );
}

static int R_MaterialResourceTable_FindTextureBindingIndex( const materialResourceTableRecord_t &record, materialResourceTextureSemantic_t semantic ) {
	if ( semantic <= MATERIAL_RESOURCE_TEXTURE_NONE || semantic >= MATERIAL_RESOURCE_TEXTURE_COUNT ) {
		return -1;
	}
	const int bindingIndex = record.semanticBindingIndex[semantic];
	return bindingIndex >= 0 && bindingIndex < record.textureBindingCount ? bindingIndex : -1;
}

static bool R_MaterialResourceTable_HasSemanticBinding( const materialResourceTableRecord_t &record, materialResourceTextureSemantic_t semantic ) {
	return R_MaterialResourceTable_FindTextureBindingIndex( record, semantic ) >= 0;
}

static bool R_MaterialResourceTable_IsPBRSemantic( materialResourceTextureSemantic_t semantic ) {
	return semantic >= MATERIAL_RESOURCE_TEXTURE_ALBEDO && semantic < MATERIAL_RESOURCE_TEXTURE_COUNT;
}

static void R_MaterialResourceTable_AddTextureBinding(
	materialResourceTableRecord_t &record,
	materialResourceTextureSemantic_t semantic,
	const idImage *image,
	const shaderStage_t *stage,
	int stageIndex ) {
	if ( semantic <= MATERIAL_RESOURCE_TEXTURE_NONE || semantic >= MATERIAL_RESOURCE_TEXTURE_COUNT || image == NULL ) {
		if ( R_MaterialResourceTable_IsPBRSemantic( semantic ) ) {
			R_MaterialResourceTable_AddPBRFallback( record, MATERIAL_RESOURCE_PBR_FALLBACK_MISSING_IMAGE );
		} else {
			record.hasMissingImage = true;
			R_MaterialResourceTable_AddFallback( record, MATERIAL_RESOURCE_FALLBACK_MISSING_IMAGE, MATERIAL_RESOURCE_FALLBACK_FLAG_MISSING_IMAGE );
		}
		rg_materialResourceTable.stats.missingImages++;
		return;
	}
	if ( R_MaterialResourceTable_HasSemanticBinding( record, semantic ) ) {
		R_MaterialResourceTable_UpdateRecordSemanticFlags( record, semantic, image );
		return;
	}
	if ( record.textureBindingCount >= MATERIAL_RESOURCE_TABLE_MAX_TEXTURE_BINDINGS ) {
		if ( R_MaterialResourceTable_IsPBRSemantic( semantic ) ) {
			R_MaterialResourceTable_AddPBRFallback( record, MATERIAL_RESOURCE_PBR_FALLBACK_TOO_MANY_TEXTURES );
		} else {
			R_MaterialResourceTable_AddFallback( record, MATERIAL_RESOURCE_FALLBACK_TOO_MANY_TEXTURES, MATERIAL_RESOURCE_FALLBACK_FLAG_TOO_MANY_TEXTURES );
		}
		rg_materialResourceTable.stats.unsupportedFeatures++;
		return;
	}

	const int bindingIndex = record.textureBindingCount++;
	materialResourceTextureBinding_t &binding = record.textures[bindingIndex];
	memset( &binding, 0, sizeof( binding ) );
	binding.semantic = semantic;
	binding.image = image;
	binding.textureHandle = image->IsLoaded() ? const_cast<idImage *>( image )->GetDeviceHandle() : 0;
	binding.filter = image->GetFilter();
	binding.repeat = image->GetRepeat();
	binding.classicUnit = R_MaterialResourceTable_SemanticSlot( semantic );
	if ( binding.classicUnit >= rg_materialResourceTable.maxClassicTextureUnits ) {
		if ( R_MaterialResourceTable_IsPBRSemantic( semantic ) ) {
			R_MaterialResourceTable_AddPBRFallback( record, MATERIAL_RESOURCE_PBR_FALLBACK_TOO_MANY_TEXTURES );
		} else {
			R_MaterialResourceTable_AddFallback( record, MATERIAL_RESOURCE_FALLBACK_TOO_MANY_TEXTURES, MATERIAL_RESOURCE_FALLBACK_FLAG_TOO_MANY_TEXTURES );
		}
		binding.classicUnit = -1;
	}
	binding.stageIndex = stageIndex;
	binding.stageRegisterStart = stage != NULL ? stage->mStageRegisterStart : 0;
	binding.stageRegisterCount = stage != NULL ? stage->mNumStageRegisters : 0;
	binding.drawStateBits = stage != NULL ? stage->drawStateBits : 0;
	binding.conditionRegister = stage != NULL ? stage->conditionRegister : 0;
	binding.hasConditionRegister = stage != NULL && stage->mNumStageOps > 0;
	binding.hasAlphaTest = stage != NULL && stage->hasAlphaTest;
	binding.alphaTestMode = stage != NULL ? stage->alphaTestMode : 0;
	binding.alphaTestRegister = stage != NULL ? stage->alphaTestRegister : 0;
	binding.texgen = stage != NULL ? static_cast<int>( stage->texture.texgen ) : static_cast<int>( TG_EXPLICIT );
	binding.vertexColorMode = stage != NULL ? static_cast<int>( stage->vertexColor ) : static_cast<int>( SVC_IGNORE );
	binding.privatePolygonOffset = stage != NULL ? stage->privatePolygonOffset : 0.0f;
	binding.blendEnabled = stage != NULL && R_MaterialResourceTable_BlendBits( stage->drawStateBits ) != 0;
	binding.depthWrite = stage != NULL && ( stage->drawStateBits & GLS_DEPTHMASK ) != 0;
	binding.colorMasked = stage != NULL && ( stage->drawStateBits & GLS_COLORMASK ) != 0;
	for ( int i = 0; i < 4; ++i ) {
		binding.colorRegisters[i] = -1;
	}
	if ( stage != NULL ) {
		memcpy( binding.colorRegisters, stage->color.registers, sizeof( binding.colorRegisters ) );
		memcpy( binding.matrixRegisters, stage->texture.matrix, sizeof( binding.matrixRegisters ) );
		binding.hasTextureMatrix = stage->texture.hasMatrix;
	}
	binding.loaded = image->IsLoaded();
	binding.defaulted = image->IsDefaulted();
	binding.textureArrayCandidate = rg_materialResourceTable.stats.textureArraysSupported && binding.loaded && binding.textureHandle != 0;
	binding.textureArrayLayer = -1;
	binding.textureViewCandidate = rg_materialResourceTable.stats.textureViewsSupported;
	binding.textureViewHandle = 0;
	binding.bindlessSupported = rg_materialResourceTable.stats.bindlessSupported;
	binding.bindlessEnabled = false;
	binding.bindlessHandle = 0;
	const char *semanticName = MaterialResourceTextureSemantic_Name( semantic );
	if ( r_rendererMetrics.GetInteger() >= 2 ) {
		if ( !R_MaterialResourceTable_FormatDebugString(
			binding.debugName,
			sizeof( binding.debugName ),
			"%s:%s",
			semanticName,
			image->GetName() ? image->GetName() : "<unnamed>" ) ) {
			R_MaterialResourceTable_RecordDebugStringTruncation( "texture binding debugName" );
		}
	} else if ( !R_MaterialResourceTable_CopyDebugString( binding.debugName, sizeof( binding.debugName ), semanticName ) ) {
		R_MaterialResourceTable_RecordDebugStringTruncation( "texture binding semantic" );
	}

	record.semanticBindingIndex[semantic] = bindingIndex;
	const unsigned int semanticBit = MaterialResourceTextureSemantic_Bit( semantic );
	record.textureSemanticMask |= semanticBit;
	if ( binding.textureHandle != 0 ) {
		record.loadedTextureSemanticMask |= semanticBit;
		if ( semantic == MATERIAL_RESOURCE_TEXTURE_DIFFUSE
			|| semantic == MATERIAL_RESOURCE_TEXTURE_EMISSIVE
			|| semantic == MATERIAL_RESOURCE_TEXTURE_GUI
			|| semantic == MATERIAL_RESOURCE_TEXTURE_POST_PROCESS
			|| semantic == MATERIAL_RESOURCE_TEXTURE_ALBEDO
			|| semantic == MATERIAL_RESOURCE_TEXTURE_EMISSIVE_PBR ) {
			record.renderableColorTextureMask |= semanticBit;
		}
	}
	R_MaterialResourceTable_UpdateRecordSemanticFlags( record, semantic, image );
	if ( binding.defaulted ) {
		record.hasDefaultedImage = true;
		rg_materialResourceTable.stats.defaultedImages++;
	}
	rg_materialResourceTable.stats.textureBindings++;
	if ( binding.classicUnit >= 0 ) {
		rg_materialResourceTable.stats.classicTextureBindings++;
	}
	if ( binding.textureViewCandidate ) {
		rg_materialResourceTable.stats.textureViewDescriptors++;
	}
}

static void R_MaterialResourceTable_BuildTextureArrayTable( void ) {
	memset( rg_materialResourceTable.textureArrayTable, 0, sizeof( rg_materialResourceTable.textureArrayTable ) );
	rg_materialResourceTable.textureArrayTableCount = 0;
	rg_materialResourceTable.stats.textureArrayTableCapacity = R_MaterialResourceTable_TextureArrayCapacity();
	if ( !rg_materialResourceTable.stats.textureArraysSupported || rg_materialResourceTable.stats.textureArrayTableCapacity <= 0 ) {
		return;
	}

	R_MaterialResourceTable_SeedTextureArrayFallbacks();
	for ( int recordIndex = 0; recordIndex < rg_materialResourceTable.stats.records; ++recordIndex ) {
		materialResourceTableRecord_t &record = rg_materialResourceTable.records[recordIndex];
		// PBR records stay on the legacy/classic owner until a dedicated PBR
		// shader path exists.  Do not let their bindings consume the shared
		// classic-modern texture table and perturb unrelated classic records.
		if ( record.hasPBR ) {
			for ( int bindingIndex = 0; bindingIndex < record.textureBindingCount; ++bindingIndex ) {
				record.textures[bindingIndex].textureArrayCandidate = false;
				record.textures[bindingIndex].textureArrayLayer = -1;
			}
			continue;
		}
		for ( int bindingIndex = 0; bindingIndex < record.textureBindingCount; ++bindingIndex ) {
			materialResourceTextureBinding_t &binding = record.textures[bindingIndex];
			if ( !binding.loaded || binding.textureHandle == 0 ) {
				binding.textureArrayCandidate = false;
				binding.textureArrayLayer = -1;
				continue;
			}
			const int layer = R_MaterialResourceTable_AddTextureArrayTableHandle( binding.textureHandle );
			binding.textureArrayCandidate = layer >= 0;
			binding.textureArrayLayer = layer;
			if ( layer >= 0 ) {
				rg_materialResourceTable.stats.textureArrayDescriptors++;
				rg_materialResourceTable.stats.textureArrayTableDescriptors++;
			}
		}
	}
	rg_materialResourceTable.stats.textureArrayTableTextures = rg_materialResourceTable.textureArrayTableCount;
	rg_materialResourceTable.stats.textureArrayTableReady = rg_materialResourceTable.textureArrayTableCount > 0;
}

static void R_MaterialResourceTable_CountClass( const materialResourceTableRecord_t &record ) {
	switch ( record.materialClass ) {
	case RENDER_MATERIAL_OPAQUE:
		rg_materialResourceTable.stats.opaqueRecords++;
		break;
	case RENDER_MATERIAL_PERFORATED:
		rg_materialResourceTable.stats.perforatedRecords++;
		break;
	case RENDER_MATERIAL_TRANSLUCENT:
		rg_materialResourceTable.stats.translucentRecords++;
		break;
	case RENDER_MATERIAL_GUI:
		rg_materialResourceTable.stats.guiRecords++;
		break;
	case RENDER_MATERIAL_SUBVIEW:
		rg_materialResourceTable.stats.subviewRecords++;
		break;
	case RENDER_MATERIAL_POST_PROCESS:
		rg_materialResourceTable.stats.postProcessRecords++;
		break;
	case RENDER_MATERIAL_SHADOW_ONLY:
		rg_materialResourceTable.stats.shadowOnlyRecords++;
		break;
	default:
		break;
	}
	if ( record.alphaTest ) {
		rg_materialResourceTable.stats.alphaTestRecords++;
	}
	if ( record.blendMode == MATERIAL_RESOURCE_BLEND_BLEND
		|| record.blendMode == MATERIAL_RESOURCE_BLEND_ADD
		|| record.blendMode == MATERIAL_RESOURCE_BLEND_FILTER
		|| record.blendMode == MATERIAL_RESOURCE_BLEND_GUI
		|| record.blendMode == MATERIAL_RESOURCE_BLEND_POST_PROCESS ) {
		rg_materialResourceTable.stats.blendRecords++;
	}
}

static void R_MaterialResourceTable_CountFallbacks( const materialResourceTableRecord_t &record ) {
	if ( record.fallbackReason == MATERIAL_RESOURCE_FALLBACK_NONE ) {
		return;
	}
	rg_materialResourceTable.stats.fallbackRecords++;
	if ( ( record.fallbackFlags & MATERIAL_RESOURCE_FALLBACK_FLAG_MISSING_MATERIAL ) != 0 ) {
		rg_materialResourceTable.stats.fallbackMissingMaterial++;
	}
	if ( ( record.fallbackFlags & MATERIAL_RESOURCE_FALLBACK_FLAG_NO_DRAW_STAGES ) != 0 ) {
		rg_materialResourceTable.stats.fallbackNoDrawStages++;
	}
	if ( ( record.fallbackFlags & MATERIAL_RESOURCE_FALLBACK_FLAG_MISSING_IMAGE ) != 0 ) {
		rg_materialResourceTable.stats.fallbackMissingImage++;
	}
	if ( ( record.fallbackFlags & MATERIAL_RESOURCE_FALLBACK_FLAG_CUSTOM_PROGRAM ) != 0 ) {
		rg_materialResourceTable.stats.fallbackCustomProgram++;
	}
	if ( ( record.fallbackFlags & MATERIAL_RESOURCE_FALLBACK_FLAG_CUSTOM_GLSL ) != 0 ) {
		rg_materialResourceTable.stats.fallbackCustomGLSL++;
	}
	if ( ( record.fallbackFlags & MATERIAL_RESOURCE_FALLBACK_FLAG_DYNAMIC_IMAGE ) != 0 ) {
		rg_materialResourceTable.stats.fallbackDynamicImage++;
	}
	if ( ( record.fallbackFlags & MATERIAL_RESOURCE_FALLBACK_FLAG_CURRENT_RENDER_IMAGE ) != 0 ) {
		rg_materialResourceTable.stats.fallbackCurrentRenderImage++;
	}
	if ( ( record.fallbackFlags & MATERIAL_RESOURCE_FALLBACK_FLAG_SCREEN_TEXGEN ) != 0 ) {
		rg_materialResourceTable.stats.fallbackScreenTexgen++;
	}
	if ( ( record.fallbackFlags & MATERIAL_RESOURCE_FALLBACK_FLAG_SKY_TEXGEN ) != 0 ) {
		rg_materialResourceTable.stats.fallbackSkyTexgen++;
	}
	if ( ( record.fallbackFlags & MATERIAL_RESOURCE_FALLBACK_FLAG_UNSUPPORTED_TEXGEN ) != 0 ) {
		rg_materialResourceTable.stats.fallbackUnsupportedTexgen++;
	}
	if ( ( record.fallbackFlags & MATERIAL_RESOURCE_FALLBACK_FLAG_NEEDS_CURRENT_RENDER ) != 0 ) {
		rg_materialResourceTable.stats.fallbackNeedsCurrentRender++;
	}
	if ( ( record.fallbackFlags & MATERIAL_RESOURCE_FALLBACK_FLAG_STAGE_CONDITION ) != 0 ) {
		rg_materialResourceTable.stats.fallbackStageCondition++;
	}
	if ( ( record.fallbackFlags & MATERIAL_RESOURCE_FALLBACK_FLAG_STAGE_COLOR ) != 0 ) {
		rg_materialResourceTable.stats.fallbackStageColor++;
	}
	if ( ( record.fallbackFlags & MATERIAL_RESOURCE_FALLBACK_FLAG_TEXTURE_MATRIX ) != 0 ) {
		rg_materialResourceTable.stats.fallbackTextureMatrix++;
	}
	if ( ( record.fallbackFlags & MATERIAL_RESOURCE_FALLBACK_FLAG_VERTEX_COLOR ) != 0 ) {
		rg_materialResourceTable.stats.fallbackVertexColor++;
	}
	if ( ( record.fallbackFlags & MATERIAL_RESOURCE_FALLBACK_FLAG_POLYGON_OFFSET ) != 0 ) {
		rg_materialResourceTable.stats.fallbackPolygonOffset++;
	}
	if ( ( record.fallbackFlags & MATERIAL_RESOURCE_FALLBACK_FLAG_TOO_MANY_TEXTURES ) != 0 ) {
		rg_materialResourceTable.stats.fallbackTooManyTextures++;
	}
}

static void R_MaterialResourceTable_FinalizeRegisterRange( materialResourceTableRecord_t &record ) {
	record.stageRegisterStart = record.registerCount > 0 ? record.registerCount : 0;
	int stageRegisterEnd = 0;
	for ( int i = 0; i < record.textureBindingCount; ++i ) {
		const materialResourceTextureBinding_t &binding = record.textures[i];
		if ( binding.stageRegisterCount <= 0 ) {
			continue;
		}
		if ( binding.stageRegisterStart < record.stageRegisterStart ) {
			record.stageRegisterStart = binding.stageRegisterStart;
		}
		if ( binding.stageRegisterStart + binding.stageRegisterCount > stageRegisterEnd ) {
			stageRegisterEnd = binding.stageRegisterStart + binding.stageRegisterCount;
		}
	}
	if ( stageRegisterEnd <= record.stageRegisterStart ) {
		record.stageRegisterStart = 0;
		record.stageRegisterCount = 0;
	} else {
		record.stageRegisterCount = stageRegisterEnd - record.stageRegisterStart;
	}
}

static void R_MaterialResourceTable_ScanMaterialStages( materialResourceTableRecord_t &record, const materialResourceRecord_t &sourceRecord ) {
	const idMaterial *material = sourceRecord.material;
	if ( material == NULL ) {
		R_MaterialResourceTable_AddFallback( record, MATERIAL_RESOURCE_FALLBACK_MISSING_MATERIAL, MATERIAL_RESOURCE_FALLBACK_FLAG_MISSING_MATERIAL );
		return;
	}
	record.stageCount = material->GetNumStages();
	if ( !material->IsDrawn() ) {
		R_MaterialResourceTable_AddFallback( record, MATERIAL_RESOURCE_FALLBACK_NO_DRAW_STAGES, MATERIAL_RESOURCE_FALLBACK_FLAG_NO_DRAW_STAGES );
	}
	if ( material->TestMaterialFlag( MF_NEED_CURRENT_RENDER ) ) {
		R_MaterialResourceTable_AddFallback( record, MATERIAL_RESOURCE_FALLBACK_NEEDS_CURRENT_RENDER, MATERIAL_RESOURCE_FALLBACK_FLAG_NEEDS_CURRENT_RENDER );
	}

	for ( int i = 0; i < record.stageCount; ++i ) {
		const shaderStage_t *stage = material->GetStage( i );
		if ( stage == NULL ) {
			continue;
		}
		record.evaluatedStageCount++;
		if ( stage->mNumStageOps > 0 ) {
			record.hasConditionRegisters = true;
			R_MaterialResourceTable_AddFallback( record, MATERIAL_RESOURCE_FALLBACK_STAGE_CONDITION, MATERIAL_RESOURCE_FALLBACK_FLAG_STAGE_CONDITION );
			rg_materialResourceTable.stats.unsupportedFeatures++;
		}
		if ( stage->texture.hasMatrix ) {
			record.hasTextureMatrix = true;
			R_MaterialResourceTable_AddFallback( record, MATERIAL_RESOURCE_FALLBACK_TEXTURE_MATRIX, MATERIAL_RESOURCE_FALLBACK_FLAG_TEXTURE_MATRIX );
			rg_materialResourceTable.stats.unsupportedFeatures++;
		}
		if ( stage->vertexColor != SVC_IGNORE ) {
			record.hasVertexColor = true;
			R_MaterialResourceTable_AddFallback( record, MATERIAL_RESOURCE_FALLBACK_VERTEX_COLOR, MATERIAL_RESOURCE_FALLBACK_FLAG_VERTEX_COLOR );
			rg_materialResourceTable.stats.unsupportedFeatures++;
		}
		if ( stage->privatePolygonOffset != 0.0f ) {
			record.hasPrivatePolygonOffset = true;
			R_MaterialResourceTable_AddFallback( record, MATERIAL_RESOURCE_FALLBACK_POLYGON_OFFSET, MATERIAL_RESOURCE_FALLBACK_FLAG_POLYGON_OFFSET );
			rg_materialResourceTable.stats.unsupportedFeatures++;
		}
		if ( R_MaterialResourceTable_IsAdditiveBlend( stage->drawStateBits ) ) {
			record.additiveStageCount++;
		} else if ( R_MaterialResourceTable_IsFilterBlend( stage->drawStateBits ) ) {
			record.filterStageCount++;
		} else if ( R_MaterialResourceTable_IsAlphaBlend( stage->drawStateBits ) ) {
			record.blendStageCount++;
		}
		if ( stage->hasAlphaTest ) {
			record.alphaTest = true;
			record.alphaTestMode = stage->alphaTestMode;
			record.alphaTestRegister = stage->alphaTestRegister;
			record.shadowAlphaTest = true;
			record.shadowAlphaTestMode = stage->alphaTestMode;
			record.shadowAlphaTestRegister = stage->alphaTestRegister;
			record.shadowUsesTextureMatrix |= stage->texture.hasMatrix;
			record.shadowUsesVertexColor |= stage->vertexColor != SVC_IGNORE;
		}
		if ( stage->newStage != NULL ) {
			record.hasCustomProgram = true;
			if ( stage->newStage->glslProgram ) {
				record.hasCustomGLSL = true;
				R_MaterialResourceTable_AddFallback( record, MATERIAL_RESOURCE_FALLBACK_CUSTOM_GLSL, MATERIAL_RESOURCE_FALLBACK_FLAG_CUSTOM_GLSL | MATERIAL_RESOURCE_FALLBACK_FLAG_CUSTOM_PROGRAM );
			} else {
				R_MaterialResourceTable_AddFallback( record, MATERIAL_RESOURCE_FALLBACK_CUSTOM_PROGRAM, MATERIAL_RESOURCE_FALLBACK_FLAG_CUSTOM_PROGRAM );
			}
			rg_materialResourceTable.stats.unsupportedFeatures++;
		}
		if ( stage->texture.dynamic != DI_STATIC ) {
			record.hasDynamicImage = true;
			R_MaterialResourceTable_AddFallback( record, MATERIAL_RESOURCE_FALLBACK_DYNAMIC_IMAGE, MATERIAL_RESOURCE_FALLBACK_FLAG_DYNAMIC_IMAGE );
			rg_materialResourceTable.stats.unsupportedFeatures++;
		}
		if ( R_MaterialResourceTable_ImageIsSceneCapture( stage->texture.image ) ) {
			record.hasSceneCaptureImage = true;
			R_MaterialResourceTable_AddFallback( record, MATERIAL_RESOURCE_FALLBACK_CURRENT_RENDER_IMAGE, MATERIAL_RESOURCE_FALLBACK_FLAG_CURRENT_RENDER_IMAGE | MATERIAL_RESOURCE_FALLBACK_FLAG_NEEDS_CURRENT_RENDER );
			rg_materialResourceTable.stats.unsupportedFeatures++;
		}
		if ( stage->texture.texgen != TG_EXPLICIT ) {
			if ( R_MaterialResourceTable_TexgenIsScreenSpace( stage->texture.texgen ) ) {
				record.hasScreenTexgen = true;
				R_MaterialResourceTable_AddFallback( record, MATERIAL_RESOURCE_FALLBACK_SCREEN_TEXGEN, MATERIAL_RESOURCE_FALLBACK_FLAG_SCREEN_TEXGEN | MATERIAL_RESOURCE_FALLBACK_FLAG_UNSUPPORTED_TEXGEN );
			} else if ( R_MaterialResourceTable_TexgenIsCubeOrSky( stage->texture.texgen ) ) {
				record.hasSkyTexgen = true;
				R_MaterialResourceTable_AddFallback( record, MATERIAL_RESOURCE_FALLBACK_SKY_TEXGEN, MATERIAL_RESOURCE_FALLBACK_FLAG_SKY_TEXGEN | MATERIAL_RESOURCE_FALLBACK_FLAG_UNSUPPORTED_TEXGEN );
			} else {
				R_MaterialResourceTable_AddFallback( record, MATERIAL_RESOURCE_FALLBACK_UNSUPPORTED_TEXGEN, MATERIAL_RESOURCE_FALLBACK_FLAG_UNSUPPORTED_TEXGEN );
			}
			rg_materialResourceTable.stats.unsupportedFeatures++;
		}

		const materialResourceTextureSemantic_t semantic = R_MaterialResourceTable_StageSemantic( *stage, record.materialClass, record.needsCurrentRender );
		if ( !R_MaterialResourceTable_HasSemanticBinding( record, semantic ) ) {
			R_MaterialResourceTable_AddTextureBinding( record, semantic, stage->texture.image, stage, i );
		}
		if ( stage->hasAlphaTest && record.shadowAlphaBindingIndex < 0 ) {
			record.shadowAlphaBindingIndex = R_MaterialResourceTable_FindTextureBindingIndex( record, semantic );
		}
	}

	record.shadowCasterSupported = record.castsShadow && record.fallbackReason == MATERIAL_RESOURCE_FALLBACK_NONE;
	if ( record.fallbackReason != MATERIAL_RESOURCE_FALLBACK_NONE ) {
		record.shadowFallbackFlags |= record.fallbackFlags;
		record.shadowCasterSupported = false;
	}
	if ( record.shadowAlphaTest && record.shadowAlphaBindingIndex < 0 ) {
		record.shadowFallbackFlags |= MATERIAL_RESOURCE_FALLBACK_FLAG_MISSING_IMAGE;
		record.shadowCasterSupported = false;
	}
	if ( record.shadowUsesTextureMatrix ) {
		record.shadowFallbackFlags |= MATERIAL_RESOURCE_FALLBACK_FLAG_UNSUPPORTED_TEXGEN;
		record.shadowCasterSupported = false;
	}
	if ( record.shadowUsesVertexColor || record.twoSided ) {
		record.shadowFallbackFlags |= MATERIAL_RESOURCE_FALLBACK_FLAG_CUSTOM_PROGRAM;
		record.shadowCasterSupported = false;
	}
}

static void R_MaterialResourceTable_AddSourceImages( materialResourceTableRecord_t &record, const materialResourceRecord_t &sourceRecord ) {
	if ( sourceRecord.normalImage != NULL ) {
		R_MaterialResourceTable_AddTextureBinding( record, MATERIAL_RESOURCE_TEXTURE_BUMP, sourceRecord.normalImage, NULL, -1 );
	}
	if ( sourceRecord.diffuseImage != NULL ) {
		R_MaterialResourceTable_AddTextureBinding( record, MATERIAL_RESOURCE_TEXTURE_DIFFUSE, sourceRecord.diffuseImage, NULL, -1 );
	}
	if ( sourceRecord.specularImage != NULL ) {
		R_MaterialResourceTable_AddTextureBinding( record, MATERIAL_RESOURCE_TEXTURE_SPECULAR, sourceRecord.specularImage, NULL, -1 );
	}
	if ( sourceRecord.permutation.materialClass == RENDER_MATERIAL_GUI && sourceRecord.diffuseImage != NULL ) {
		R_MaterialResourceTable_AddTextureBinding( record, MATERIAL_RESOURCE_TEXTURE_GUI, sourceRecord.diffuseImage, NULL, -1 );
	}
	if ( sourceRecord.permutation.materialClass == RENDER_MATERIAL_POST_PROCESS && sourceRecord.diffuseImage != NULL ) {
		R_MaterialResourceTable_AddTextureBinding( record, MATERIAL_RESOURCE_TEXTURE_POST_PROCESS, sourceRecord.diffuseImage, NULL, -1 );
	}
}

static void R_MaterialResourceTable_CountPBR( const materialResourceTableRecord_t &record ) {
	if ( !record.hasPBR ) {
		rg_materialResourceTable.stats.classicRecords++;
		return;
	}
	materialResourceTableStats_t &stats = rg_materialResourceTable.stats;
	stats.pbrRecords++;
	stats.pbrResourceReadyRecords += record.pbrResourceReady ? 1 : 0;
	stats.pbrModernReadyRecords += record.pbrModernReady ? 1 : 0;
	stats.pbrPackedMapRecords += record.pbrPackedMaterialData ? 1 : 0;
	stats.pbrSeparateMapRecords += record.pbrSeparateMaterialData ? 1 : 0;
	stats.pbrAuthoredClassicFallbackRecords += record.pbrHasAuthoredClassicFallback ? 1 : 0;
	stats.pbrExplicitGeneratedFallbackRecords += record.pbrHasExplicitLegacyFallback && record.pbrUsesGeneratedLegacyFallback ? 1 : 0;
	stats.pbrGeneratedFallbackRecords += record.pbrUsesGeneratedLegacyFallback ? 1 : 0;
	stats.pbrApproximateFallbackRecords += record.pbrUsesApproximateLegacyFallback ? 1 : 0;
	stats.pbrMissingLegacyFallbackRecords += record.pbrLegacyFallbackMissing ? 1 : 0;
	stats.pbrMissingAlbedoMapRecords += record.hasPBRAlbedo ? 0 : 1;
	stats.pbrMissingNormalMapRecords += record.hasPBRNormal ? 0 : 1;
	stats.pbrMissingORMMapRecords += record.hasPBRORM ? 0 : 1;
	if ( record.pbrFallbackReason == MATERIAL_RESOURCE_PBR_FALLBACK_NONE ) {
		return;
	}
	stats.pbrFallbackRecords++;
	switch ( record.pbrFallbackReason ) {
	case MATERIAL_RESOURCE_PBR_FALLBACK_DISABLED:
		stats.pbrFallbackDisabled++;
		break;
	case MATERIAL_RESOURCE_PBR_FALLBACK_UNSUPPORTED_WORKFLOW:
		stats.pbrFallbackUnsupportedWorkflow++;
		break;
	case MATERIAL_RESOURCE_PBR_FALLBACK_UNSUPPORTED_MATERIAL_CLASS:
		stats.pbrFallbackUnsupportedMaterialClass++;
		break;
	case MATERIAL_RESOURCE_PBR_FALLBACK_MISSING_ALBEDO:
		stats.pbrFallbackMissingAlbedo++;
		break;
	case MATERIAL_RESOURCE_PBR_FALLBACK_MISSING_NORMAL_FORMAT:
		stats.pbrFallbackMissingNormalFormat++;
		break;
	case MATERIAL_RESOURCE_PBR_FALLBACK_CONFLICTING_LAYOUT:
		stats.pbrFallbackConflictingLayout++;
		break;
	case MATERIAL_RESOURCE_PBR_FALLBACK_MISSING_IMAGE:
		stats.pbrFallbackMissingImage++;
		break;
	case MATERIAL_RESOURCE_PBR_FALLBACK_CLASSIC_FEATURE:
		stats.pbrFallbackClassicFeature++;
		break;
	case MATERIAL_RESOURCE_PBR_FALLBACK_TOO_MANY_TEXTURES:
		stats.pbrFallbackTooManyTextures++;
		break;
	case MATERIAL_RESOURCE_PBR_FALLBACK_SHADER_PATH_UNAVAILABLE:
		stats.pbrFallbackShaderPathUnavailable++;
		break;
	case MATERIAL_RESOURCE_PBR_FALLBACK_NONE:
	default:
		break;
	}
}

static void R_MaterialResourceTable_AddPBRSourceImages( materialResourceTableRecord_t &record, const materialResourceRecord_t &sourceRecord ) {
	if ( !sourceRecord.hasPBR ) {
		return;
	}
	record.hasPBR = true;
	record.pbrWorkflow = sourceRecord.pbrWorkflow;
	record.pbrNormalFormat = sourceRecord.pbrNormalFormat;
	record.pbrHasAuthoredClassicFallback = sourceRecord.pbrHasAuthoredClassicFallback;
	record.pbrHasExplicitLegacyFallback = sourceRecord.pbrHasExplicitLegacyFallback;
	record.pbrUsesGeneratedLegacyFallback = sourceRecord.pbrUsesGeneratedLegacyFallback;
	record.pbrUsesApproximateLegacyFallback = sourceRecord.pbrUsesApproximateLegacyFallback;
	record.pbrLegacyFallbackMissing = sourceRecord.pbrLegacyFallbackMissing;
	record.pbrMetallicRegister = sourceRecord.pbrMetallicRegister;
	record.pbrRoughnessRegister = sourceRecord.pbrRoughnessRegister;
	record.pbrAORegister = sourceRecord.pbrAORegister;
	record.pbrNormalScaleRegister = sourceRecord.pbrNormalScaleRegister;
	memcpy( record.pbrEmissiveColorRegisters, sourceRecord.pbrEmissiveColorRegisters, sizeof( record.pbrEmissiveColorRegisters ) );

	if ( sourceRecord.pbrAlbedoImage != NULL ) {
		R_MaterialResourceTable_AddTextureBinding( record, MATERIAL_RESOURCE_TEXTURE_ALBEDO, sourceRecord.pbrAlbedoImage, NULL, -1 );
	}
	if ( sourceRecord.pbrNormalImage != NULL ) {
		R_MaterialResourceTable_AddTextureBinding( record, MATERIAL_RESOURCE_TEXTURE_NORMAL, sourceRecord.pbrNormalImage, NULL, -1 );
	}
	if ( sourceRecord.pbrORMImage != NULL ) {
		R_MaterialResourceTable_AddTextureBinding( record, MATERIAL_RESOURCE_TEXTURE_ORM, sourceRecord.pbrORMImage, NULL, -1 );
	}
	if ( sourceRecord.pbrMetallicImage != NULL ) {
		R_MaterialResourceTable_AddTextureBinding( record, MATERIAL_RESOURCE_TEXTURE_METALLIC, sourceRecord.pbrMetallicImage, NULL, -1 );
	}
	if ( sourceRecord.pbrRoughnessImage != NULL ) {
		R_MaterialResourceTable_AddTextureBinding( record, MATERIAL_RESOURCE_TEXTURE_ROUGHNESS, sourceRecord.pbrRoughnessImage, NULL, -1 );
	}
	if ( sourceRecord.pbrAOImage != NULL ) {
		R_MaterialResourceTable_AddTextureBinding( record, MATERIAL_RESOURCE_TEXTURE_AO, sourceRecord.pbrAOImage, NULL, -1 );
	}
	if ( sourceRecord.pbrEmissiveImage != NULL ) {
		R_MaterialResourceTable_AddTextureBinding( record, MATERIAL_RESOURCE_TEXTURE_EMISSIVE_PBR, sourceRecord.pbrEmissiveImage, NULL, -1 );
	}
	record.pbrPackedMaterialData = sourceRecord.pbrORMImage != NULL;
	record.pbrSeparateMaterialData = sourceRecord.pbrMetallicImage != NULL
		|| sourceRecord.pbrRoughnessImage != NULL
		|| sourceRecord.pbrAOImage != NULL;
}

static bool R_MaterialResourceTable_PBRBindingReady( const materialResourceTableRecord_t &record, materialResourceTextureSemantic_t semantic, bool required ) {
	const int index = R_MaterialResourceTable_FindTextureBindingIndex( record, semantic );
	if ( index < 0 ) {
		return !required;
	}
	const materialResourceTextureBinding_t &binding = record.textures[index];
	return binding.image != NULL
		&& binding.loaded
		&& !binding.defaulted
		&& !R_IsMutableRenderImage( binding.image );
}

static void R_MaterialResourceTable_FinalizePBRContract( materialResourceTableRecord_t &record ) {
	if ( !record.hasPBR ) {
		return;
	}

	if ( !record.hasPBRAlbedo ) {
		R_MaterialResourceTable_AddPBRFallback( record, MATERIAL_RESOURCE_PBR_FALLBACK_MISSING_ALBEDO );
	}
	if ( record.hasPBRNormal && record.pbrNormalFormat == PBR_NORMAL_UNSPECIFIED ) {
		R_MaterialResourceTable_AddPBRFallback( record, MATERIAL_RESOURCE_PBR_FALLBACK_MISSING_NORMAL_FORMAT );
	}
	if ( record.pbrPackedMaterialData && record.pbrSeparateMaterialData ) {
		R_MaterialResourceTable_AddPBRFallback( record, MATERIAL_RESOURCE_PBR_FALLBACK_CONFLICTING_LAYOUT );
	}
	if ( record.pbrWorkflow != PBR_WORKFLOW_METALLIC_ROUGHNESS ) {
		R_MaterialResourceTable_AddPBRFallback( record, MATERIAL_RESOURCE_PBR_FALLBACK_UNSUPPORTED_WORKFLOW );
	}
	if ( record.materialClass != RENDER_MATERIAL_OPAQUE && record.materialClass != RENDER_MATERIAL_PERFORATED ) {
		R_MaterialResourceTable_AddPBRFallback( record, MATERIAL_RESOURCE_PBR_FALLBACK_UNSUPPORTED_MATERIAL_CLASS );
	}
	if ( !R_MaterialResourceTable_PBRBindingReady( record, MATERIAL_RESOURCE_TEXTURE_ALBEDO, true )
		|| !R_MaterialResourceTable_PBRBindingReady( record, MATERIAL_RESOURCE_TEXTURE_NORMAL, record.hasPBRNormal )
		|| !R_MaterialResourceTable_PBRBindingReady( record, MATERIAL_RESOURCE_TEXTURE_ORM, record.hasPBRORM )
		|| !R_MaterialResourceTable_PBRBindingReady( record, MATERIAL_RESOURCE_TEXTURE_METALLIC, record.hasPBRMetallic )
		|| !R_MaterialResourceTable_PBRBindingReady( record, MATERIAL_RESOURCE_TEXTURE_ROUGHNESS, record.hasPBRRoughness )
		|| !R_MaterialResourceTable_PBRBindingReady( record, MATERIAL_RESOURCE_TEXTURE_AO, record.hasPBRAO )
		|| !R_MaterialResourceTable_PBRBindingReady( record, MATERIAL_RESOURCE_TEXTURE_EMISSIVE_PBR, record.hasPBREmissive ) ) {
		R_MaterialResourceTable_AddPBRFallback( record, MATERIAL_RESOURCE_PBR_FALLBACK_MISSING_IMAGE );
	}
	if ( record.fallbackReason != MATERIAL_RESOURCE_FALLBACK_NONE ) {
		R_MaterialResourceTable_AddPBRFallback( record, MATERIAL_RESOURCE_PBR_FALLBACK_CLASSIC_FEATURE );
	}

	record.pbrResourceReady = record.pbrFallbackReason == MATERIAL_RESOURCE_PBR_FALLBACK_NONE;
	if ( record.pbrFallbackReason == MATERIAL_RESOURCE_PBR_FALLBACK_NONE && !r_pbrMaterials.GetBool() ) {
		R_MaterialResourceTable_AddPBRFallback( record, MATERIAL_RESOURCE_PBR_FALLBACK_DISABLED );
	}
	// Phase 3 publishes complete resource ownership but deliberately does not
	// claim a visible PBR shader path. Phase 4 replaces this reason only after
	// G-buffer and forward/deferred program readiness are proven.
	if ( record.pbrFallbackReason == MATERIAL_RESOURCE_PBR_FALLBACK_NONE ) {
		R_MaterialResourceTable_AddPBRFallback( record, MATERIAL_RESOURCE_PBR_FALLBACK_SHADER_PATH_UNAVAILABLE );
	}
	record.pbrModernReady = false;
}

static void R_MaterialResourceTable_FinalizeShadowContract( materialResourceTableRecord_t &record ) {
	record.shadowFallbackFlags |= record.fallbackFlags;
	record.shadowCasterSupported = record.castsShadow && record.fallbackReason == MATERIAL_RESOURCE_FALLBACK_NONE;
	if ( record.shadowAlphaTest && record.shadowAlphaBindingIndex < 0 ) {
		record.shadowFallbackFlags |= MATERIAL_RESOURCE_FALLBACK_FLAG_MISSING_IMAGE;
		record.shadowCasterSupported = false;
	}
	if ( record.shadowUsesTextureMatrix ) {
		record.shadowFallbackFlags |= MATERIAL_RESOURCE_FALLBACK_FLAG_UNSUPPORTED_TEXGEN;
		record.shadowCasterSupported = false;
	}
	if ( record.shadowUsesVertexColor || record.twoSided ) {
		record.shadowFallbackFlags |= MATERIAL_RESOURCE_FALLBACK_FLAG_CUSTOM_PROGRAM;
		record.shadowCasterSupported = false;
	}
}

static bool R_MaterialResourceTable_AddRecordFromSource( const materialResourceRecord_t &sourceRecord, int sourceIndex, bool scanMaterialStages ) {
	if ( rg_materialResourceTable.stats.records >= MATERIAL_RESOURCE_TABLE_MAX_RECORDS ) {
		rg_materialResourceTable.stats.overflow = true;
		R_MaterialResourceTable_SetStatus( "material table overflow" );
		return false;
	}

	materialResourceTableRecord_t &record = rg_materialResourceTable.records[rg_materialResourceTable.stats.records];
	memset( &record, 0, sizeof( record ) );
	for ( int i = 0; i < MATERIAL_RESOURCE_TEXTURE_COUNT; ++i ) {
		record.semanticBindingIndex[i] = -1;
	}
	record.tableIndex = rg_materialResourceTable.stats.records;
	record.sourceMaterialRecordIndex = sourceIndex;
	// The scene-packet record owns the stable decl index. Synthetic validation
	// decls allocated through declManager intentionally have no idDeclBase and
	// therefore must never be asked for Index() directly.
	record.materialId = sourceRecord.material != NULL ? sourceRecord.resourceTableIndex : -1;
	record.material = sourceRecord.material;
	if ( !R_MaterialResourceTable_CopyDebugString( record.materialName, sizeof( record.materialName ), sourceRecord.material != NULL ? sourceRecord.material->GetName() : "<missing>" ) ) {
		R_MaterialResourceTable_RecordDebugStringTruncation( "material record name" );
	}
	record.materialClass = static_cast<rendererMaterialClass_t>( sourceRecord.permutation.materialClass );
	record.sortValue = sourceRecord.material != NULL ? sourceRecord.material->GetSort() : 0.0f;
	record.sortGroup = R_MaterialResourceTable_SortGroupForMaterial( sourceRecord.material, record.materialClass );
	record.blendMode = R_MaterialResourceTable_BlendModeForMaterial( sourceRecord.material, record.materialClass );
	record.drawn = sourceRecord.material != NULL ? sourceRecord.material->IsDrawn() : false;
	record.receivesLighting = sourceRecord.material != NULL ? sourceRecord.material->ReceivesLighting() : false;
	record.castsShadow = sourceRecord.material != NULL
		? ( !sourceRecord.material->IsDedicatedCollisionSurface() && sourceRecord.material->SurfaceCastsShadow() )
		: false;
	record.cullType = sourceRecord.material != NULL ? sourceRecord.material->GetCullType() : CT_FRONT_SIDED;
	record.shouldCreateBackSides = sourceRecord.material != NULL && sourceRecord.material->ShouldCreateBackSides();
	record.twoSided = sourceRecord.material != NULL ? ( record.cullType == CT_TWO_SIDED || record.shouldCreateBackSides ) : false;
	record.alphaTest = sourceRecord.permutation.alphaMode == MC_PERFORATED || record.blendMode == MATERIAL_RESOURCE_BLEND_ALPHA_TEST;
	record.alphaTestMode = 0;
	record.alphaTestRegister = 0;
	record.needsCurrentRender = sourceRecord.material != NULL && sourceRecord.material->TestMaterialFlag( MF_NEED_CURRENT_RENDER );
	record.hasGui = sourceRecord.material != NULL && sourceRecord.material->HasGui();
	record.hasSubview = sourceRecord.material != NULL && sourceRecord.material->HasSubview();
	record.hasMaterialPolygonOffset = sourceRecord.material != NULL && sourceRecord.material->TestMaterialFlag( MF_POLYGONOFFSET );
	record.polygonOffset = sourceRecord.material != NULL ? sourceRecord.material->GetPolygonOffset() : 0.0f;
	record.shadowAlphaBindingIndex = -1;
	record.registerStart = 0;
	record.registerCount = sourceRecord.material != NULL ? sourceRecord.material->GetNumRegisters() : 0;
	record.fallbackReason = MATERIAL_RESOURCE_FALLBACK_NONE;
	if ( record.hasMaterialPolygonOffset ) {
		R_MaterialResourceTable_AddFallback( record, MATERIAL_RESOURCE_FALLBACK_POLYGON_OFFSET, MATERIAL_RESOURCE_FALLBACK_FLAG_POLYGON_OFFSET );
		rg_materialResourceTable.stats.unsupportedFeatures++;
	}

	if ( scanMaterialStages ) {
		R_MaterialResourceTable_ScanMaterialStages( record, sourceRecord );
		R_MaterialResourceTable_AddSourceImages( record, sourceRecord );
	} else if ( sourceRecord.material == NULL ) {
		R_MaterialResourceTable_AddSourceImages( record, sourceRecord );
		R_MaterialResourceTable_AddFallback( record, MATERIAL_RESOURCE_FALLBACK_MISSING_MATERIAL, MATERIAL_RESOURCE_FALLBACK_FLAG_MISSING_MATERIAL );
	} else {
		R_MaterialResourceTable_AddSourceImages( record, sourceRecord );
	}
	R_MaterialResourceTable_AddPBRSourceImages( record, sourceRecord );
	if ( R_MaterialResourceTable_RecordNeedsSurfaceImage( record )
		&& !record.hasDiffuse
		&& !record.hasEmissive
		&& !record.hasGui
		&& !record.hasPostProcess ) {
		record.hasMissingImage = true;
		R_MaterialResourceTable_AddFallback( record, MATERIAL_RESOURCE_FALLBACK_MISSING_IMAGE, MATERIAL_RESOURCE_FALLBACK_FLAG_MISSING_IMAGE );
		rg_materialResourceTable.stats.missingImages++;
	}
	R_MaterialResourceTable_ValidateStageColorContract( record );
	R_MaterialResourceTable_ValidateAmbientOverlayContract( record );
	R_MaterialResourceTable_FinalizePBRContract( record );
	if ( !scanMaterialStages ) {
		record.shadowCasterSupported = record.castsShadow && record.fallbackReason == MATERIAL_RESOURCE_FALLBACK_NONE;
		record.shadowFallbackFlags = record.fallbackFlags;
	}
	R_MaterialResourceTable_FinalizeShadowContract( record );
	R_MaterialResourceTable_FinalizeRegisterRange( record );
	R_MaterialResourceTable_CountClass( record );
	R_MaterialResourceTable_CountFallbacks( record );
	R_MaterialResourceTable_CountPBR( record );

	R_MaterialResourceTable_HashInsert( record.material, rg_materialResourceTable.stats.records );
	rg_materialResourceTable.stats.records++;
	return true;
}

static void R_MaterialResourceTable_ResetFrameStats( void ) {
	const bool initialized = rg_materialResourceTable.stats.initialized;
	const bool available = rg_materialResourceTable.stats.available;
	const bool bindlessSupported = rg_materialResourceTable.stats.bindlessSupported;
	const bool textureArraysSupported = rg_materialResourceTable.stats.textureArraysSupported;
	const bool textureViewsSupported = rg_materialResourceTable.stats.textureViewsSupported;
	// records are not cleared wholesale (~2.3 MB): AddRecordFromSource memsets each
	// record before use, and every reader is bounded by stats.records or materialHash,
	// so entries past stats.records hold stale prior-frame data
	memset( rg_materialResourceTable.textureArrayTable, 0, sizeof( rg_materialResourceTable.textureArrayTable ) );
	memset( rg_materialResourceTable.materialHash, 0, sizeof( rg_materialResourceTable.materialHash ) );
	rg_materialResourceTable.textureArrayTableCount = 0;
	memset( &rg_materialResourceTable.stats, 0, sizeof( rg_materialResourceTable.stats ) );
	rg_materialResourceTable.stats.initialized = initialized;
	rg_materialResourceTable.stats.available = available;
	rg_materialResourceTable.stats.bindlessSupported = bindlessSupported;
	rg_materialResourceTable.stats.bindlessEnabled = false;
	rg_materialResourceTable.stats.textureArraysSupported = textureArraysSupported;
	rg_materialResourceTable.stats.textureViewsSupported = textureViewsSupported;
	R_MaterialResourceTable_SetStatus( available ? "ready" : "unavailable" );
}

void R_MaterialResourceTable_Init( const renderBackendCaps_t &caps, const renderFeatureSet_t &features ) {
	memset( &rg_materialResourceTable, 0, sizeof( rg_materialResourceTable ) );
	rg_materialResourceTable.caps = caps;
	rg_materialResourceTable.features = features;
	rg_materialResourceTable.maxClassicTextureUnits = caps.maxTextureImageUnits > 0 ? Min( caps.maxTextureImageUnits, MATERIAL_RESOURCE_TABLE_MAX_TEXTURE_BINDINGS ) : MATERIAL_RESOURCE_TABLE_MAX_TEXTURE_BINDINGS;
	rg_materialResourceTable.stats.initialized = true;
	rg_materialResourceTable.stats.available = features.scenePackets;
	rg_materialResourceTable.stats.bindlessSupported = features.bindlessTextures && caps.hasBindlessTexture;
	rg_materialResourceTable.stats.bindlessEnabled = false;
	rg_materialResourceTable.stats.textureArraysSupported = features.gpuDriven && caps.hasTextureArrays && caps.maxTextureImageUnits >= MATERIAL_RESOURCE_TABLE_TEXTURE_ARRAY_CAPACITY;
	rg_materialResourceTable.stats.textureViewsSupported = features.gpuDriven && caps.hasTextureViews;
	rg_materialResourceTable.stats.textureArrayTableCapacity = R_MaterialResourceTable_TextureArrayCapacity();
	R_MaterialResourceTable_SetStatus( rg_materialResourceTable.stats.available ? "ready" : "scene packets unavailable" );
}

void R_MaterialResourceTable_Shutdown( void ) {
	memset( &rg_materialResourceTable, 0, sizeof( rg_materialResourceTable ) );
	R_MaterialResourceTable_SetStatus( "shutdown" );
}

void R_MaterialResourceTable_PrepareFrame( const idScenePacketFrame &packetFrame ) {
	idGLDebugScope scope( "MaterialResourceTable::PrepareFrame" );
	R_MaterialResourceTable_ResetFrameStats();
	rg_materialResourceTable.stats.prepared = true;
	const int materialRecordCount = packetFrame.NumMaterialRecords();
	const int drawPacketCount = packetFrame.NumDrawPackets();
	rg_materialResourceTable.stats.sourceMaterialRecords = materialRecordCount;
	for ( int i = 0; i < drawPacketCount; ++i ) {
		if ( packetFrame.DrawPacket( i ).materialRecordIndex >= 0 ) {
			rg_materialResourceTable.stats.drawPacketReferences++;
		}
	}
	if ( !rg_materialResourceTable.stats.available ) {
		return;
	}
	for ( int i = 0; i < materialRecordCount; ++i ) {
		R_MaterialResourceTable_AddRecordFromSource( packetFrame.MaterialRecord( i ), i, true );
	}
	R_MaterialResourceTable_BuildTextureArrayTable();
	if ( rg_materialResourceTable.stats.overflow ) {
		R_MaterialResourceTable_SetStatus( "overflow" );
	} else {
		R_MaterialResourceTable_SetStatus( rg_materialResourceTable.stats.records > 0 ? "ready" : "empty" );
	}
}

const materialResourceTableStats_t &R_MaterialResourceTable_Stats( void ) {
	return rg_materialResourceTable.stats;
}

const materialResourceTableRecord_t *R_MaterialResourceTable_RecordForIndex( int tableIndex ) {
	if ( tableIndex < 0 || tableIndex >= rg_materialResourceTable.stats.records ) {
		return NULL;
	}
	return &rg_materialResourceTable.records[tableIndex];
}

const materialResourceTextureBinding_t *R_MaterialResourceTable_TextureBindingForSemantic( const materialResourceTableRecord_t &record, materialResourceTextureSemantic_t semantic ) {
	const int bindingIndex = R_MaterialResourceTable_FindTextureBindingIndex( record, semantic );
	return bindingIndex >= 0 ? &record.textures[bindingIndex] : NULL;
}

const materialResourceTableRecord_t *R_MaterialResourceTable_FindRecordForMaterial( const idMaterial *material ) {
	if ( material == NULL ) {
		return NULL;
	}
	int slot = R_MaterialResourceTable_HashMaterial( material );
	for ( int probe = 0; probe < MATERIAL_RESOURCE_TABLE_HASH_SIZE; ++probe ) {
		const short entry = rg_materialResourceTable.materialHash[slot];
		if ( entry == 0 ) {
			return NULL;
		}
		const materialResourceTableRecord_t &record = rg_materialResourceTable.records[entry - 1];
		if ( record.material == material ) {
			return &record;
		}
		slot = ( slot + 1 ) & ( MATERIAL_RESOURCE_TABLE_HASH_SIZE - 1 );
	}
	return NULL;
}

const unsigned int *R_MaterialResourceTable_TextureArrayTable( int &count ) {
	count = rg_materialResourceTable.textureArrayTableCount;
	return rg_materialResourceTable.textureArrayTable;
}

int R_MaterialResourceTable_TextureArrayTableIndexForHandle( unsigned int textureHandle ) {
	return R_MaterialResourceTable_FindTextureArrayTableIndexInternal( textureHandle );
}

void R_MaterialResourceTable_PrintGfxInfo( void ) {
	const materialResourceTableStats_t &stats = R_MaterialResourceTable_Stats();
	common->Printf(
		"Material resource table: initialized=%d available=%d prepared=%d records=%d classicRecords=%d source=%d draws=%d textures=%d classicTextures=%d arrays=%d table=%d/%d desc=%d overflow=%d views=%d bindless=%d/%d fallback=%d missing=%d unsupported=%d custom=%d/%d dynamic=%d current=%d texgen=%d(screen=%d sky=%d) condition=%d stageColor=%d matrix=%d vertexColor=%d offset=%d debugTrunc=%d source='%s' status='%s'\n",
		stats.initialized ? 1 : 0,
		stats.available ? 1 : 0,
		stats.prepared ? 1 : 0,
		stats.records,
		stats.classicRecords,
		stats.sourceMaterialRecords,
		stats.drawPacketReferences,
		stats.textureBindings,
		stats.classicTextureBindings,
		stats.textureArrayDescriptors,
		stats.textureArrayTableTextures,
		stats.textureArrayTableCapacity,
		stats.textureArrayTableDescriptors,
		stats.textureArrayTableOverflows,
		stats.textureViewDescriptors,
		stats.bindlessEnabled ? 1 : 0,
		stats.bindlessSupported ? 1 : 0,
		stats.fallbackRecords,
		stats.missingImages,
		stats.unsupportedFeatures,
		stats.fallbackCustomProgram,
		stats.fallbackCustomGLSL,
		stats.fallbackDynamicImage,
		stats.fallbackCurrentRenderImage,
		stats.fallbackUnsupportedTexgen,
		stats.fallbackScreenTexgen,
		stats.fallbackSkyTexgen,
		stats.fallbackStageCondition,
		stats.fallbackStageColor,
		stats.fallbackTextureMatrix,
		stats.fallbackVertexColor,
		stats.fallbackPolygonOffset,
		stats.debugStringTruncations,
		stats.debugStringTruncationSource,
		stats.lastFailure );
	common->Printf(
		"PBR material resources: records=%d resourceReady=%d modernReady=%d packed=%d separate=%d fallback=%d disabled=%d workflow=%d class=%d albedo=%d normalFormat=%d layout=%d image=%d classic=%d units=%d shader=%d authored=%d explicitGenerated=%d generated=%d approximate=%d missingFallback=%d mapsMissing=%d/%d/%d\n",
		stats.pbrRecords,
		stats.pbrResourceReadyRecords,
		stats.pbrModernReadyRecords,
		stats.pbrPackedMapRecords,
		stats.pbrSeparateMapRecords,
		stats.pbrFallbackRecords,
		stats.pbrFallbackDisabled,
		stats.pbrFallbackUnsupportedWorkflow,
		stats.pbrFallbackUnsupportedMaterialClass,
		stats.pbrFallbackMissingAlbedo,
		stats.pbrFallbackMissingNormalFormat,
		stats.pbrFallbackConflictingLayout,
		stats.pbrFallbackMissingImage,
		stats.pbrFallbackClassicFeature,
		stats.pbrFallbackTooManyTextures,
		stats.pbrFallbackShaderPathUnavailable,
		stats.pbrAuthoredClassicFallbackRecords,
		stats.pbrExplicitGeneratedFallbackRecords,
		stats.pbrGeneratedFallbackRecords,
		stats.pbrApproximateFallbackRecords,
		stats.pbrMissingLegacyFallbackRecords,
		stats.pbrMissingAlbedoMapRecords,
		stats.pbrMissingNormalMapRecords,
		stats.pbrMissingORMMapRecords );
}

bool R_MaterialResourceTable_ClassicModernPathEligible( const materialResourceTableRecord_t &record ) {
	// The current modern-visible programs implement the classic Quake 4 stage
	// contract.  PBR metadata must remain on the compatible legacy owner until a
	// dedicated PBR pipeline explicitly consumes pbrModernReady and its bindings.
	return !record.hasPBR;
}

void R_MaterialResourceTable_DumpLatest( void ) {
	const materialResourceTableStats_t &stats = R_MaterialResourceTable_Stats();
	common->Printf(
		"MaterialResourceTable dump: prepared=%d available=%d records=%d classicRecords=%d source=%d draws=%d textures=%d table=%d/%d desc=%d overflow=%d fallback=%d missing=%d defaulted=%d unsupported=%d debugTrunc=%d source='%s' status='%s'\n",
		stats.prepared ? 1 : 0,
		stats.available ? 1 : 0,
		stats.records,
		stats.classicRecords,
		stats.sourceMaterialRecords,
		stats.drawPacketReferences,
		stats.textureBindings,
		stats.textureArrayTableTextures,
		stats.textureArrayTableCapacity,
		stats.textureArrayTableDescriptors,
		stats.textureArrayTableOverflows,
		stats.fallbackRecords,
		stats.missingImages,
		stats.defaultedImages,
		stats.unsupportedFeatures,
		stats.debugStringTruncations,
		stats.debugStringTruncationSource,
		stats.lastFailure );
	common->Printf(
		"PBR summary: records=%d resourceReady=%d modernReady=%d packed=%d separate=%d fallback=%d approximate=%d missingFallback=%d mapsMissing=%d/%d/%d\n",
		stats.pbrRecords,
		stats.pbrResourceReadyRecords,
		stats.pbrModernReadyRecords,
		stats.pbrPackedMapRecords,
		stats.pbrSeparateMapRecords,
		stats.pbrFallbackRecords,
		stats.pbrApproximateFallbackRecords,
		stats.pbrMissingLegacyFallbackRecords,
		stats.pbrMissingAlbedoMapRecords,
		stats.pbrMissingNormalMapRecords,
		stats.pbrMissingORMMapRecords );
	for ( int i = 0; i < stats.records; ++i ) {
		const materialResourceTableRecord_t &record = rg_materialResourceTable.records[i];
		common->Printf(
			"  material[%d] source=%d id=%d name='%s' class=%s blend=%s sort=%s/%.2f cull=%d regs=%d+%d stageRegs=%d+%d stages=%d/%d alpha=%d textures=%d fallback=%s flags=0x%x current=%d capture=%d gui=%d subview=%d light=%d shadow=%d/%d matrix=%d vertexColor=%d dynamic=%d screenTexgen=%d skyTexgen=%d custom=%d/%d offset=%.2f twoSided=%d backsides=%d shadowFlags=0x%x\n",
			record.tableIndex,
			record.sourceMaterialRecordIndex,
			record.materialId,
			record.materialName,
			RendererMaterialClass_Name( record.materialClass ),
			MaterialResourceBlendMode_Name( record.blendMode ),
			R_MaterialResourceTable_SortGroupName( record.sortGroup ),
			record.sortValue,
			record.cullType,
			record.registerStart,
			record.registerCount,
			record.stageRegisterStart,
			record.stageRegisterCount,
			record.evaluatedStageCount,
			record.stageCount,
			record.alphaTest ? 1 : 0,
			record.textureBindingCount,
			MaterialResourceFallbackReason_Name( record.fallbackReason ),
			record.fallbackFlags,
			record.needsCurrentRender ? 1 : 0,
			record.hasSceneCaptureImage ? 1 : 0,
			record.hasGui ? 1 : 0,
			record.hasSubview ? 1 : 0,
			record.receivesLighting ? 1 : 0,
			record.castsShadow ? 1 : 0,
			record.shadowCasterSupported ? 1 : 0,
			record.hasTextureMatrix ? 1 : 0,
			record.hasVertexColor ? 1 : 0,
			record.hasDynamicImage ? 1 : 0,
			record.hasScreenTexgen ? 1 : 0,
			record.hasSkyTexgen ? 1 : 0,
			record.hasCustomProgram ? 1 : 0,
			record.hasCustomGLSL ? 1 : 0,
			record.polygonOffset,
			record.twoSided ? 1 : 0,
			record.shouldCreateBackSides ? 1 : 0,
			record.shadowFallbackFlags );
		if ( record.hasPBR ) {
			common->Printf(
				"    pbr workflow=%d normalFormat=%d resourceReady=%d modernReady=%d fallback=%s packed=%d separate=%d maps=a%d n%d orm%d m%d r%d ao%d e%d authored=%d explicit=%d generated=%d approximate=%d missingFallback=%d regs=%d/%d/%d/%d emit=%d,%d,%d\n",
				record.pbrWorkflow,
				record.pbrNormalFormat,
				record.pbrResourceReady ? 1 : 0,
				record.pbrModernReady ? 1 : 0,
				MaterialResourcePBRFallbackReason_Name( record.pbrFallbackReason ),
				record.pbrPackedMaterialData ? 1 : 0,
				record.pbrSeparateMaterialData ? 1 : 0,
				record.hasPBRAlbedo ? 1 : 0,
				record.hasPBRNormal ? 1 : 0,
				record.hasPBRORM ? 1 : 0,
				record.hasPBRMetallic ? 1 : 0,
				record.hasPBRRoughness ? 1 : 0,
				record.hasPBRAO ? 1 : 0,
				record.hasPBREmissive ? 1 : 0,
				record.pbrHasAuthoredClassicFallback ? 1 : 0,
				record.pbrHasExplicitLegacyFallback ? 1 : 0,
				record.pbrUsesGeneratedLegacyFallback ? 1 : 0,
				record.pbrUsesApproximateLegacyFallback ? 1 : 0,
				record.pbrLegacyFallbackMissing ? 1 : 0,
				record.pbrMetallicRegister,
				record.pbrRoughnessRegister,
				record.pbrAORegister,
				record.pbrNormalScaleRegister,
				record.pbrEmissiveColorRegisters[0],
				record.pbrEmissiveColorRegisters[1],
				record.pbrEmissiveColorRegisters[2] );
		}
		for ( int bindingIndex = 0; bindingIndex < record.textureBindingCount; ++bindingIndex ) {
			const materialResourceTextureBinding_t &binding = record.textures[bindingIndex];
			common->Printf(
				"    tex[%d] semantic=%s unit=%d image='%s' handle=%u loaded=%d defaulted=%d filter=%d repeat=%d stage=%d regs=%d+%d cond=%d alpha=%d texgen=%d matrix=%d vcolor=%d blend=%d depthWrite=%d offset=%.2f array=%d layer=%d view=%d bindless=%d\n",
				bindingIndex,
				MaterialResourceTextureSemantic_Name( binding.semantic ),
				binding.classicUnit,
				binding.image != NULL ? binding.image->GetName() : "<missing>",
				binding.textureHandle,
				binding.loaded ? 1 : 0,
				binding.defaulted ? 1 : 0,
				static_cast<int>( binding.filter ),
				static_cast<int>( binding.repeat ),
				binding.stageIndex,
				binding.stageRegisterStart,
				binding.stageRegisterCount,
				binding.hasConditionRegister ? 1 : 0,
				binding.hasAlphaTest ? 1 : 0,
				binding.texgen,
				binding.hasTextureMatrix ? 1 : 0,
				binding.vertexColorMode,
				binding.blendEnabled ? 1 : 0,
				binding.depthWrite ? 1 : 0,
				binding.privatePolygonOffset,
				binding.textureArrayCandidate ? 1 : 0,
				binding.textureArrayLayer,
				binding.textureViewCandidate ? 1 : 0,
				binding.bindlessEnabled ? 1 : 0 );
		}
	}
}

static bool R_MaterialResourceTable_RunPBRContractSelfTest( void ) {
	if ( globalImages == NULL ) {
		common->Printf( "RendererMaterialResourceTable PBR self-test skipped: images unavailable\n" );
		return true;
	}
	static const char declaration[] =
		"material _pbr_resource_table_selftest {\n"
		" bumpmap _flat\n"
		" diffusemap _white\n"
		" specularmap _black\n"
		" pbr {\n"
		"  workflow metallicRoughness\n"
		"  albedoMap _white\n"
		"  normalMap _flat\n"
		"  normalFormat tangentRG\n"
		"  ormMap _white\n"
		"  metallic 0.2\n"
		"  roughness 0.7\n"
		"  ao 1.0\n"
		" }\n"
		"}\n";

	idDecl *materialDecl = declManager->AllocateDecl( DECL_MATERIAL );
	if ( materialDecl == NULL ) {
		common->Printf( "RendererMaterialResourceTable self-test failed: PBR declaration allocation\n" );
		return false;
	}
	idMaterial *material = static_cast<idMaterial *>( materialDecl );
	if ( !material->Parse( declaration, idLib::SizeToInt( sizeof( declaration ) - 1, "PBR resource-table self-test" ) ) ) {
		common->Printf( "RendererMaterialResourceTable self-test failed: PBR declaration parse\n" );
		DeclManager_FreeAllocatedDecl( materialDecl );
		return false;
	}
	const int savedMaxClassicTextureUnits = rg_materialResourceTable.maxClassicTextureUnits;
	// Vulkan can invoke this CPU-only contract before the classic material table
	// is initialized. Scope the same minimum unit budget used by this synthetic
	// bump/diffuse/specular material, then restore the live backend state.
	rg_materialResourceTable.maxClassicTextureUnits = Max( savedMaxClassicTextureUnits, 3 );
	const pbrMaterialInfo_t &pbr = material->GetPBRInfo();
	materialResourceRecord_t source;
	memset( &source, 0, sizeof( source ) );
	source.material = material;
	source.diffuseImage = globalImages->whiteImage;
	source.normalImage = globalImages->flatNormalMap;
	source.specularImage = globalImages->blackImage;
	source.hasPBR = true;
	source.pbrWorkflow = static_cast<int>( pbr.workflow );
	source.pbrNormalFormat = static_cast<int>( pbr.normalFormat );
	source.pbrAlbedoImage = pbr.albedo.image;
	source.pbrNormalImage = pbr.normal.image;
	source.pbrORMImage = pbr.orm.image;
	source.pbrHasAuthoredClassicFallback = pbr.hasAuthoredClassicFallback;
	source.pbrHasExplicitLegacyFallback = pbr.hasExplicitLegacyFallback;
	source.pbrUsesGeneratedLegacyFallback = pbr.usesGeneratedLegacyFallback;
	source.pbrUsesApproximateLegacyFallback = pbr.usesApproximateLegacyFallback;
	source.pbrLegacyFallbackMissing = pbr.legacyFallbackMissing;
	source.pbrMetallicRegister = pbr.metallicRegister;
	source.pbrRoughnessRegister = pbr.roughnessRegister;
	source.pbrAORegister = pbr.aoRegister;
	source.pbrNormalScaleRegister = pbr.normalScaleRegister;
	memcpy( source.pbrEmissiveColorRegisters, pbr.emissiveColorRegisters, sizeof( source.pbrEmissiveColorRegisters ) );
	source.permutation.materialClass = RENDER_MATERIAL_OPAQUE;
	source.permutation.alphaMode = MC_OPAQUE;
	source.resourceTableIndex = 200;

	R_MaterialResourceTable_ResetFrameStats();
	rg_materialResourceTable.stats.prepared = true;
	const bool added = R_MaterialResourceTable_AddRecordFromSource( source, 200, true );

	materialResourceRecord_t separateSource = source;
	separateSource.pbrORMImage = NULL;
	separateSource.pbrMetallicImage = globalImages->whiteImage;
	separateSource.pbrRoughnessImage = globalImages->whiteImage;
	separateSource.pbrAOImage = globalImages->whiteImage;
	separateSource.resourceTableIndex = 201;
	const bool separateAdded = R_MaterialResourceTable_AddRecordFromSource( separateSource, 201, true );

	materialResourceRecord_t scalarSource = source;
	scalarSource.pbrNormalImage = NULL;
	scalarSource.pbrNormalFormat = PBR_NORMAL_UNSPECIFIED;
	scalarSource.pbrORMImage = NULL;
	scalarSource.resourceTableIndex = 202;
	const bool scalarAdded = R_MaterialResourceTable_AddRecordFromSource( scalarSource, 202, true );

	materialResourceRecord_t explicitSource = source;
	explicitSource.pbrHasAuthoredClassicFallback = false;
	explicitSource.pbrHasExplicitLegacyFallback = true;
	explicitSource.pbrUsesGeneratedLegacyFallback = true;
	explicitSource.pbrUsesApproximateLegacyFallback = false;
	explicitSource.resourceTableIndex = 203;
	const bool explicitAdded = R_MaterialResourceTable_AddRecordFromSource( explicitSource, 203, true );

	materialResourceRecord_t unsupportedSource = source;
	unsupportedSource.pbrWorkflow = PBR_WORKFLOW_SPECULAR_GLOSSINESS;
	unsupportedSource.resourceTableIndex = 204;
	const bool unsupportedAdded = R_MaterialResourceTable_AddRecordFromSource( unsupportedSource, 204, true );

	materialResourceRecord_t missingFallbackSource = source;
	missingFallbackSource.pbrHasAuthoredClassicFallback = false;
	missingFallbackSource.pbrLegacyFallbackMissing = true;
	missingFallbackSource.resourceTableIndex = 205;
	const bool missingFallbackAdded = R_MaterialResourceTable_AddRecordFromSource( missingFallbackSource, 205, true );

	materialResourceRecord_t missingAlbedoSource = source;
	missingAlbedoSource.pbrAlbedoImage = NULL;
	missingAlbedoSource.resourceTableIndex = 206;
	const bool missingAlbedoAdded = R_MaterialResourceTable_AddRecordFromSource( missingAlbedoSource, 206, true );

	idImageOpts mutableImageOpts = globalImages->whiteImage->GetOpts();
	idImage *mutableImage = globalImages->ScratchImage(
		"_pbr_resource_table_mutable_selftest",
		&mutableImageOpts,
		TF_DEFAULT,
		TR_REPEAT,
		TD_PBR_COLOR );
	materialResourceRecord_t mutableImageSource = source;
	mutableImageSource.pbrAlbedoImage = mutableImage;
	mutableImageSource.resourceTableIndex = 207;
	const bool mutableImageAdded = mutableImage != NULL
		&& R_MaterialResourceTable_AddRecordFromSource( mutableImageSource, 207, true );

	// Authored legacy-map metadata is not an explicit-generated fallback when
	// the complete classic interaction already made stage generation redundant.
	materialResourceRecord_t redundantExplicitSource = source;
	redundantExplicitSource.pbrHasExplicitLegacyFallback = true;
	redundantExplicitSource.pbrUsesGeneratedLegacyFallback = false;
	redundantExplicitSource.resourceTableIndex = 208;
	const bool redundantExplicitAdded = R_MaterialResourceTable_AddRecordFromSource( redundantExplicitSource, 208, true );

	R_MaterialResourceTable_BuildTextureArrayTable();
	const materialResourceTableRecord_t *record = R_MaterialResourceTable_RecordForIndex( 0 );
	const materialResourceTableRecord_t *separateRecord = R_MaterialResourceTable_RecordForIndex( 1 );
	const materialResourceTableRecord_t *scalarRecord = R_MaterialResourceTable_RecordForIndex( 2 );
	const materialResourceTableRecord_t *explicitRecord = R_MaterialResourceTable_RecordForIndex( 3 );
	const materialResourceTableRecord_t *unsupportedRecord = R_MaterialResourceTable_RecordForIndex( 4 );
	const materialResourceTableRecord_t *missingFallbackRecord = R_MaterialResourceTable_RecordForIndex( 5 );
	const materialResourceTableRecord_t *missingAlbedoRecord = R_MaterialResourceTable_RecordForIndex( 6 );
	const materialResourceTableRecord_t *mutableImageRecord = R_MaterialResourceTable_RecordForIndex( 7 );
	const materialResourceTableRecord_t *redundantExplicitRecord = R_MaterialResourceTable_RecordForIndex( 8 );
	const materialResourceTableStats_t &stats = R_MaterialResourceTable_Stats();
	const materialResourcePBRFallbackReason_t expectedReason = r_pbrMaterials.GetBool()
		? MATERIAL_RESOURCE_PBR_FALLBACK_SHADER_PATH_UNAVAILABLE
		: MATERIAL_RESOURCE_PBR_FALLBACK_DISABLED;
	bool pbrBindingsExcludedFromClassicTable = stats.records == 9;
	for ( int recordIndex = 0; recordIndex < stats.records; ++recordIndex ) {
		const materialResourceTableRecord_t *testRecord = R_MaterialResourceTable_RecordForIndex( recordIndex );
		pbrBindingsExcludedFromClassicTable &= testRecord != NULL && testRecord->hasPBR;
		if ( testRecord != NULL ) {
			for ( int i = 0; i < testRecord->textureBindingCount; ++i ) {
				pbrBindingsExcludedFromClassicTable &= !testRecord->textures[i].textureArrayCandidate
					&& testRecord->textures[i].textureArrayLayer == -1;
			}
		}
	}
	const bool ok = added && separateAdded && scalarAdded && explicitAdded
		&& unsupportedAdded && missingFallbackAdded && missingAlbedoAdded && mutableImageAdded && redundantExplicitAdded
		&& record != NULL
		&& record->hasPBR
		&& record->pbrResourceReady
		&& !record->pbrModernReady
		&& !R_MaterialResourceTable_ClassicModernPathEligible( *record )
		&& record->pbrFallbackReason == expectedReason
		&& record->pbrPackedMaterialData
		&& !record->pbrSeparateMaterialData
		&& record->pbrHasAuthoredClassicFallback
		&& R_MaterialResourceTable_TextureBindingForSemantic( *record, MATERIAL_RESOURCE_TEXTURE_ALBEDO ) != NULL
		&& R_MaterialResourceTable_TextureBindingForSemantic( *record, MATERIAL_RESOURCE_TEXTURE_NORMAL ) != NULL
		&& R_MaterialResourceTable_TextureBindingForSemantic( *record, MATERIAL_RESOURCE_TEXTURE_ORM ) != NULL
		&& separateRecord != NULL && separateRecord->pbrResourceReady
		&& !separateRecord->pbrPackedMaterialData && separateRecord->pbrSeparateMaterialData
		&& scalarRecord != NULL && scalarRecord->pbrResourceReady
		&& !scalarRecord->pbrPackedMaterialData && !scalarRecord->pbrSeparateMaterialData
		&& !scalarRecord->hasPBRNormal && !scalarRecord->hasPBRORM
		&& explicitRecord != NULL && explicitRecord->pbrHasExplicitLegacyFallback
		&& explicitRecord->pbrUsesGeneratedLegacyFallback
		&& !explicitRecord->pbrUsesApproximateLegacyFallback
		&& unsupportedRecord != NULL
		&& unsupportedRecord->pbrFallbackReason == MATERIAL_RESOURCE_PBR_FALLBACK_UNSUPPORTED_WORKFLOW
		&& missingFallbackRecord != NULL && missingFallbackRecord->pbrLegacyFallbackMissing
		&& missingAlbedoRecord != NULL && !missingAlbedoRecord->hasPBRAlbedo
		&& missingAlbedoRecord->pbrFallbackReason == MATERIAL_RESOURCE_PBR_FALLBACK_MISSING_ALBEDO
		&& mutableImageRecord != NULL && mutableImageRecord->hasPBRAlbedo
		&& !mutableImageRecord->pbrResourceReady
		&& mutableImageRecord->pbrFallbackReason == MATERIAL_RESOURCE_PBR_FALLBACK_MISSING_IMAGE
		&& redundantExplicitRecord != NULL && redundantExplicitRecord->pbrHasExplicitLegacyFallback
		&& !redundantExplicitRecord->pbrUsesGeneratedLegacyFallback
		&& stats.pbrRecords == 9
		&& stats.classicRecords == 0
		&& stats.pbrResourceReadyRecords == 6
		&& stats.pbrModernReadyRecords == 0
		&& stats.pbrPackedMapRecords == 7
		&& stats.pbrSeparateMapRecords == 1
		&& stats.pbrAuthoredClassicFallbackRecords == 7
		&& stats.pbrExplicitGeneratedFallbackRecords == 1
		&& stats.pbrGeneratedFallbackRecords == 1
		&& stats.pbrApproximateFallbackRecords == 0
		&& stats.pbrMissingLegacyFallbackRecords == 1
		&& stats.pbrMissingAlbedoMapRecords == 1
		&& stats.pbrMissingNormalMapRecords == 1
		&& stats.pbrMissingORMMapRecords == 2
		&& stats.pbrFallbackMissingImage == 1
		&& stats.pbrFallbackRecords == 9
		&& pbrBindingsExcludedFromClassicTable
		&& stats.textureArrayTableDescriptors == 0;
	if ( !ok ) {
		common->Printf(
			"RendererMaterialResourceTable self-test failed: PBR contract added=%d/%d/%d/%d/%d/%d/%d/%d/%d record=%d resource=%d modern=%d fallback=%s records=%d packed=%d separate=%d missingFallback=%d mapsMissing=%d/%d/%d\n",
			added ? 1 : 0,
			separateAdded ? 1 : 0,
			scalarAdded ? 1 : 0,
			explicitAdded ? 1 : 0,
			unsupportedAdded ? 1 : 0,
			missingFallbackAdded ? 1 : 0,
			missingAlbedoAdded ? 1 : 0,
			mutableImageAdded ? 1 : 0,
			redundantExplicitAdded ? 1 : 0,
			record != NULL ? 1 : 0,
			record != NULL && record->pbrResourceReady ? 1 : 0,
			record != NULL && record->pbrModernReady ? 1 : 0,
			record != NULL ? MaterialResourcePBRFallbackReason_Name( record->pbrFallbackReason ) : "missing",
			stats.pbrRecords,
			stats.pbrPackedMapRecords,
			stats.pbrSeparateMapRecords,
			stats.pbrMissingLegacyFallbackRecords,
			stats.pbrMissingAlbedoMapRecords,
			stats.pbrMissingNormalMapRecords,
			stats.pbrMissingORMMapRecords );
	}
	// The table owns no declaration lifetime. Remove every retained synthetic
	// pointer and invalidate the hash before releasing the temporary material,
	// including failure exits that callers may inspect afterward.
	for ( int i = 0; i < stats.records; ++i ) {
		rg_materialResourceTable.records[i].material = NULL;
	}
	DeclManager_FreeAllocatedDecl( materialDecl );
	R_MaterialResourceTable_ResetFrameStats();
	rg_materialResourceTable.maxClassicTextureUnits = savedMaxClassicTextureUnits;
	if ( ok ) {
		common->Printf( "RendererMaterialResourceTable PBR contract self-test passed\n" );
	}
	return ok;
}

static bool R_MaterialResourceTable_RunSyntheticRecordSelfTest( void ) {
	R_MaterialResourceTable_ResetFrameStats();
	rg_materialResourceTable.stats.prepared = true;

	const rendererMaterialClass_t classes[] = {
		RENDER_MATERIAL_OPAQUE,
		RENDER_MATERIAL_PERFORATED,
		RENDER_MATERIAL_TRANSLUCENT,
		RENDER_MATERIAL_GUI,
		RENDER_MATERIAL_POST_PROCESS
	};
	for ( int i = 0; i < static_cast<int>( sizeof( classes ) / sizeof( classes[0] ) ); ++i ) {
		materialResourceRecord_t source;
		memset( &source, 0, sizeof( source ) );
		source.material = tr.defaultMaterial;
		source.diffuseImage = globalImages != NULL ? globalImages->defaultImage : NULL;
		source.normalImage = globalImages != NULL ? globalImages->flatNormalMap : NULL;
		source.specularImage = globalImages != NULL ? globalImages->whiteImage : NULL;
		source.resourceTableIndex = i;
		source.permutation.materialClass = classes[i];
		source.permutation.alphaMode = classes[i] == RENDER_MATERIAL_PERFORATED ? MC_PERFORATED : MC_OPAQUE;
		if ( !R_MaterialResourceTable_AddRecordFromSource( source, i, false ) ) {
			common->Printf( "RendererMaterialResourceTable self-test failed: synthetic class add failed\n" );
			return false;
		}
	}

	materialResourceRecord_t missingTextureSource;
	memset( &missingTextureSource, 0, sizeof( missingTextureSource ) );
	missingTextureSource.material = tr.defaultMaterial;
	missingTextureSource.resourceTableIndex = 100;
	missingTextureSource.permutation.materialClass = RENDER_MATERIAL_OPAQUE;
	missingTextureSource.permutation.alphaMode = MC_OPAQUE;
	if ( !R_MaterialResourceTable_AddRecordFromSource( missingTextureSource, 100, false ) ) {
		common->Printf( "RendererMaterialResourceTable self-test failed: missing-texture record add failed\n" );
		return false;
	}
	R_MaterialResourceTable_BuildTextureArrayTable();

	const materialResourceTableStats_t &stats = R_MaterialResourceTable_Stats();
	if ( stats.records != 6
		|| stats.classicRecords != 6
		|| stats.opaqueRecords != 2
		|| stats.perforatedRecords != 1
		|| stats.translucentRecords != 1
		|| stats.guiRecords != 1
		|| stats.postProcessRecords != 1
		|| stats.alphaTestRecords != 1
		|| stats.fallbackMissingImage <= 0 ) {
		common->Printf(
			"RendererMaterialResourceTable self-test failed: synthetic counts records=%d classic=%d opaque=%d perforated=%d translucent=%d gui=%d post=%d alpha=%d missingFallback=%d\n",
			stats.records,
			stats.classicRecords,
			stats.opaqueRecords,
			stats.perforatedRecords,
			stats.translucentRecords,
			stats.guiRecords,
			stats.postProcessRecords,
			stats.alphaTestRecords,
			stats.fallbackMissingImage );
		return false;
	}
	const materialResourceTableRecord_t *perforated = R_MaterialResourceTable_RecordForIndex( 1 );
	const materialResourceTableRecord_t *post = R_MaterialResourceTable_RecordForIndex( 4 );
	const materialResourceTableRecord_t *missing = R_MaterialResourceTable_RecordForIndex( 5 );
	if ( perforated == NULL || perforated->blendMode != MATERIAL_RESOURCE_BLEND_ALPHA_TEST || !perforated->alphaTest ) {
		common->Printf( "RendererMaterialResourceTable self-test failed: perforated classification mismatch\n" );
		return false;
	}
	if ( post == NULL || post->blendMode != MATERIAL_RESOURCE_BLEND_POST_PROCESS || !post->hasPostProcess ) {
		common->Printf( "RendererMaterialResourceTable self-test failed: post classification mismatch\n" );
		return false;
	}
	if ( missing == NULL || missing->fallbackReason != MATERIAL_RESOURCE_FALLBACK_MISSING_IMAGE ) {
		common->Printf( "RendererMaterialResourceTable self-test failed: missing-texture fallback mismatch\n" );
		return false;
	}
	if ( stats.textureArraysSupported && ( !stats.textureArrayTableReady || stats.textureArrayTableTextures <= 0 || stats.textureArrayTableDescriptors <= 0 ) ) {
		common->Printf(
			"RendererMaterialResourceTable self-test failed: texture-array table mismatch ready=%d textures=%d desc=%d overflow=%d\n",
			stats.textureArrayTableReady ? 1 : 0,
			stats.textureArrayTableTextures,
			stats.textureArrayTableDescriptors,
			stats.textureArrayTableOverflows );
		return false;
	}
	if ( !R_MaterialResourceTable_RunAmbientOverlayContractSelfTest() ) {
		return false;
	}
	return true;
}

bool RendererMaterialResourceTable_RunSelfTest( void ) {
	if ( !R_MaterialResourceTable_RunPBRContractSelfTest() ) {
		return false;
	}
	if ( !rg_materialResourceTable.stats.initialized ) {
		common->Printf( "RendererMaterialResourceTable full self-test skipped: material resource table uninitialized\n" );
		return true;
	}
	if ( !rg_materialResourceTable.stats.available ) {
		common->Printf( "RendererMaterialResourceTable full self-test skipped: scene packets unavailable\n" );
		return true;
	}

	if ( !R_MaterialResourceTable_RunSyntheticRecordSelfTest() ) {
		return false;
	}

	srfTriangles_t geo;
	memset( &geo, 0, sizeof( geo ) );
	geo.numVerts = 3;
	geo.numIndexes = 6;
	drawSurf_t drawSurfs[2];
	memset( drawSurfs, 0, sizeof( drawSurfs ) );
	for ( int i = 0; i < 2; ++i ) {
		drawSurfs[i].geo = &geo;
		if ( tr.defaultMaterial != NULL ) {
			drawSurfs[i].material = tr.defaultMaterial;
			drawSurfs[i].sort = tr.defaultMaterial->GetSort() + static_cast<float>( i ) * 0.000001f;
		}
	}
	drawSurf_t *drawSurfPtrs[2] = { &drawSurfs[0], &drawSurfs[1] };
	viewEntity_t viewEntity;
	memset( &viewEntity, 0, sizeof( viewEntity ) );
	drawSurfs[0].space = &viewEntity;
	drawSurfs[1].space = &viewEntity;
	viewDef_t worldView;
	memset( &worldView, 0, sizeof( worldView ) );
	worldView.viewEntitys = &viewEntity;
	worldView.drawSurfs = drawSurfPtrs;
	worldView.numDrawSurfs = 2;

	drawSurfsCommand_t drawCmd;
	memset( &drawCmd, 0, sizeof( drawCmd ) );
	drawCmd.commandId = RC_DRAW_VIEW;
	drawCmd.viewDef = &worldView;
	emptyCommand_t swapCmd;
	memset( &swapCmd, 0, sizeof( swapCmd ) );
	swapCmd.commandId = RC_SWAP_BUFFERS;
	drawCmd.next = &swapCmd.commandId;
	swapCmd.next = NULL;

	idScenePacketFrame packetFrame;
	R_ScenePackets_BuildLegacyCommandStream( reinterpret_cast<const emptyCommand_t *>( &drawCmd ), packetFrame );
	R_MaterialResourceTable_PrepareFrame( packetFrame );
	const materialResourceTableStats_t &stats = R_MaterialResourceTable_Stats();
	const int materialRecordCount = packetFrame.NumMaterialRecords();
	const int drawPacketCount = packetFrame.NumDrawPackets();
	const int expectedRecords = tr.defaultMaterial != NULL ? 1 : 0;
	if ( stats.sourceMaterialRecords != materialRecordCount
		|| stats.records != expectedRecords
		|| stats.classicRecords != expectedRecords
		|| stats.drawPacketReferences != ( tr.defaultMaterial != NULL ? drawPacketCount : 0 )
		|| stats.overflow ) {
		common->Printf(
			"RendererMaterialResourceTable self-test failed: packet build mismatch source=%d/%d records=%d classic=%d expected=%d drawRefs=%d overflow=%d\n",
			stats.sourceMaterialRecords,
			materialRecordCount,
			stats.records,
			stats.classicRecords,
			expectedRecords,
			stats.drawPacketReferences,
			stats.overflow ? 1 : 0 );
		return false;
	}
	if ( expectedRecords > 0 ) {
		const materialResourceTableRecord_t *record = R_MaterialResourceTable_RecordForIndex( 0 );
		if ( record == NULL || record->tableIndex != 0 || record->sourceMaterialRecordIndex != 0 || record->material != tr.defaultMaterial || record->materialClass == RENDER_MATERIAL_NONE ) {
			common->Printf( "RendererMaterialResourceTable self-test failed: packet record identity mismatch\n" );
			return false;
		}
	}

	common->Printf(
		"RendererMaterialResourceTable self-test passed (records=%d textures=%d fallback=%d missing=%d arrays=%d table=%d/%d views=%d bindless=%d/%d)\n",
		stats.records,
		stats.textureBindings,
		stats.fallbackRecords,
		stats.missingImages,
		stats.textureArrayDescriptors,
		stats.textureArrayTableTextures,
		stats.textureArrayTableCapacity,
		stats.textureViewDescriptors,
		stats.bindlessEnabled ? 1 : 0,
		stats.bindlessSupported ? 1 : 0 );
	return true;
}
