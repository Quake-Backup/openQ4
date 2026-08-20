// Copyright (C) 2026 DarkMatter Productions
//

#include "src/renderer/RendererContracts.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

static int failures = 0;

static void Check( bool condition, const char *message ) {
	if ( condition ) {
		return;
	}
	std::fprintf( stderr, "RendererContractsTest: %s\n", message );
	failures++;
}

static bool NearlyEqual( float a, float b ) {
	return std::fabs( a - b ) <= 0.00001f;
}

static void ExerciseMaterialPassContract( void ) {
	rendererMaterialPassList_t list;
	RendererContracts_ResetMaterialPassList( list );

	rendererMaterialPass_t first = RendererContracts_DefaultMaterialPass();
	first.sourceStageIndex = 3;
	first.kind = RENDERER_MATERIAL_PASS_SURFACE;
	first.textureSemantic = RENDERER_TEXTURE_DIFFUSE;
	first.textureResourceId = 0x101u;
	first.condition = RendererContracts_Register( 7 );
	for ( int i = 0; i < 4; ++i ) {
		first.color[ i ] = RendererContracts_Register( 10 + i );
	}
	for ( int i = 0; i < 6; ++i ) {
		first.textureMatrix[ i ] = RendererContracts_Register( 20 + i );
	}
	first.blend.enabled = true;
	first.blend.sourceColor = RENDERER_BLEND_SRC_ALPHA;
	first.blend.destinationColor = RENDERER_BLEND_ONE_MINUS_SRC_ALPHA;
	first.depth.testEnabled = true;
	first.depth.writeEnabled = false;
	first.depth.compareOperation = RENDERER_COMPARE_EQUAL;
	first.cull = RENDERER_CULL_FRONT;
	first.colorWriteMask = RENDERER_COLOR_WRITE_RED | RENDERER_COLOR_WRITE_GREEN;
	first.alphaTestEnabled = true;
	first.alphaTest = RendererContracts_Register( 31 );
	first.texgen = RENDERER_TEXGEN_SCREEN;
	first.vertexColor = RENDERER_VERTEX_COLOR_MODULATE;
	first.polygonOffsetEnabled = true;
	first.polygonOffsetFactor = RendererContracts_Register( 32 );
	first.polygonOffsetUnits = RendererContracts_Constant( 2.0f );
	first.programFamily = RENDERER_PROGRAM_CUSTOM;
	first.programKey = 0x33445566u;

	rendererMaterialPass_t second = RendererContracts_DefaultMaterialPass();
	second.sourceStageIndex = 9;
	second.textureSemantic = RENDERER_TEXTURE_DIFFUSE;
	second.textureResourceId = 0x202u;
	second.condition = RendererContracts_Register( 41 );

	Check( RendererContracts_AppendMaterialPass( list, first ),
		"first material pass must append" );
	Check( RendererContracts_AppendMaterialPass( list, second ),
		"repeated texture semantic must append" );
	Check( list.count == 2 && list.passes[ 0 ].order == 0 && list.passes[ 1 ].order == 1,
		"material pass insertion order must be stable" );
	Check( list.passes[ 0 ].textureSemantic == RENDERER_TEXTURE_DIFFUSE
			&& list.passes[ 1 ].textureSemantic == RENDERER_TEXTURE_DIFFUSE
			&& list.passes[ 0 ].textureResourceId == 0x101u
			&& list.passes[ 1 ].textureResourceId == 0x202u,
		"repeated semantics must remain distinct" );
	Check( list.passes[ 0 ].condition.index == 7
			&& list.passes[ 0 ].color[ 3 ].index == 13
			&& list.passes[ 0 ].textureMatrix[ 5 ].index == 25
			&& list.passes[ 0 ].alphaTest.index == 31
			&& list.passes[ 0 ].polygonOffsetFactor.index == 32,
		"material register references must survive append" );
	Check( list.passes[ 0 ].blend.enabled && !list.passes[ 0 ].depth.writeEnabled
			&& list.passes[ 0 ].cull == RENDERER_CULL_FRONT
			&& list.passes[ 0 ].colorWriteMask == 3u
			&& list.passes[ 0 ].texgen == RENDERER_TEXGEN_SCREEN
			&& list.passes[ 0 ].vertexColor == RENDERER_VERTEX_COLOR_MODULATE
			&& list.passes[ 0 ].polygonOffsetEnabled
			&& list.passes[ 0 ].programFamily == RENDERER_PROGRAM_CUSTOM,
		"material pass render state must survive append" );

	RendererContracts_ResetMaterialPassList( list );
	rendererMaterialPass_t bounded = RendererContracts_DefaultMaterialPass();
	bounded.sourceStageIndex = 0;
	for ( std::uint32_t i = 0; i < RENDERER_CONTRACT_MAX_MATERIAL_PASSES; ++i ) {
		Check( RendererContracts_AppendMaterialPass( list, bounded ),
			"bounded material list rejected an in-range pass" );
	}
	Check( !RendererContracts_AppendMaterialPass( list, bounded ) && list.overflowed
			&& list.count == RENDERER_CONTRACT_MAX_MATERIAL_PASSES,
		"bounded material list must fail closed at capacity" );
}

