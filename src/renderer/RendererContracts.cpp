// Copyright (C) 2026 DarkMatter Productions
//

#include "RendererContracts.h"

#include <cmath>
#include <cstring>

static bool RendererContracts_RegisterRefValid( const rendererRegisterRef_t &reference ) {
	switch ( reference.source ) {
	case RENDERER_REGISTER_UNUSED:
		return true;
	case RENDERER_REGISTER_INDEX:
		return reference.index >= 0;
	case RENDERER_REGISTER_CONSTANT:
		return std::isfinite( reference.constantValue );
	default:
		return false;
	}
}

rendererRegisterRef_t RendererContracts_UnusedRegister( void ) {
	rendererRegisterRef_t result;
	std::memset( &result, 0, sizeof( result ) );
	result.source = RENDERER_REGISTER_UNUSED;
	result.index = -1;
	return result;
}

rendererRegisterRef_t RendererContracts_Register( std::int32_t index ) {
	rendererRegisterRef_t result;
	std::memset( &result, 0, sizeof( result ) );
	result.source = RENDERER_REGISTER_INDEX;
	result.index = index;
	return result;
}

rendererRegisterRef_t RendererContracts_Constant( float value ) {
	rendererRegisterRef_t result;
	std::memset( &result, 0, sizeof( result ) );
	result.source = RENDERER_REGISTER_CONSTANT;
	result.index = -1;
	result.constantValue = value;
	return result;
}

rendererMaterialPass_t RendererContracts_DefaultMaterialPass( void ) {
	rendererMaterialPass_t pass;
	std::memset( &pass, 0, sizeof( pass ) );
	pass.sourceStageIndex = -1;
	pass.kind = RENDERER_MATERIAL_PASS_SURFACE;
	pass.textureSemantic = RENDERER_TEXTURE_NONE;
	pass.condition = RendererContracts_Constant( 1.0f );
	for ( int i = 0; i < 4; ++i ) {
		pass.color[ i ] = RendererContracts_Constant( 1.0f );
	}
	pass.textureMatrix[ 0 ] = RendererContracts_Constant( 1.0f );
	pass.textureMatrix[ 1 ] = RendererContracts_Constant( 0.0f );
	pass.textureMatrix[ 2 ] = RendererContracts_Constant( 0.0f );
	pass.textureMatrix[ 3 ] = RendererContracts_Constant( 0.0f );
	pass.textureMatrix[ 4 ] = RendererContracts_Constant( 1.0f );
	pass.textureMatrix[ 5 ] = RendererContracts_Constant( 0.0f );
	pass.blend.enabled = false;
	pass.blend.sourceColor = RENDERER_BLEND_ONE;
	pass.blend.destinationColor = RENDERER_BLEND_ZERO;
	pass.blend.colorOperation = RENDERER_BLEND_OP_ADD;
	pass.blend.sourceAlpha = RENDERER_BLEND_ONE;
	pass.blend.destinationAlpha = RENDERER_BLEND_ZERO;
	pass.blend.alphaOperation = RENDERER_BLEND_OP_ADD;
	pass.depth.testEnabled = true;
	pass.depth.writeEnabled = true;
	pass.depth.compareOperation = RENDERER_COMPARE_LESS_OR_EQUAL;
	pass.cull = RENDERER_CULL_BACK;
	pass.colorWriteMask = RENDERER_COLOR_WRITE_RGBA;
	pass.alphaTest = RendererContracts_Constant( 0.5f );
	pass.texgen = RENDERER_TEXGEN_EXPLICIT;
	pass.vertexColor = RENDERER_VERTEX_COLOR_IGNORE;
	pass.polygonOffsetFactor = RendererContracts_Constant( 0.0f );
	pass.polygonOffsetUnits = RendererContracts_Constant( 0.0f );
	pass.programFamily = RENDERER_PROGRAM_FIXED;
	return pass;
}

void RendererContracts_ResetMaterialPassList( rendererMaterialPassList_t &list ) {
	std::memset( &list, 0, sizeof( list ) );
}

