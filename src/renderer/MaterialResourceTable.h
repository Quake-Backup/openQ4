// Copyright (C) 2004 Id Software, Inc.
//

#ifndef __MATERIAL_RESOURCE_TABLE_H__
#define __MATERIAL_RESOURCE_TABLE_H__

#include "RendererCaps.h"
#include "ScenePackets.h"

/*
===============================================================================

	Backend-facing material resource table.

	The legacy renderer still owns visible material execution.  This table is the
	stable per-frame bridge that modern passes use to reason about materials,
	texture slots, sampler state, shader-register ranges, and explicit fallback
	reasons without reaching back through idMaterial at submit time.

===============================================================================
*/

const int MATERIAL_RESOURCE_TABLE_MAX_RECORDS = SCENE_PACKET_MAX_MATERIAL_RECORDS;
// GL 3.3 guarantees at least 16 fragment texture units. Keep enough record
// slots for a dual-authored material's classic fallback plus the initial PBR
// semantics. The descriptor-array capacity remains 12 so units 12..15 stay
// reserved for the existing shadow contract.
const int MATERIAL_RESOURCE_TABLE_MAX_TEXTURE_BINDINGS = 16;
const int MATERIAL_RESOURCE_TABLE_TEXTURE_ARRAY_CAPACITY = 12;

enum materialResourceBlendMode_t {
	MATERIAL_RESOURCE_BLEND_OPAQUE = 0,
	MATERIAL_RESOURCE_BLEND_ALPHA_TEST,
	MATERIAL_RESOURCE_BLEND_BLEND,
	MATERIAL_RESOURCE_BLEND_ADD,
	MATERIAL_RESOURCE_BLEND_FILTER,
	MATERIAL_RESOURCE_BLEND_GUI,
	MATERIAL_RESOURCE_BLEND_POST_PROCESS
};

enum materialResourceTextureSemantic_t {
	MATERIAL_RESOURCE_TEXTURE_NONE = 0,
	MATERIAL_RESOURCE_TEXTURE_BUMP,
	MATERIAL_RESOURCE_TEXTURE_DIFFUSE,
	MATERIAL_RESOURCE_TEXTURE_SPECULAR,
	MATERIAL_RESOURCE_TEXTURE_EMISSIVE,
	MATERIAL_RESOURCE_TEXTURE_GUI,
	MATERIAL_RESOURCE_TEXTURE_POST_PROCESS,
	MATERIAL_RESOURCE_TEXTURE_ALBEDO,
	MATERIAL_RESOURCE_TEXTURE_NORMAL,
	MATERIAL_RESOURCE_TEXTURE_ORM,
	MATERIAL_RESOURCE_TEXTURE_METALLIC,
	MATERIAL_RESOURCE_TEXTURE_ROUGHNESS,
	MATERIAL_RESOURCE_TEXTURE_AO,
	MATERIAL_RESOURCE_TEXTURE_EMISSIVE_PBR,
	MATERIAL_RESOURCE_TEXTURE_COUNT
};

enum materialResourcePBRFallbackReason_t {
	MATERIAL_RESOURCE_PBR_FALLBACK_NONE = 0,
	MATERIAL_RESOURCE_PBR_FALLBACK_DISABLED,
	MATERIAL_RESOURCE_PBR_FALLBACK_UNSUPPORTED_WORKFLOW,
	MATERIAL_RESOURCE_PBR_FALLBACK_UNSUPPORTED_MATERIAL_CLASS,
	MATERIAL_RESOURCE_PBR_FALLBACK_MISSING_ALBEDO,
	MATERIAL_RESOURCE_PBR_FALLBACK_MISSING_NORMAL_FORMAT,
	MATERIAL_RESOURCE_PBR_FALLBACK_CONFLICTING_LAYOUT,
	MATERIAL_RESOURCE_PBR_FALLBACK_MISSING_IMAGE,
	MATERIAL_RESOURCE_PBR_FALLBACK_CLASSIC_FEATURE,
	MATERIAL_RESOURCE_PBR_FALLBACK_TOO_MANY_TEXTURES,
	MATERIAL_RESOURCE_PBR_FALLBACK_SHADER_PATH_UNAVAILABLE
};

enum materialResourceSortGroup_t {
	MATERIAL_RESOURCE_SORT_UNKNOWN = 0,
	MATERIAL_RESOURCE_SORT_SUBVIEW,
	MATERIAL_RESOURCE_SORT_GUI,
	MATERIAL_RESOURCE_SORT_OPAQUE,
	MATERIAL_RESOURCE_SORT_DECAL,
	MATERIAL_RESOURCE_SORT_TRANSLUCENT,
	MATERIAL_RESOURCE_SORT_POST_PROCESS
};