static void ExerciseClipSpaceContract( void ) {
	const rendererClipSpaceConvention_t gl = RendererContracts_GLClipSpace();
	const rendererClipSpaceConvention_t vk = RendererContracts_VulkanClipSpace();
	float identity[ 16 ];
	std::memset( identity, 0, sizeof( identity ) );
	identity[ 0 ] = identity[ 5 ] = identity[ 10 ] = identity[ 15 ] = 1.0f;

	float converted[ 16 ];
	Check( RendererContracts_ConvertClipMatrix( converted, identity, gl, vk ),
		"GL to Vulkan clip conversion failed" );
	Check( NearlyEqual( converted[ 10 ], 0.5f ) && NearlyEqual( converted[ 14 ], 0.5f )
			&& NearlyEqual( converted[ 0 ], 1.0f ) && NearlyEqual( converted[ 15 ], 1.0f ),
		"GL to Vulkan depth conversion must map z from [-w,w] to [0,w]" );
	Check( RendererContracts_ConvertClipMatrix( converted, converted, vk, gl ),
		"in-place Vulkan to GL clip conversion failed" );
	for ( int i = 0; i < 16; ++i ) {
		Check( NearlyEqual( converted[ i ], identity[ i ] ),
			"round-trip clip conversion changed the matrix" );
	}
	Check( RendererContracts_ConvertClipMatrix( converted, identity, gl, gl )
			&& std::memcmp( converted, identity, sizeof( identity ) ) == 0,
		"same-convention clip conversion must be an exact no-op" );

	rendererCanonicalViewport_t source = { 12.0f, 34.0f, 640.0f, 360.0f, 0.1f, 0.9f };
	rendererBackendViewport_t viewport;
	Check( RendererContracts_BuildViewport( viewport, source, 1080.0f, gl )
			&& NearlyEqual( viewport.y, 34.0f ) && NearlyEqual( viewport.height, 360.0f ),
		"GL viewport must preserve lower-left coordinates" );
	Check( RendererContracts_BuildViewport( viewport, source, 1080.0f, vk )
			&& NearlyEqual( viewport.y, 1046.0f ) && NearlyEqual( viewport.height, -360.0f )
			&& NearlyEqual( viewport.minDepth, 0.1f ) && NearlyEqual( viewport.maxDepth, 0.9f ),
		"Vulkan viewport must use the shared upper-left negative-height convention" );
}