bool RendererContracts_ValidateMaterialPass( const rendererMaterialPass_t &pass ) {
	if ( pass.sourceStageIndex < 0
			|| pass.kind < RENDERER_MATERIAL_PASS_SURFACE
			|| pass.kind > RENDERER_MATERIAL_PASS_POST_PROCESS
			|| pass.textureSemantic < RENDERER_TEXTURE_NONE
			|| pass.textureSemantic > RENDERER_TEXTURE_CUSTOM
			|| pass.blend.sourceColor < RENDERER_BLEND_ZERO
			|| pass.blend.sourceColor > RENDERER_BLEND_ONE_MINUS_DST_ALPHA
			|| pass.blend.destinationColor < RENDERER_BLEND_ZERO
			|| pass.blend.destinationColor > RENDERER_BLEND_ONE_MINUS_DST_ALPHA
			|| pass.blend.sourceAlpha < RENDERER_BLEND_ZERO
			|| pass.blend.sourceAlpha > RENDERER_BLEND_ONE_MINUS_DST_ALPHA
			|| pass.blend.destinationAlpha < RENDERER_BLEND_ZERO
			|| pass.blend.destinationAlpha > RENDERER_BLEND_ONE_MINUS_DST_ALPHA
			|| pass.blend.colorOperation < RENDERER_BLEND_OP_ADD
			|| pass.blend.colorOperation > RENDERER_BLEND_OP_MAX
			|| pass.blend.alphaOperation < RENDERER_BLEND_OP_ADD
			|| pass.blend.alphaOperation > RENDERER_BLEND_OP_MAX
			|| pass.depth.compareOperation < RENDERER_COMPARE_NEVER
			|| pass.depth.compareOperation > RENDERER_COMPARE_ALWAYS
			|| pass.cull < RENDERER_CULL_NONE || pass.cull > RENDERER_CULL_BACK
			|| ( pass.colorWriteMask & ~static_cast<std::uint32_t>( RENDERER_COLOR_WRITE_RGBA ) ) != 0
			|| pass.texgen < RENDERER_TEXGEN_EXPLICIT || pass.texgen > RENDERER_TEXGEN_GLASS_WARP
			|| pass.vertexColor < RENDERER_VERTEX_COLOR_IGNORE
			|| pass.vertexColor > RENDERER_VERTEX_COLOR_INVERSE_MODULATE
			|| pass.programFamily < RENDERER_PROGRAM_FIXED
			|| pass.programFamily > RENDERER_PROGRAM_CUSTOM ) {
		return false;
	}
	if ( !RendererContracts_RegisterRefValid( pass.condition )
			|| !RendererContracts_RegisterRefValid( pass.alphaTest )
			|| !RendererContracts_RegisterRefValid( pass.polygonOffsetFactor )
			|| !RendererContracts_RegisterRefValid( pass.polygonOffsetUnits ) ) {
		return false;
	}
	for ( int i = 0; i < 4; ++i ) {
		if ( !RendererContracts_RegisterRefValid( pass.color[ i ] ) ) {
			return false;
		}
	}
	for ( int i = 0; i < 6; ++i ) {
		if ( !RendererContracts_RegisterRefValid( pass.textureMatrix[ i ] ) ) {
			return false;
		}
	}
	return true;
}

bool RendererContracts_AppendMaterialPass( rendererMaterialPassList_t &list,
		const rendererMaterialPass_t &pass ) {
	if ( !RendererContracts_ValidateMaterialPass( pass ) ) {
		return false;
	}
	if ( list.count >= RENDERER_CONTRACT_MAX_MATERIAL_PASSES ) {
		list.overflowed = true;
		return false;
	}
	list.passes[ list.count ] = pass;
	list.passes[ list.count ].order = list.count;
	list.count++;
	return true;
}

rendererClipSpaceConvention_t RendererContracts_GLClipSpace( void ) {
	rendererClipSpaceConvention_t convention;
	convention.depthRange = RENDERER_CLIP_DEPTH_NEGATIVE_ONE_TO_ONE;
	convention.framebufferOrigin = RENDERER_FRAMEBUFFER_ORIGIN_LOWER_LEFT;
	convention.viewportYAxis = RENDERER_VIEWPORT_Y_POSITIVE;
	convention.frontFace = RENDERER_FRONT_FACE_COUNTER_CLOCKWISE;
	return convention;
}