enum materialResourceFallbackReason_t {
	MATERIAL_RESOURCE_FALLBACK_NONE = 0,
	MATERIAL_RESOURCE_FALLBACK_MISSING_MATERIAL,
	MATERIAL_RESOURCE_FALLBACK_NO_DRAW_STAGES,
	MATERIAL_RESOURCE_FALLBACK_MISSING_IMAGE,
	MATERIAL_RESOURCE_FALLBACK_CUSTOM_PROGRAM,
	MATERIAL_RESOURCE_FALLBACK_CUSTOM_GLSL,
	MATERIAL_RESOURCE_FALLBACK_DYNAMIC_IMAGE,
	MATERIAL_RESOURCE_FALLBACK_CURRENT_RENDER_IMAGE,
	MATERIAL_RESOURCE_FALLBACK_SCREEN_TEXGEN,
	MATERIAL_RESOURCE_FALLBACK_SKY_TEXGEN,
	MATERIAL_RESOURCE_FALLBACK_UNSUPPORTED_TEXGEN,
	MATERIAL_RESOURCE_FALLBACK_NEEDS_CURRENT_RENDER,
	MATERIAL_RESOURCE_FALLBACK_STAGE_CONDITION,
	MATERIAL_RESOURCE_FALLBACK_STAGE_COLOR,
	MATERIAL_RESOURCE_FALLBACK_TEXTURE_MATRIX,
	MATERIAL_RESOURCE_FALLBACK_VERTEX_COLOR,
	MATERIAL_RESOURCE_FALLBACK_POLYGON_OFFSET,
	MATERIAL_RESOURCE_FALLBACK_TOO_MANY_TEXTURES
};

enum materialResourceFallbackFlags_t {
	MATERIAL_RESOURCE_FALLBACK_FLAG_MISSING_MATERIAL = 1 << 0,
	MATERIAL_RESOURCE_FALLBACK_FLAG_NO_DRAW_STAGES = 1 << 1,
	MATERIAL_RESOURCE_FALLBACK_FLAG_MISSING_IMAGE = 1 << 2,
	MATERIAL_RESOURCE_FALLBACK_FLAG_CUSTOM_PROGRAM = 1 << 3,
	MATERIAL_RESOURCE_FALLBACK_FLAG_DYNAMIC_IMAGE = 1 << 4,
	MATERIAL_RESOURCE_FALLBACK_FLAG_UNSUPPORTED_TEXGEN = 1 << 5,
	MATERIAL_RESOURCE_FALLBACK_FLAG_NEEDS_CURRENT_RENDER = 1 << 6,
	MATERIAL_RESOURCE_FALLBACK_FLAG_TOO_MANY_TEXTURES = 1 << 7,
	MATERIAL_RESOURCE_FALLBACK_FLAG_CUSTOM_GLSL = 1 << 8,
	MATERIAL_RESOURCE_FALLBACK_FLAG_CURRENT_RENDER_IMAGE = 1 << 9,
	MATERIAL_RESOURCE_FALLBACK_FLAG_SCREEN_TEXGEN = 1 << 10,
	MATERIAL_RESOURCE_FALLBACK_FLAG_SKY_TEXGEN = 1 << 11,
	MATERIAL_RESOURCE_FALLBACK_FLAG_STAGE_CONDITION = 1 << 12,
	MATERIAL_RESOURCE_FALLBACK_FLAG_TEXTURE_MATRIX = 1 << 13,
	MATERIAL_RESOURCE_FALLBACK_FLAG_VERTEX_COLOR = 1 << 14,
	MATERIAL_RESOURCE_FALLBACK_FLAG_POLYGON_OFFSET = 1 << 15,
	MATERIAL_RESOURCE_FALLBACK_FLAG_STAGE_COLOR = 1 << 16
};

typedef struct materialResourceTextureBinding_s {
	materialResourceTextureSemantic_t	semantic;
	const idImage						*image;
	unsigned int						textureHandle;
	textureFilter_t						filter;
	textureRepeat_t						repeat;
	int									classicUnit;
	int									stageIndex;
	int									drawStateBits;
	int									conditionRegister;
	bool								hasConditionRegister;
	bool								hasAlphaTest;
	int									alphaTestMode;
	int									alphaTestRegister;
	int									stageRegisterStart;
	int									stageRegisterCount;
	int									texgen;
	int									vertexColorMode;
	float								privatePolygonOffset;
	int									colorRegisters[4];
	int									matrixRegisters[2][3];
	bool								hasTextureMatrix;
	bool								blendEnabled;
	bool								depthWrite;
	bool								colorMasked;
	bool								loaded;
	bool								defaulted;
	bool								missing;
	bool								textureArrayCandidate;
	int									textureArrayLayer;
	bool								textureViewCandidate;
	unsigned int						textureViewHandle;
	bool								bindlessSupported;
	bool								bindlessEnabled;
	unsigned long long					bindlessHandle;
	char								debugName[96];
} materialResourceTextureBinding_t;