static void ExerciseVertexLayoutContract( void ) {
	const rendererVertexLayoutDesc_t &legacy = RendererContracts_LegacyDrawVertLayout();
	Check( RendererContracts_ValidateVertexLayout( legacy ),
		"legacy idDrawVert layout must validate" );
	Check( legacy.bindingCount == 1 && legacy.bindings[ 0 ].stride == 64
			&& legacy.attributeCount == 6,
		"legacy idDrawVert descriptor shape mismatch" );
	const rendererVertexAttributeDesc_t *texcoord = RendererContracts_FindVertexAttribute(
		legacy, RENDERER_VERTEX_SEMANTIC_TEXCOORD0 );
	Check( texcoord != NULL && texcoord->format == RENDERER_VERTEX_FORMAT_FLOAT32X2
			&& texcoord->offset == 56,
		"legacy idDrawVert texcoord descriptor mismatch" );

	const rendererVertexLayoutDesc_t &skinned = RendererContracts_SkinnedDrawVertLayout();
	Check( RendererContracts_ValidateVertexLayout( skinned )
			&& skinned.bindingCount == 2 && skinned.bindings[ 1 ].stride == 32,
		"skinned vertex layout must expose its dedicated 32-byte binding" );
	const rendererVertexAttributeDesc_t *joints = RendererContracts_FindVertexAttribute(
		skinned, RENDERER_VERTEX_SEMANTIC_JOINT_INDICES );
	const rendererVertexAttributeDesc_t *weights = RendererContracts_FindVertexAttribute(
		skinned, RENDERER_VERTEX_SEMANTIC_JOINT_WEIGHTS );
	Check( joints != NULL && joints->binding == 1 && joints->offset == 0
			&& joints->format == RENDERER_VERTEX_FORMAT_UINT32X4,
		"skin joint indices must be full uint32x4 values" );
	Check( weights != NULL && weights->binding == 1 && weights->offset == 16
			&& weights->format == RENDERER_VERTEX_FORMAT_FLOAT32X4,
		"skin weights must be full float32x4 values" );

	rendererVertexLayoutDesc_t invalid = legacy;
	invalid.attributes[ 1 ].semantic = invalid.attributes[ 0 ].semantic;
	Check( !RendererContracts_ValidateVertexLayout( invalid ),
		"duplicate vertex semantics must fail validation" );
	invalid = legacy;
	invalid.attributes[ 5 ].offset = 60;
	Check( !RendererContracts_ValidateVertexLayout( invalid ),
		"out-of-bounds vertex attributes must fail validation" );
	invalid = legacy;
	invalid.attributes[ 2 ].offset = 8;
	Check( !RendererContracts_ValidateVertexLayout( invalid ),
		"overlapping vertex attributes must fail validation" );
}

static void ExerciseBufferHandleContract( void ) {
	const rendererBufferHandle_t handle = RendererContracts_MakeBufferHandle(
		RENDERER_BUFFER_KIND_VERTEX, RENDERER_BUFFER_LIFETIME_FRAME, 12, 7 );
	rendererBufferRecord_t record = { handle, 4096, 99 };
	rendererBufferSlice_t slice = { handle, 128, 512 };
	Check( RendererContracts_ValidateBufferSlice( slice, record,
			RENDERER_BUFFER_KIND_VERTEX, 99 ) == RENDERER_BUFFER_SLICE_VALID,
		"valid typed buffer slice was rejected" );

	slice.handle = RendererContracts_MakeBufferHandle(
		RENDERER_BUFFER_KIND_VERTEX, RENDERER_BUFFER_LIFETIME_FRAME, 12, 8 );
	Check( RendererContracts_ValidateBufferSlice( slice, record,
			RENDERER_BUFFER_KIND_VERTEX, 99 ) == RENDERER_BUFFER_SLICE_STALE_GENERATION,
		"stale buffer generation must fail validation" );

	slice.handle = RendererContracts_MakeBufferHandle(
		RENDERER_BUFFER_KIND_INDEX, RENDERER_BUFFER_LIFETIME_FRAME, 12, 7 );
	Check( RendererContracts_ValidateBufferSlice( slice, record,
			RENDERER_BUFFER_KIND_VERTEX, 99 ) == RENDERER_BUFFER_SLICE_WRONG_KIND,
		"wrong buffer kind must fail validation" );

	slice.handle = handle;
	Check( RendererContracts_ValidateBufferSlice( slice, record,
			RENDERER_BUFFER_KIND_VERTEX, 100 ) == RENDERER_BUFFER_SLICE_EXPIRED,
		"frame buffer slice must expire with its frame serial" );

	record.capacityBytes = ( std::numeric_limits<std::uint64_t>::max )();
	slice.offsetBytes = record.capacityBytes - 2;
	slice.sizeBytes = 8;
	Check( RendererContracts_ValidateBufferSlice( slice, record,
			RENDERER_BUFFER_KIND_VERTEX, 99 ) == RENDERER_BUFFER_SLICE_RANGE_OVERFLOW,
		"overflowing buffer slice range must fail without wrapping" );
}

int main() {
	Check( RendererContracts_RunSelfTest(),
		"shared runtime self-test must pass without a graphics device" );
	ExerciseMaterialPassContract();
	ExerciseClipSpaceContract();
	ExerciseVertexLayoutContract();
	ExerciseBufferHandleContract();
	if ( failures != 0 ) {
		std::fprintf( stderr, "RendererContractsTest: %d failure(s)\n", failures );
		return 1;
	}
	std::printf( "RendererContractsTest: PASS\n" );
	return 0;
}