rendererClipSpaceConvention_t RendererContracts_VulkanClipSpace( void ) {
	rendererClipSpaceConvention_t convention;
	convention.depthRange = RENDERER_CLIP_DEPTH_ZERO_TO_ONE;
	convention.framebufferOrigin = RENDERER_FRAMEBUFFER_ORIGIN_UPPER_LEFT;
	convention.viewportYAxis = RENDERER_VIEWPORT_Y_NEGATIVE;
	convention.frontFace = RENDERER_FRONT_FACE_COUNTER_CLOCKWISE;
	return convention;
}

static bool RendererContracts_ClipConventionValid( const rendererClipSpaceConvention_t &convention ) {
	return convention.depthRange >= RENDERER_CLIP_DEPTH_NEGATIVE_ONE_TO_ONE
		&& convention.depthRange <= RENDERER_CLIP_DEPTH_ZERO_TO_ONE
		&& convention.framebufferOrigin >= RENDERER_FRAMEBUFFER_ORIGIN_LOWER_LEFT
		&& convention.framebufferOrigin <= RENDERER_FRAMEBUFFER_ORIGIN_UPPER_LEFT
		&& convention.viewportYAxis >= RENDERER_VIEWPORT_Y_POSITIVE
		&& convention.viewportYAxis <= RENDERER_VIEWPORT_Y_NEGATIVE
		&& convention.frontFace >= RENDERER_FRONT_FACE_COUNTER_CLOCKWISE
		&& convention.frontFace <= RENDERER_FRONT_FACE_CLOCKWISE;
}

bool RendererContracts_ConvertClipMatrix( float destination[ 16 ],
		const float source[ 16 ], const rendererClipSpaceConvention_t &sourceConvention,
		const rendererClipSpaceConvention_t &destinationConvention ) {
	if ( destination == NULL || source == NULL
			|| !RendererContracts_ClipConventionValid( sourceConvention )
			|| !RendererContracts_ClipConventionValid( destinationConvention ) ) {
		return false;
	}
	float canonical[ 16 ];
	std::memcpy( canonical, source, sizeof( canonical ) );
	std::memcpy( destination, canonical, sizeof( canonical ) );
	if ( sourceConvention.depthRange == destinationConvention.depthRange ) {
		return true;
	}
	for ( int column = 0; column < 4; ++column ) {
		const int z = column * 4 + 2;
		const int w = column * 4 + 3;
		if ( sourceConvention.depthRange == RENDERER_CLIP_DEPTH_NEGATIVE_ONE_TO_ONE ) {
			destination[ z ] = 0.5f * ( canonical[ z ] + canonical[ w ] );
		} else {
			destination[ z ] = 2.0f * canonical[ z ] - canonical[ w ];
		}
	}
	return true;
}

bool RendererContracts_BuildViewport( rendererBackendViewport_t &destination,
		const rendererCanonicalViewport_t &source, float framebufferHeight,
		const rendererClipSpaceConvention_t &destinationConvention ) {
	if ( !RendererContracts_ClipConventionValid( destinationConvention )
			|| !std::isfinite( source.x ) || !std::isfinite( source.y )
			|| !std::isfinite( source.width ) || !std::isfinite( source.height )
			|| !std::isfinite( source.minDepth ) || !std::isfinite( source.maxDepth )
			|| !std::isfinite( framebufferHeight ) || framebufferHeight <= 0.0f
			|| source.width <= 0.0f || source.height <= 0.0f
			|| source.minDepth < 0.0f || source.maxDepth > 1.0f
			|| source.minDepth > source.maxDepth ) {
		return false;
	}
	destination.x = source.x;
	destination.width = source.width;
	destination.minDepth = source.minDepth;
	destination.maxDepth = source.maxDepth;
	if ( destinationConvention.framebufferOrigin == RENDERER_FRAMEBUFFER_ORIGIN_UPPER_LEFT ) {
		if ( destinationConvention.viewportYAxis == RENDERER_VIEWPORT_Y_NEGATIVE ) {
			destination.y = framebufferHeight - source.y;
			destination.height = -source.height;
		} else {
			destination.y = framebufferHeight - source.y - source.height;
			destination.height = source.height;
		}
	} else if ( destinationConvention.viewportYAxis == RENDERER_VIEWPORT_Y_NEGATIVE ) {
		destination.y = source.y + source.height;
		destination.height = -source.height;
	} else {
		destination.y = source.y;
		destination.height = source.height;
	}
	return true;
}