typedef struct materialResourceTableRecord_s {
	int									tableIndex;
	int									sourceMaterialRecordIndex;
	int									materialId;
	const idMaterial						*material;
	char								materialName[128];
	rendererMaterialClass_t				materialClass;
	materialResourceBlendMode_t			blendMode;
	materialResourceSortGroup_t			sortGroup;
	materialResourceFallbackReason_t		fallbackReason;
	unsigned int						fallbackFlags;
	float								sortValue;
	int									cullType;
	int									registerStart;
	int									registerCount;
	int									stageRegisterStart;
	int									stageRegisterCount;
	int									stageCount;
	int									evaluatedStageCount;
	int									additiveStageCount;
	int									filterStageCount;
	int									blendStageCount;
	bool								drawn;
	bool								receivesLighting;
	bool								castsShadow;
	bool								twoSided;
	bool								shouldCreateBackSides;
	bool								alphaTest;
	int									alphaTestMode;
	int									alphaTestRegister;
	bool								needsCurrentRender;
	bool								hasSceneCaptureImage;
	bool								hasGui;
	bool								hasSubview;
	bool								hasBump;
	bool								hasDiffuse;
	bool								hasSpecular;
	bool								hasEmissive;
	bool								hasPostProcess;
	bool								hasPBR;
	bool								pbrResourceReady;
	bool								pbrModernReady;
	int								pbrWorkflow;
	int								pbrNormalFormat;
	materialResourcePBRFallbackReason_t pbrFallbackReason;
	bool								hasPBRAlbedo;
	bool								hasPBRNormal;
	bool								hasPBRORM;
	bool								hasPBRMetallic;
	bool								hasPBRRoughness;
	bool								hasPBRAO;
	bool								hasPBREmissive;
	bool								pbrPackedMaterialData;
	bool								pbrSeparateMaterialData;
	bool								pbrHasAuthoredClassicFallback;
	bool								pbrHasExplicitLegacyFallback;
	bool								pbrUsesGeneratedLegacyFallback;
	bool								pbrUsesApproximateLegacyFallback;
	bool								pbrLegacyFallbackMissing;
	int								pbrMetallicRegister;
	int								pbrRoughnessRegister;
	int								pbrAORegister;
	int								pbrNormalScaleRegister;
	int								pbrEmissiveColorRegisters[3];
	bool								hasConditionRegisters;
	bool								hasTextureMatrix;
	bool								hasVertexColor;
	bool								hasDynamicImage;
	bool								hasScreenTexgen;
	bool								hasSkyTexgen;
	bool								hasCustomProgram;
	bool								hasCustomGLSL;
	bool								hasPrivatePolygonOffset;
	bool								hasMaterialPolygonOffset;
	float								polygonOffset;
	bool								shadowCasterSupported;
	bool								shadowAlphaTest;
	int									shadowAlphaBindingIndex;
	int									shadowAlphaTestMode;
	int									shadowAlphaTestRegister;
	bool								shadowUsesTextureMatrix;
	bool								shadowUsesVertexColor;
	unsigned int						shadowFallbackFlags;
	bool								hasDefaultedImage;
	bool								hasMissingImage;
	unsigned int						textureSemanticMask;
	unsigned int						loadedTextureSemanticMask;
	unsigned int						renderableColorTextureMask;
	int									semanticBindingIndex[MATERIAL_RESOURCE_TEXTURE_COUNT];
	int									textureBindingCount;
	materialResourceTextureBinding_t	textures[MATERIAL_RESOURCE_TABLE_MAX_TEXTURE_BINDINGS];
} materialResourceTableRecord_t;