static const rendererVertexLayoutDesc_t rendererLegacyDrawVertLayout = {
	1,
	6,
	{
		{ 0, 64, RENDERER_VERTEX_RATE_PER_VERTEX },
		{ 0, 0, RENDERER_VERTEX_RATE_PER_VERTEX },
		{ 0, 0, RENDERER_VERTEX_RATE_PER_VERTEX }
	},
	{
		{ RENDERER_VERTEX_SEMANTIC_POSITION, RENDERER_VERTEX_FORMAT_FLOAT32X3, 0, 0 },
		{ RENDERER_VERTEX_SEMANTIC_COLOR0, RENDERER_VERTEX_FORMAT_UNORM8X4, 0, 12 },
		{ RENDERER_VERTEX_SEMANTIC_NORMAL, RENDERER_VERTEX_FORMAT_FLOAT32X3, 0, 16 },
		{ RENDERER_VERTEX_SEMANTIC_TANGENT0, RENDERER_VERTEX_FORMAT_FLOAT32X3, 0, 32 },
		{ RENDERER_VERTEX_SEMANTIC_TANGENT1, RENDERER_VERTEX_FORMAT_FLOAT32X3, 0, 44 },
		{ RENDERER_VERTEX_SEMANTIC_TEXCOORD0, RENDERER_VERTEX_FORMAT_FLOAT32X2, 0, 56 }
	}
};

static const rendererVertexLayoutDesc_t rendererSkinnedDrawVertLayout = {
	2,
	8,
	{
		{ 0, 64, RENDERER_VERTEX_RATE_PER_VERTEX },
		{ 1, 32, RENDERER_VERTEX_RATE_PER_VERTEX },
		{ 0, 0, RENDERER_VERTEX_RATE_PER_VERTEX }
	},
	{
		{ RENDERER_VERTEX_SEMANTIC_POSITION, RENDERER_VERTEX_FORMAT_FLOAT32X3, 0, 0 },
		{ RENDERER_VERTEX_SEMANTIC_COLOR0, RENDERER_VERTEX_FORMAT_UNORM8X4, 0, 12 },
		{ RENDERER_VERTEX_SEMANTIC_NORMAL, RENDERER_VERTEX_FORMAT_FLOAT32X3, 0, 16 },
		{ RENDERER_VERTEX_SEMANTIC_TANGENT0, RENDERER_VERTEX_FORMAT_FLOAT32X3, 0, 32 },
		{ RENDERER_VERTEX_SEMANTIC_TANGENT1, RENDERER_VERTEX_FORMAT_FLOAT32X3, 0, 44 },
		{ RENDERER_VERTEX_SEMANTIC_TEXCOORD0, RENDERER_VERTEX_FORMAT_FLOAT32X2, 0, 56 },
		{ RENDERER_VERTEX_SEMANTIC_JOINT_INDICES, RENDERER_VERTEX_FORMAT_UINT32X4, 1, 0 },
		{ RENDERER_VERTEX_SEMANTIC_JOINT_WEIGHTS, RENDERER_VERTEX_FORMAT_FLOAT32X4, 1, 16 }
	}
};

const rendererVertexLayoutDesc_t &RendererContracts_LegacyDrawVertLayout( void ) {
	return rendererLegacyDrawVertLayout;
}

const rendererVertexLayoutDesc_t &RendererContracts_SkinnedDrawVertLayout( void ) {
	return rendererSkinnedDrawVertLayout;
}

std::uint32_t RendererContracts_VertexFormatSize( rendererVertexFormat_t format ) {
	switch ( format ) {
	case RENDERER_VERTEX_FORMAT_FLOAT32X2:
		return 8;
	case RENDERER_VERTEX_FORMAT_FLOAT32X3:
		return 12;
	case RENDERER_VERTEX_FORMAT_FLOAT32X4:
	case RENDERER_VERTEX_FORMAT_UINT32X4:
		return 16;
	case RENDERER_VERTEX_FORMAT_UNORM8X4:
		return 4;
	default:
		return 0;
	}
}

static const rendererVertexBindingDesc_t *RendererContracts_FindVertexBinding(
		const rendererVertexLayoutDesc_t &layout, std::uint32_t binding ) {
	for ( std::uint32_t i = 0; i < layout.bindingCount; ++i ) {
		if ( layout.bindings[ i ].binding == binding ) {
			return &layout.bindings[ i ];
		}
	}
	return NULL;
}

bool RendererContracts_ValidateVertexLayout( const rendererVertexLayoutDesc_t &layout ) {
	if ( layout.bindingCount == 0 || layout.bindingCount > RENDERER_CONTRACT_MAX_VERTEX_BINDINGS
			|| layout.attributeCount == 0 || layout.attributeCount > RENDERER_CONTRACT_MAX_VERTEX_ATTRIBUTES ) {
		return false;
	}
	for ( std::uint32_t i = 0; i < layout.bindingCount; ++i ) {
		const rendererVertexBindingDesc_t &binding = layout.bindings[ i ];
		if ( binding.stride == 0 || binding.inputRate < RENDERER_VERTEX_RATE_PER_VERTEX
				|| binding.inputRate > RENDERER_VERTEX_RATE_PER_INSTANCE ) {
			return false;
		}
		for ( std::uint32_t j = i + 1; j < layout.bindingCount; ++j ) {
			if ( binding.binding == layout.bindings[ j ].binding ) {
				return false;
			}
		}
	}
	for ( std::uint32_t i = 0; i < layout.attributeCount; ++i ) {
		const rendererVertexAttributeDesc_t &attribute = layout.attributes[ i ];
		if ( attribute.semantic < RENDERER_VERTEX_SEMANTIC_POSITION
				|| attribute.semantic > RENDERER_VERTEX_SEMANTIC_JOINT_WEIGHTS ) {
			return false;
		}
		const rendererVertexBindingDesc_t *binding =
			RendererContracts_FindVertexBinding( layout, attribute.binding );
		const std::uint32_t formatSize = RendererContracts_VertexFormatSize( attribute.format );
		if ( binding == NULL || formatSize == 0 || attribute.offset > binding->stride
				|| formatSize > binding->stride - attribute.offset ) {
			return false;
		}
		for ( std::uint32_t j = i + 1; j < layout.attributeCount; ++j ) {
			const rendererVertexAttributeDesc_t &other = layout.attributes[ j ];
			if ( attribute.semantic == other.semantic ) {
				return false;
			}
			if ( attribute.binding != other.binding ) {
				continue;
			}
			const std::uint32_t otherSize = RendererContracts_VertexFormatSize( other.format );
			if ( otherSize != 0 && attribute.offset < other.offset + otherSize
					&& other.offset < attribute.offset + formatSize ) {
				return false;
			}
		}
	}
	return true;
}

const rendererVertexAttributeDesc_t *RendererContracts_FindVertexAttribute(
		const rendererVertexLayoutDesc_t &layout, rendererVertexSemantic_t semantic ) {
	if ( layout.attributeCount > RENDERER_CONTRACT_MAX_VERTEX_ATTRIBUTES ) {
		return NULL;
	}
	for ( std::uint32_t i = 0; i < layout.attributeCount; ++i ) {
		if ( layout.attributes[ i ].semantic == semantic ) {
			return &layout.attributes[ i ];
		}
	}
	return NULL;
}

static const std::uint64_t RENDERER_BUFFER_SLOT_MASK = 0x0000000000ffffffULL;
static const std::uint64_t RENDERER_BUFFER_GENERATION_MASK = 0x0000ffffff000000ULL;
static const std::uint64_t RENDERER_BUFFER_KIND_MASK = 0x00ff000000000000ULL;
static const std::uint64_t RENDERER_BUFFER_LIFETIME_MASK = 0x0f00000000000000ULL;
static const std::uint64_t RENDERER_BUFFER_RESERVED_MASK = 0xf000000000000000ULL;

static_assert( sizeof( rendererBufferHandle_t ) == sizeof( std::uint64_t ),
	"renderer buffer handles must remain opaque 64-bit values" );

rendererBufferHandle_t RendererContracts_InvalidBufferHandle( void ) {
	rendererBufferHandle_t result;
	result.value = 0;
	return result;
}