typedef struct materialResourceTableStats_s {
	bool	initialized;
	bool	available;
	bool	prepared;
	bool	overflow;
	bool	bindlessSupported;
	bool	bindlessEnabled;
	bool	textureArraysSupported;
	bool	textureViewsSupported;
	bool	textureArrayTableReady;
	int		sourceMaterialRecords;
	int		drawPacketReferences;
	int		records;
	int		classicRecords;
	int		opaqueRecords;
	int		perforatedRecords;
	int		translucentRecords;
	int		guiRecords;
	int		subviewRecords;
	int		postProcessRecords;
	int		shadowOnlyRecords;
	int		alphaTestRecords;
	int		blendRecords;
	int		textureBindings;
	int		classicTextureBindings;
	int		textureArrayDescriptors;
	int		textureArrayTableCapacity;
	int		textureArrayTableTextures;
	int		textureArrayTableDescriptors;
	int		textureArrayTableOverflows;
	int		textureViewDescriptors;
	int		bindlessDescriptors;
	int		missingImages;
	int		defaultedImages;
	int		unsupportedFeatures;
	int		fallbackRecords;
	int		fallbackMissingMaterial;
	int		fallbackNoDrawStages;
	int		fallbackMissingImage;
	int		fallbackCustomProgram;
	int		fallbackCustomGLSL;
	int		fallbackDynamicImage;
	int		fallbackCurrentRenderImage;
	int		fallbackScreenTexgen;
	int		fallbackSkyTexgen;
	int		fallbackUnsupportedTexgen;
	int		fallbackNeedsCurrentRender;
	int		fallbackStageCondition;
	int		fallbackStageColor;
	int		fallbackTextureMatrix;
	int		fallbackVertexColor;
	int		fallbackPolygonOffset;
	int		fallbackTooManyTextures;
	int		pbrRecords;
	int		pbrResourceReadyRecords;
	int		pbrModernReadyRecords;
	int		pbrPackedMapRecords;
	int		pbrSeparateMapRecords;
	int		pbrAuthoredClassicFallbackRecords;
	int		pbrExplicitGeneratedFallbackRecords;
	int		pbrGeneratedFallbackRecords;
	int		pbrApproximateFallbackRecords;
	int		pbrMissingLegacyFallbackRecords;
	int		pbrMissingAlbedoMapRecords;
	int		pbrMissingNormalMapRecords;
	int		pbrMissingORMMapRecords;
	int		pbrFallbackRecords;
	int		pbrFallbackDisabled;
	int		pbrFallbackUnsupportedWorkflow;
	int		pbrFallbackUnsupportedMaterialClass;
	int		pbrFallbackMissingAlbedo;
	int		pbrFallbackMissingNormalFormat;
	int		pbrFallbackConflictingLayout;
	int		pbrFallbackMissingImage;
	int		pbrFallbackClassicFeature;
	int		pbrFallbackTooManyTextures;
	int		pbrFallbackShaderPathUnavailable;
	int		debugStringTruncations;
	char	debugStringTruncationSource[64];
	char	lastFailure[96];
} materialResourceTableStats_t;

void R_MaterialResourceTable_Init( const renderBackendCaps_t &caps, const renderFeatureSet_t &features );
void R_MaterialResourceTable_Shutdown( void );
void R_MaterialResourceTable_PrepareFrame( const idScenePacketFrame &packetFrame );
const materialResourceTableStats_t &R_MaterialResourceTable_Stats( void );
const materialResourceTableRecord_t *R_MaterialResourceTable_RecordForIndex( int tableIndex );
const materialResourceTableRecord_t *R_MaterialResourceTable_FindRecordForMaterial( const idMaterial *material );
bool R_MaterialResourceTable_ClassicModernPathEligible( const materialResourceTableRecord_t &record );
const unsigned int *R_MaterialResourceTable_TextureArrayTable( int &count );
int R_MaterialResourceTable_TextureArrayTableIndexForHandle( unsigned int textureHandle );
const char *MaterialResourceBlendMode_Name( materialResourceBlendMode_t blendMode );
const char *MaterialResourceTextureSemantic_Name( materialResourceTextureSemantic_t semantic );
unsigned int MaterialResourceTextureSemantic_Bit( materialResourceTextureSemantic_t semantic );
const materialResourceTextureBinding_t *R_MaterialResourceTable_TextureBindingForSemantic( const materialResourceTableRecord_t &record, materialResourceTextureSemantic_t semantic );
const char *MaterialResourceFallbackReason_Name( materialResourceFallbackReason_t reason );
const char *MaterialResourcePBRFallbackReason_Name( materialResourcePBRFallbackReason_t reason );
void R_MaterialResourceTable_PrintGfxInfo( void );
void R_MaterialResourceTable_DumpLatest( void );
bool RendererMaterialResourceTable_RunSelfTest( void );

#endif /* !__MATERIAL_RESOURCE_TABLE_H__ */