rendererBufferHandle_t RendererContracts_MakeBufferHandle( rendererBufferKind_t kind,
		rendererBufferLifetime_t lifetime, std::uint32_t slot, std::uint32_t generation ) {
	rendererBufferHandle_t result = RendererContracts_InvalidBufferHandle();
	if ( kind < RENDERER_BUFFER_KIND_VERTEX || kind > RENDERER_BUFFER_KIND_JOINT_PALETTE
			|| lifetime < RENDERER_BUFFER_LIFETIME_STATIC || lifetime > RENDERER_BUFFER_LIFETIME_FRAME
			|| slot >= 0x00ffffffu || generation == 0 || generation > 0x00ffffffu ) {
		return result;
	}
	result.value = static_cast<std::uint64_t>( slot + 1 )
		| ( static_cast<std::uint64_t>( generation ) << 24 )
		| ( static_cast<std::uint64_t>( kind ) << 48 )
		| ( static_cast<std::uint64_t>( lifetime ) << 56 );
	return result;
}

bool RendererContracts_DecodeBufferHandle( rendererBufferHandle_t handle,
		rendererBufferKind_t &kind, rendererBufferLifetime_t &lifetime,
		std::uint32_t &slot, std::uint32_t &generation ) {
	if ( handle.value == 0 || ( handle.value & RENDERER_BUFFER_RESERVED_MASK ) != 0 ) {
		return false;
	}
	const std::uint32_t encodedSlot = static_cast<std::uint32_t>( handle.value & RENDERER_BUFFER_SLOT_MASK );
	generation = static_cast<std::uint32_t>( ( handle.value & RENDERER_BUFFER_GENERATION_MASK ) >> 24 );
	kind = static_cast<rendererBufferKind_t>( ( handle.value & RENDERER_BUFFER_KIND_MASK ) >> 48 );
	lifetime = static_cast<rendererBufferLifetime_t>( ( handle.value & RENDERER_BUFFER_LIFETIME_MASK ) >> 56 );
	if ( encodedSlot == 0 || generation == 0
			|| kind < RENDERER_BUFFER_KIND_VERTEX || kind > RENDERER_BUFFER_KIND_JOINT_PALETTE
			|| lifetime < RENDERER_BUFFER_LIFETIME_STATIC || lifetime > RENDERER_BUFFER_LIFETIME_FRAME ) {
		return false;
	}
	slot = encodedSlot - 1;
	return true;
}

rendererBufferSliceValidation_t RendererContracts_ValidateBufferSlice(
		const rendererBufferSlice_t &slice, const rendererBufferRecord_t &record,
		rendererBufferKind_t expectedKind, std::uint64_t currentFrameSerial ) {
	rendererBufferKind_t sliceKind;
	rendererBufferLifetime_t sliceLifetime;
	std::uint32_t sliceSlot;
	std::uint32_t sliceGeneration;
	rendererBufferKind_t recordKind;
	rendererBufferLifetime_t recordLifetime;
	std::uint32_t recordSlot;
	std::uint32_t recordGeneration;
	if ( !RendererContracts_DecodeBufferHandle( slice.handle, sliceKind, sliceLifetime,
			sliceSlot, sliceGeneration )
			|| !RendererContracts_DecodeBufferHandle( record.handle, recordKind, recordLifetime,
				recordSlot, recordGeneration ) ) {
		return RENDERER_BUFFER_SLICE_INVALID_HANDLE;
	}
	if ( sliceKind != expectedKind || recordKind != expectedKind ) {
		return RENDERER_BUFFER_SLICE_WRONG_KIND;
	}
	if ( sliceSlot != recordSlot ) {
		return RENDERER_BUFFER_SLICE_WRONG_SLOT;
	}
	if ( sliceGeneration != recordGeneration ) {
		return RENDERER_BUFFER_SLICE_STALE_GENERATION;
	}
	if ( sliceLifetime != recordLifetime ) {
		return RENDERER_BUFFER_SLICE_WRONG_LIFETIME;
	}
	if ( recordLifetime == RENDERER_BUFFER_LIFETIME_FRAME
			&& record.frameSerial != currentFrameSerial ) {
		return RENDERER_BUFFER_SLICE_EXPIRED;
	}
	if ( slice.sizeBytes == 0 ) {
		return RENDERER_BUFFER_SLICE_EMPTY;
	}
	if ( slice.offsetBytes > record.capacityBytes
			|| slice.sizeBytes > record.capacityBytes - slice.offsetBytes ) {
		return RENDERER_BUFFER_SLICE_RANGE_OVERFLOW;
	}
	return RENDERER_BUFFER_SLICE_VALID;
}

bool RendererContracts_RunSelfTest( void ) {
	rendererMaterialPassList_t passes;
	RendererContracts_ResetMaterialPassList( passes );
	rendererMaterialPass_t first = RendererContracts_DefaultMaterialPass();
	first.sourceStageIndex = 2;
	first.textureSemantic = RENDERER_TEXTURE_DIFFUSE;
	first.condition = RendererContracts_Register( 4 );
	rendererMaterialPass_t second = first;
	second.sourceStageIndex = 5;
	second.condition = RendererContracts_Register( 9 );
	if ( !RendererContracts_AppendMaterialPass( passes, first )
			|| !RendererContracts_AppendMaterialPass( passes, second )
			|| passes.count != 2 || passes.passes[ 0 ].order != 0
			|| passes.passes[ 1 ].order != 1
			|| passes.passes[ 0 ].textureSemantic != passes.passes[ 1 ].textureSemantic
			|| passes.passes[ 0 ].condition.index != 4
			|| passes.passes[ 1 ].condition.index != 9 ) {
		return false;
	}

	float identity[ 16 ];
	std::memset( identity, 0, sizeof( identity ) );
	identity[ 0 ] = identity[ 5 ] = identity[ 10 ] = identity[ 15 ] = 1.0f;
	float converted[ 16 ];
	if ( !RendererContracts_ConvertClipMatrix( converted, identity,
			RendererContracts_GLClipSpace(), RendererContracts_VulkanClipSpace() )
			|| converted[ 10 ] != 0.5f || converted[ 14 ] != 0.5f
			|| !RendererContracts_ConvertClipMatrix( converted, converted,
				RendererContracts_VulkanClipSpace(), RendererContracts_GLClipSpace() )
			|| std::memcmp( converted, identity, sizeof( identity ) ) != 0 ) {
		return false;
	}

	if ( !RendererContracts_ValidateVertexLayout( RendererContracts_LegacyDrawVertLayout() )
			|| !RendererContracts_ValidateVertexLayout( RendererContracts_SkinnedDrawVertLayout() ) ) {
		return false;
	}
	const rendererVertexAttributeDesc_t *joints = RendererContracts_FindVertexAttribute(
		RendererContracts_SkinnedDrawVertLayout(), RENDERER_VERTEX_SEMANTIC_JOINT_INDICES );
	const rendererVertexAttributeDesc_t *weights = RendererContracts_FindVertexAttribute(
		RendererContracts_SkinnedDrawVertLayout(), RENDERER_VERTEX_SEMANTIC_JOINT_WEIGHTS );
	if ( joints == NULL || weights == NULL || joints->binding != 1 || joints->offset != 0
			|| joints->format != RENDERER_VERTEX_FORMAT_UINT32X4
			|| weights->binding != 1 || weights->offset != 16
			|| weights->format != RENDERER_VERTEX_FORMAT_FLOAT32X4 ) {
		return false;
	}

	const rendererBufferHandle_t handle = RendererContracts_MakeBufferHandle(
		RENDERER_BUFFER_KIND_VERTEX, RENDERER_BUFFER_LIFETIME_FRAME, 3, 11 );
	rendererBufferRecord_t record = { handle, 1024, 17 };
	rendererBufferSlice_t slice = { handle, 64, 128 };
	if ( RendererContracts_ValidateBufferSlice( slice, record,
			RENDERER_BUFFER_KIND_VERTEX, 17 ) != RENDERER_BUFFER_SLICE_VALID ) {
		return false;
	}
	slice.handle = RendererContracts_MakeBufferHandle(
		RENDERER_BUFFER_KIND_VERTEX, RENDERER_BUFFER_LIFETIME_FRAME, 3, 12 );
	return RendererContracts_ValidateBufferSlice( slice, record,
		RENDERER_BUFFER_KIND_VERTEX, 17 ) == RENDERER_BUFFER_SLICE_STALE_GENERATION;
}
