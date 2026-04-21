/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#include "tr_local.h"
#include "Model_local.h"
#include "../idlib/LexerFactory.h"

rvRenderModelMD5R *rvRenderModelMD5R::modelList = NULL;

/*
===========================
R_MD5R_ParseFlexibleVec3

Retail MD5R files use plain numeric rows in several places, but some community
tools emit parenthesized vectors. Accept either form so the metadata loader
stays tolerant while the full packed-mesh runtime is still in progress.
===========================
*/
static void R_MD5R_ParseFlexibleVec3( Lexer &parser, idVec3 &value ) {
	if ( parser.PeekTokenString( "(" ) ) {
		parser.Parse1DMatrix( 3, value.ToFloatPtr() );
		return;
	}

	value.x = parser.ParseFloat();
	value.y = parser.ParseFloat();
	value.z = parser.ParseFloat();
}

/*
===========================
R_MD5R_ParseFlexibleBounds
===========================
*/
static void R_MD5R_ParseFlexibleBounds( Lexer &parser, idBounds &bounds ) {
	idVec3 mins;
	idVec3 maxs;

	R_MD5R_ParseFlexibleVec3( parser, mins );
	R_MD5R_ParseFlexibleVec3( parser, maxs );
	bounds[ 0 ] = mins;
	bounds[ 1 ] = maxs;
}

/*
===========================
R_MD5R_CalcMeshGeometryProfile

Retail rvMesh tracks aggregate geometry counts across prim batches. OpenQ4 uses
the same accounting now so later rvMesh porting can plug into already-populated
metadata instead of reparsing the MD5R text file.
===========================
*/
static void R_MD5R_CalcMeshGeometryProfile( rvMD5RMesh &mesh ) {
	mesh.numSilTraceVertices = 0;
	mesh.numSilTraceIndices = 0;
	mesh.numSilTracePrimitives = 0;
	mesh.numSilEdges = 0;
	mesh.numDrawVertices = 0;
	mesh.numDrawIndices = 0;
	mesh.numDrawPrimitives = 0;
	mesh.numTransforms = 0;

	for ( int i = 0; i < mesh.primBatches.Num(); ++i ) {
		const rvMD5RPrimBatch &primBatch = mesh.primBatches[ i ];

		if ( primBatch.hasSilTraceGeoSpec ) {
			mesh.numSilTraceVertices += primBatch.silTraceGeoSpec.vertexCount;
			mesh.numSilTraceIndices += primBatch.silTraceGeoSpec.primitiveCount * 3;
			mesh.numSilTracePrimitives += primBatch.silTraceGeoSpec.primitiveCount;
		}

		mesh.numSilEdges += primBatch.silEdgeCount;

		if ( primBatch.hasDrawGeoSpec ) {
			mesh.numDrawVertices += primBatch.drawGeoSpec.vertexCount;
			mesh.numDrawIndices += primBatch.drawGeoSpec.primitiveCount * 3;
			mesh.numDrawPrimitives += primBatch.drawGeoSpec.primitiveCount;
		}

		mesh.numTransforms += primBatch.numTransforms;
	}
}

/*
============================
rvRenderModelMD5R::RemoveFromList
============================
*/
void rvRenderModelMD5R::RemoveFromList( rvRenderModelMD5R &model ) {
	rvRenderModelMD5R *previous = NULL;
	for ( rvRenderModelMD5R *current = modelList; current != NULL; current = current->next ) {
		if ( current != &model ) {
			previous = current;
			continue;
		}

		if ( previous != NULL ) {
			previous->next = model.next;
		} else {
			modelList = model.next;
		}
		model.next = NULL;
		return;
	}
}

/*
========================
rvRenderModelMD5R::rvRenderModelMD5R
========================
*/
rvRenderModelMD5R::rvRenderModelMD5R() :
	md5rVersion( 0 ),
	metadataOnly( false ),
	geometrySectionsSkipped( false ),
	hasSky( false ),
	source( MD5R_SOURCE_FILE ),
	next( modelList ) {
	modelList = this;
}

/*
========================
rvRenderModelMD5R::~rvRenderModelMD5R
========================
*/
rvRenderModelMD5R::~rvRenderModelMD5R() {
	RemoveFromList( *this );
}

/*
========================
rvRenderModelMD5R::InitFromFile
========================
*/
void rvRenderModelMD5R::InitFromFile( const char *fileName ) {
	idRenderModelStatic::InitEmpty( fileName );
	source = MD5R_SOURCE_FILE;
	reloadable = true;
	LoadModel();
}

/*
========================
rvRenderModelMD5R::ParseVertexBuffer
========================
*/
void rvRenderModelMD5R::ParseVertexBuffer( Lexer &parser, rvMD5RVertexBufferDesc &vertexBuffer ) {
	idToken token;

	vertexBuffer = rvMD5RVertexBufferDesc();

	parser.ExpectTokenString( "{" );
	while ( parser.ReadToken( &token ) ) {
		if ( token.Icmp( "VertexFormat" ) == 0 ) {
			vertexBuffer.hasVertexFormat = true;
			if ( !parser.SkipBracedSection() ) {
				parser.Error( "Malformed VertexFormat block" );
			}
			continue;
		}

		if ( token.Icmp( "LoadVertexFormat" ) == 0 ) {
			vertexBuffer.hasLoadVertexFormat = true;
			if ( !parser.SkipBracedSection() ) {
				parser.Error( "Malformed LoadVertexFormat block" );
			}
			continue;
		}

		if ( token.Icmp( "SystemMemory" ) == 0 ) {
			vertexBuffer.systemMemory = true;
			continue;
		}

		if ( token.Icmp( "VideoMemory" ) == 0 ) {
			vertexBuffer.videoMemory = true;
			continue;
		}

		if ( token.Icmp( "SoA" ) == 0 ) {
			vertexBuffer.soA = true;
			continue;
		}

		if ( token.Icmp( "Vertex" ) == 0 ) {
			break;
		}

		parser.Error( "Unexpected token '%s' in VertexBuffer", token.c_str() );
	}

	if ( !vertexBuffer.hasVertexFormat ) {
		parser.Error( "Expected VertexFormat block in VertexBuffer" );
	}

	if ( vertexBuffer.soA && vertexBuffer.hasLoadVertexFormat ) {
		parser.Error( "SoA is currently not supported with LoadVertexFormat" );
	}

	if ( !vertexBuffer.systemMemory && !vertexBuffer.videoMemory ) {
		vertexBuffer.videoMemory = true;
	}

	parser.ExpectTokenString( "[" );
	vertexBuffer.numVertices = parser.ParseInt();
	parser.ExpectTokenString( "]" );
	if ( vertexBuffer.numVertices <= 0 ) {
		parser.Error( "Invalid vertex count" );
	}

	parser.ExpectTokenString( "{" );
	if ( !parser.SkipBracedSection( false ) ) {
		parser.Error( "Malformed Vertex data block" );
	}
	parser.ExpectTokenString( "}" );
}

/*
========================
rvRenderModelMD5R::ParseVertexBuffers
========================
*/
void rvRenderModelMD5R::ParseVertexBuffers( Lexer &parser ) {
	parser.ExpectTokenString( "[" );
	const int numVertexBuffers = parser.ParseInt();
	parser.ExpectTokenString( "]" );
	parser.ExpectTokenString( "{" );

	if ( numVertexBuffers < 0 ) {
		parser.Error( "Invalid MD5R vertex-buffer count %d", numVertexBuffers );
	}

	vertexBuffers.SetNum( numVertexBuffers );
	for ( int i = 0; i < numVertexBuffers; ++i ) {
		parser.ExpectTokenString( "VertexBuffer" );
		ParseVertexBuffer( parser, vertexBuffers[ i ] );
	}

	parser.ExpectTokenString( "}" );
}

/*
========================
rvRenderModelMD5R::ParseIndexBuffer
========================
*/
void rvRenderModelMD5R::ParseIndexBuffer( Lexer &parser, rvMD5RIndexBufferDesc &indexBuffer ) {
	idToken token;

	indexBuffer = rvMD5RIndexBufferDesc();

	parser.ExpectTokenString( "{" );
	while ( parser.ReadToken( &token ) ) {
		if ( token.Icmp( "SystemMemory" ) == 0 ) {
			indexBuffer.systemMemory = true;
			continue;
		}

		if ( token.Icmp( "VideoMemory" ) == 0 ) {
			indexBuffer.videoMemory = true;
			continue;
		}

		if ( token.Icmp( "BitDepth" ) == 0 ) {
			indexBuffer.bitDepth = parser.ParseInt();
			continue;
		}

		if ( token.Icmp( "Index" ) == 0 ) {
			break;
		}

		parser.Error( "Unexpected token '%s' in IndexBuffer", token.c_str() );
	}

	if ( !indexBuffer.systemMemory && !indexBuffer.videoMemory ) {
		indexBuffer.videoMemory = true;
	}

	parser.ExpectTokenString( "[" );
	indexBuffer.numIndices = parser.ParseInt();
	parser.ExpectTokenString( "]" );
	if ( indexBuffer.numIndices <= 0 ) {
		parser.Error( "Invalid index count" );
	}

	parser.ExpectTokenString( "{" );
	if ( !parser.SkipBracedSection( false ) ) {
		parser.Error( "Malformed Index data block" );
	}
	parser.ExpectTokenString( "}" );
}

/*
========================
rvRenderModelMD5R::ParseIndexBuffers
========================
*/
void rvRenderModelMD5R::ParseIndexBuffers( Lexer &parser ) {
	parser.ExpectTokenString( "[" );
	const int numIndexBuffers = parser.ParseInt();
	parser.ExpectTokenString( "]" );
	parser.ExpectTokenString( "{" );

	if ( numIndexBuffers < 0 ) {
		parser.Error( "Invalid MD5R index-buffer count %d", numIndexBuffers );
	}

	indexBuffers.SetNum( numIndexBuffers );
	for ( int i = 0; i < numIndexBuffers; ++i ) {
		parser.ExpectTokenString( "IndexBuffer" );
		ParseIndexBuffer( parser, indexBuffers[ i ] );
	}

	parser.ExpectTokenString( "}" );
}

/*
========================
rvRenderModelMD5R::ParseSilhouetteEdges
========================
*/
void rvRenderModelMD5R::ParseSilhouetteEdges( Lexer &parser ) {
	parser.ExpectTokenString( "[" );
	const int numSilEdges = parser.ParseInt();
	parser.ExpectTokenString( "]" );
	parser.ExpectTokenString( "{" );

	if ( numSilEdges < 0 ) {
		parser.Error( "Invalid MD5R silhouette-edge count %d", numSilEdges );
	}

	silEdges.SetNum( numSilEdges );
	for ( int i = 0; i < numSilEdges; ++i ) {
		silEdges[ i ].p1 = parser.ParseInt();
		silEdges[ i ].p2 = parser.ParseInt();
		silEdges[ i ].v1 = parser.ParseInt();
		silEdges[ i ].v2 = parser.ParseInt();
	}

	parser.ExpectTokenString( "}" );
}

/*
========================
rvRenderModelMD5R::ParseLevelOfDetail
========================
*/
void rvRenderModelMD5R::ParseLevelOfDetail( Lexer &parser ) {
	parser.ExpectTokenString( "[" );
	const int numLODs = parser.ParseInt();
	parser.ExpectTokenString( "]" );
	parser.ExpectTokenString( "{" );

	if ( numLODs < 0 ) {
		parser.Error( "Invalid MD5R LOD count %d", numLODs );
	}

	lods.SetNum( numLODs );
	for ( int i = 0; i < numLODs; ++i ) {
		lods[ i ].rangeEnd = parser.ParseFloat();
		lods[ i ].rangeEndSquared = lods[ i ].rangeEnd * lods[ i ].rangeEnd;
	}

	parser.ExpectTokenString( "}" );
}

/*
========================
rvRenderModelMD5R::ParsePrimBatch
========================
*/
void rvRenderModelMD5R::ParsePrimBatch( Lexer &parser, rvMD5RPrimBatch &primBatch ) {
	idToken token;

	primBatch = rvMD5RPrimBatch();

	parser.ExpectTokenString( "{" );
	if ( !parser.ReadToken( &token ) ) {
		parser.Error( "Expected Transform or geometry keyword" );
	}

	if ( token.Icmp( "Transform" ) == 0 ) {
		parser.ExpectTokenString( "[" );
		primBatch.numTransforms = parser.ParseInt();
		parser.ExpectTokenString( "]" );
		parser.ExpectTokenString( "{" );

		if ( primBatch.numTransforms > 25 ) {
			parser.Error( "Primitive batch initialization failed - too many transforms per batch" );
		}

		if ( primBatch.numTransforms < 0 ) {
			parser.Error( "Primitive batch initialization failed - invalid transform count %d", primBatch.numTransforms );
		}

		primBatch.transformPalette.SetNum( primBatch.numTransforms );
		for ( int i = 0; i < primBatch.numTransforms; ++i ) {
			primBatch.transformPalette[ i ] = parser.ParseInt();
		}

		parser.ExpectTokenString( "}" );
		if ( !parser.ReadToken( &token ) ) {
			parser.Error( "Expected primitive-batch geometry keyword" );
		}
	}

	if ( token.Icmp( "SilTraceIndexedTriList" ) == 0 ) {
		primBatch.silTraceGeoSpec.vertexStart = parser.ParseInt();
		primBatch.silTraceGeoSpec.vertexCount = parser.ParseInt();
		primBatch.silTraceGeoSpec.indexStart = parser.ParseInt();
		primBatch.silTraceGeoSpec.primitiveCount = parser.ParseInt();
		primBatch.hasSilTraceGeoSpec = true;
		if ( !parser.ReadToken( &token ) ) {
			parser.Error( "Expected DrawIndexedTriList, ShadowVerts, ShadowIndexedTriList or }" );
		}
	}

	if ( token.Icmp( "DrawIndexedTriList" ) == 0 ) {
		primBatch.drawGeoSpec.vertexStart = parser.ParseInt();
		primBatch.drawGeoSpec.vertexCount = parser.ParseInt();
		primBatch.drawGeoSpec.indexStart = parser.ParseInt();
		primBatch.drawGeoSpec.primitiveCount = parser.ParseInt();
		primBatch.hasDrawGeoSpec = true;
		if ( !parser.ReadToken( &token ) ) {
			parser.Error( "Expected ShadowVerts, ShadowIndexedTriList or }" );
		}
	}

	if ( token.Icmp( "ShadowVerts" ) == 0 ) {
		primBatch.shadowVolGeoSpec.vertexStart = parser.ParseInt();
		primBatch.shadowVolGeoSpec.vertexCount = primBatch.silTraceGeoSpec.vertexCount * 2;
		if ( primBatch.shadowVolGeoSpec.vertexCount == 0 ) {
			parser.Error( "Primitive batch initialization failed - expected SilTraceIndexedTriList statement" );
		}
		primBatch.hasShadowGeoSpec = true;
		if ( !parser.ReadToken( &token ) ) {
			parser.Error( "Expected SilhouetteEdge or }" );
		}
	} else if ( token.Icmp( "ShadowIndexedTriList" ) == 0 ) {
		primBatch.shadowVolGeoSpec.vertexStart = parser.ParseInt();
		primBatch.shadowVolGeoSpec.vertexCount = parser.ParseInt();
		primBatch.shadowVolGeoSpec.indexStart = parser.ParseInt();
		primBatch.shadowVolGeoSpec.primitiveCount = parser.ParseInt();
		primBatch.numShadowPrimitivesNoCaps = parser.ParseInt();
		primBatch.shadowCapPlaneBits = parser.ParseInt();
		primBatch.hasShadowGeoSpec = true;
		if ( !parser.ReadToken( &token ) ) {
			parser.Error( "Expected SilhouetteEdge or }" );
		}
	}

	if ( token.Icmp( "SilhouetteEdge" ) == 0 ) {
		primBatch.silEdgeStart = parser.ParseInt();
		primBatch.silEdgeCount = parser.ParseInt();
		if ( !parser.ReadToken( &token ) ) {
			parser.Error( "Expected }" );
		}
	}

	if ( token.Icmp( "}" ) != 0 ) {
		parser.Error( "Expected }." );
	}
}

/*
========================
rvRenderModelMD5R::ParseMesh
========================
*/
void rvRenderModelMD5R::ParseMesh( Lexer &parser, int meshIndex ) {
	idToken token;
	idToken materialToken;
	rvMD5RMesh &mesh = meshes[ meshIndex ];

	mesh = rvMD5RMesh();
	mesh.meshIdentifier = meshIndex;

	parser.ExpectTokenString( "{" );
	if ( !parser.ReadToken( &token ) ) {
		parser.Error( "Expected Material keyword." );
	}

	if ( token.Icmp( "LevelOfDetail" ) == 0 ) {
		mesh.levelOfDetail = parser.ParseInt();
		if ( !parser.ReadToken( &token ) ) {
			parser.Error( "Expected Material keyword." );
		}
	}

	if ( token.Icmp( "Material" ) != 0 ) {
		parser.Error( "Expected Material keyword." );
	}

	parser.ExpectAnyToken( &materialToken );
	mesh.materialName = materialToken;
	mesh.material = declManager->FindMaterial( mesh.materialName.c_str() );

	if ( !parser.ReadToken( &token ) ) {
		parser.Error( "Expected SilTraceBuffers, DrawBuffers, ShadowVolumeBuffers or PrimBatch keyword." );
	}

	if ( token.Icmp( "SilTraceBuffers" ) == 0 ) {
		mesh.silTraceVertexBuffer = parser.ParseInt();
		mesh.silTraceIndexBuffer = parser.ParseInt();
		if ( mesh.silTraceVertexBuffer < 0 || mesh.silTraceVertexBuffer >= vertexBuffers.Num()
			|| mesh.silTraceIndexBuffer < 0 || mesh.silTraceIndexBuffer >= indexBuffers.Num() ) {
			parser.Error( "Invalid buffer reference by SilTraceBuffers statement" );
		}
		if ( !parser.ReadToken( &token ) ) {
			parser.Error( "Expected DrawBuffers, ShadowVolumeBuffers or PrimBatch keyword." );
		}
	}

	if ( token.Icmp( "DrawBuffers" ) == 0 ) {
		mesh.drawVertexBuffer = parser.ParseInt();
		mesh.drawIndexBuffer = parser.ParseInt();
		if ( mesh.drawVertexBuffer < 0 || mesh.drawVertexBuffer >= vertexBuffers.Num()
			|| mesh.drawIndexBuffer < 0 || mesh.drawIndexBuffer >= indexBuffers.Num() ) {
			parser.Error( "Invalid buffer reference by DrawBuffers statement" );
		}
		if ( !parser.ReadToken( &token ) ) {
			parser.Error( "Expected ShadowVolumeBuffers or PrimBatch keyword." );
		}
	}

	if ( token.Icmp( "ShadowVolumeBuffers" ) == 0 ) {
		mesh.shadowVolVertexBuffer = parser.ParseInt();
		mesh.shadowVolIndexBuffer = parser.ParseInt();
		if ( mesh.shadowVolVertexBuffer < 0 || mesh.shadowVolVertexBuffer >= vertexBuffers.Num()
			|| mesh.shadowVolIndexBuffer < 0 || mesh.shadowVolIndexBuffer >= indexBuffers.Num() ) {
			parser.Error( "Invalid buffer reference by ShadowVolumeBuffers statement" );
		}
		if ( !parser.ReadToken( &token ) ) {
			parser.Error( "Expected PrimBatch keyword." );
		}
	}

	if ( token.Icmp( "PrimBatch" ) != 0 ) {
		parser.Error( "Expected PrimBatch keyword." );
	}

	parser.ExpectTokenString( "[" );
	const int numPrimBatches = parser.ParseInt();
	parser.ExpectTokenString( "]" );
	parser.ExpectTokenString( "{" );

	if ( numPrimBatches < 0 ) {
		parser.Error( "Invalid MD5R primitive-batch count %d", numPrimBatches );
	}

	mesh.primBatches.SetNum( numPrimBatches );
	for ( int i = 0; i < numPrimBatches; ++i ) {
		parser.ExpectTokenString( "PrimBatch" );
		ParsePrimBatch( parser, mesh.primBatches[ i ] );
	}

	parser.ExpectTokenString( "}" );
	R_MD5R_CalcMeshGeometryProfile( mesh );

	if ( !parser.ReadToken( &token ) ) {
		parser.Error( "Expected } or Bounds." );
	}

	if ( token.Icmp( "Bounds" ) == 0 ) {
		R_MD5R_ParseFlexibleBounds( parser, mesh.bounds );
		if ( !parser.ReadToken( &token ) ) {
			parser.Error( "Expected }." );
		}
	}

	if ( token.Icmp( "}" ) != 0 ) {
		parser.Error( "Couldn't find expected '}'" );
	}
}

/*
========================
rvRenderModelMD5R::ParseMeshes
========================
*/
void rvRenderModelMD5R::ParseMeshes( Lexer &parser ) {
	parser.ExpectTokenString( "[" );
	const int numMeshes = parser.ParseInt();
	parser.ExpectTokenString( "]" );
	parser.ExpectTokenString( "{" );

	if ( numMeshes < 0 ) {
		parser.Error( "Invalid MD5R mesh count %d", numMeshes );
	}

	meshes.SetNum( numMeshes );
	for ( int i = 0; i < numMeshes; ++i ) {
		parser.ExpectTokenString( "Mesh" );
		ParseMesh( parser, i );
	}

	parser.ExpectTokenString( "}" );
}

/*
========================
rvRenderModelMD5R::BuildLevelsOfDetail
========================
*/
void rvRenderModelMD5R::BuildLevelsOfDetail() {
	int maxReferencedLOD = -1;

	allLODMeshes.Clear();
	for ( int i = 0; i < lods.Num(); ++i ) {
		lods[ i ].meshIndexes.Clear();
	}

	for ( int i = 0; i < meshes.Num(); ++i ) {
		if ( meshes[ i ].levelOfDetail > maxReferencedLOD ) {
			maxReferencedLOD = meshes[ i ].levelOfDetail;
		}
	}

	if ( lods.Num() == 0 && maxReferencedLOD >= 0 ) {
		lods.SetNum( maxReferencedLOD + 1 );
		for ( int i = 0; i < lods.Num(); ++i ) {
			lods[ i ].rangeEnd = idMath::INFINITY;
			lods[ i ].rangeEndSquared = idMath::INFINITY;
		}

		common->DPrintf(
			"rvRenderModelMD5R::BuildLevelsOfDetail: '%s' referenced LODs without explicit ranges; synthetic ranges default to infinity until rvMesh runtime support is ported\n",
			name.c_str() );
	}

	for ( int i = 0; i < meshes.Num(); ++i ) {
		const int lodIndex = meshes[ i ].levelOfDetail;
		if ( lodIndex < 0 || lodIndex >= lods.Num() ) {
			allLODMeshes.Append( i );
			continue;
		}

		lods[ lodIndex ].meshIndexes.Append( i );
	}
}

/*
========================
rvRenderModelMD5R::ParseJoint
========================
*/
void rvRenderModelMD5R::ParseJoint( Lexer &parser, int jointIndex, idJointQuat &worldPose ) {
	idToken nameToken;
	parser.ExpectAnyToken( &nameToken );

	idMD5Joint &joint = joints[ jointIndex ];
	joint.name = nameToken;

	const int parentIndex = parser.ParseInt();
	if ( parentIndex < -1 || parentIndex >= jointIndex ) {
		parser.Error( "Invalid parent index %d for joint '%s'", parentIndex, joint.name.c_str() );
	}
	joint.parent = ( parentIndex >= 0 ) ? &joints[ parentIndex ] : NULL;

	R_MD5R_ParseFlexibleVec3( parser, worldPose.t );
	idVec3 quatXYZ;
	R_MD5R_ParseFlexibleVec3( parser, quatXYZ );
	worldPose.q.x = quatXYZ.x;
	worldPose.q.y = quatXYZ.y;
	worldPose.q.z = quatXYZ.z;
	worldPose.q.w = worldPose.q.CalcW();
}

/*
========================
rvRenderModelMD5R::ParseJoints
========================
*/
void rvRenderModelMD5R::ParseJoints( Lexer &parser ) {
	parser.ExpectTokenString( "[" );
	const int numJoints = parser.ParseInt();
	parser.ExpectTokenString( "]" );
	parser.ExpectTokenString( "{" );

	if ( numJoints < 0 ) {
		parser.Error( "Invalid MD5R joint count %d", numJoints );
	}

	joints.SetNum( numJoints );
	defaultPose.SetNum( numJoints );
	skinSpaceToLocalMats.SetNum( numJoints );

	idList<idJointMat> worldPoseMats;
	worldPoseMats.SetNum( numJoints );

	for ( int i = 0; i < numJoints; ++i ) {
		ParseJoint( parser, i, defaultPose[ i ] );
		worldPoseMats[ i ].SetRotation( defaultPose[ i ].q.ToMat3() );
		worldPoseMats[ i ].SetTranslation( defaultPose[ i ].t );

		if ( joints[ i ].parent != NULL ) {
			const int parentIndex = static_cast<int>( joints[ i ].parent - joints.Ptr() );
			defaultPose[ i ].q = ( worldPoseMats[ i ].ToMat3() * worldPoseMats[ parentIndex ].ToMat3().Transpose() ).ToQuat();
			defaultPose[ i ].t = ( worldPoseMats[ i ].ToVec3() - worldPoseMats[ parentIndex ].ToVec3() ) * worldPoseMats[ parentIndex ].ToMat3().Transpose();
		}
	}

	parser.ExpectTokenString( "}" );

	for ( int i = 0; i < numJoints; ++i ) {
		skinSpaceToLocalMats[ i ] = worldPoseMats[ i ];
		skinSpaceToLocalMats[ i ].Invert();
	}
}

/*
========================
rvRenderModelMD5R::LoadModel

This follows the retail top-level section ordering and captures the same model
metadata, but it still skips the packed vertex/index payload and does not yet
build live rvMesh-backed render surfaces.
========================
*/
void rvRenderModelMD5R::LoadModel() {
	PurgeModel();
	purged = false;
	reloadable = true;
	source = MD5R_SOURCE_FILE;

	idAutoPtr<Lexer> parser( LexerFactory::MakeLexer( name.c_str(), LEXFL_ALLOWPATHNAMES | LEXFL_NOSTRINGESCAPECHARS ) );
	if ( parser.get() == NULL || !parser->LoadFile( name.c_str() ) ) {
		MakeDefaultModel();
		return;
	}

	parser->ExpectTokenString( MD5R_VERSION_STRING );
	md5rVersion = parser->ParseInt();
	if ( md5rVersion != MD5R_VERSION ) {
		parser->Error( "Invalid version %d. Should be version %d", md5rVersion, MD5R_VERSION );
	}

	if ( parser->PeekTokenString( "CommandLine" ) ) {
		idToken commandLineToken;
		parser->ExpectTokenString( "CommandLine" );
		parser->ExpectAnyToken( &commandLineToken );
		commandLine = commandLineToken;
	}

	if ( parser->PeekTokenString( "Joint" ) ) {
		parser->ExpectTokenString( "Joint" );
		ParseJoints( *parser );
	}

	if ( !parser->PeekTokenString( "VertexBuffer" ) ) {
		parser->Error( "Expected VertexBuffer keyword" );
	}
	parser->ExpectTokenString( "VertexBuffer" );
	ParseVertexBuffers( *parser );

	if ( parser->PeekTokenString( "IndexBuffer" ) ) {
		parser->ExpectTokenString( "IndexBuffer" );
		ParseIndexBuffers( *parser );
	}

	if ( parser->PeekTokenString( "SilhouetteEdge" ) ) {
		parser->ExpectTokenString( "SilhouetteEdge" );
		ParseSilhouetteEdges( *parser );
	}

	if ( parser->PeekTokenString( "LevelOfDetail" ) ) {
		parser->ExpectTokenString( "LevelOfDetail" );
		ParseLevelOfDetail( *parser );
	}

	if ( !parser->PeekTokenString( "Mesh" ) ) {
		parser->Error( "Expected Mesh keyword" );
	}
	parser->ExpectTokenString( "Mesh" );
	ParseMeshes( *parser );
	BuildLevelsOfDetail();

	parser->ExpectTokenString( "Bounds" );
	R_MD5R_ParseFlexibleBounds( *parser, bounds );

	if ( parser->PeekTokenString( "HasSky" ) ) {
		parser->ExpectTokenString( "HasSky" );
		hasSky = true;
	}

	idToken token;
	while ( parser->ReadToken( &token ) ) {
		parser->Warning( "Ignoring unexpected trailing token '%s' in '%s'", token.c_str(), name.c_str() );
	}

	geometrySectionsSkipped = ( vertexBuffers.Num() > 0 || indexBuffers.Num() > 0 || meshes.Num() > 0 );
	if ( geometrySectionsSkipped ) {
		metadataOnly = true;
		common->Warning(
			"rvRenderModelMD5R::LoadModel: parsed packed-section metadata for '%s', but MD5R surface generation is not implemented yet",
			name.c_str() );
	}

	fileSystem->ReadFile( name.c_str(), NULL, &timeStamp );
}

/*
========================
rvRenderModelMD5R::PurgeModel
========================
*/
void rvRenderModelMD5R::PurgeModel() {
	idRenderModelStatic::PurgeModel();
	vertexBuffers.Clear();
	indexBuffers.Clear();
	silEdges.Clear();
	lods.Clear();
	allLODMeshes.Clear();
	meshes.Clear();
	joints.Clear();
	defaultPose.Clear();
	skinSpaceToLocalMats.Clear();
	commandLine.Clear();
	md5rVersion = 0;
	metadataOnly = false;
	geometrySectionsSkipped = false;
	hasSky = false;
}

/*
========================
rvRenderModelMD5R::IsDynamicModel
========================
*/
dynamicModel_t rvRenderModelMD5R::IsDynamicModel() const {
	return joints.Num() > 0 ? DM_CACHED : DM_STATIC;
}

/*
========================
rvRenderModelMD5R::Bounds
========================
*/
idBounds rvRenderModelMD5R::Bounds( const renderEntity_s *ent ) const {
	if ( ent != NULL && joints.Num() > 0 ) {
		return ent->bounds;
	}
	return bounds;
}

/*
========================
rvRenderModelMD5R::NumJoints
========================
*/
int rvRenderModelMD5R::NumJoints() const {
	return joints.Num();
}

/*
========================
rvRenderModelMD5R::GetJoints
========================
*/
const idMD5Joint *rvRenderModelMD5R::GetJoints() const {
	return joints.Num() > 0 ? joints.Ptr() : NULL;
}

/*
========================
rvRenderModelMD5R::GetDefaultPose
========================
*/
const idJointQuat *rvRenderModelMD5R::GetDefaultPose() const {
	return defaultPose.Num() > 0 ? defaultPose.Ptr() : NULL;
}

/*
========================
rvRenderModelMD5R::GetSkinSpaceToLocalMats
========================
*/
const idJointMat *rvRenderModelMD5R::GetSkinSpaceToLocalMats() const {
	return skinSpaceToLocalMats.Num() > 0 ? skinSpaceToLocalMats.Ptr() : NULL;
}

/*
========================
rvRenderModelMD5R::Memory
========================
*/
int rvRenderModelMD5R::Memory() const {
	int total = idRenderModelStatic::Memory();
	total += vertexBuffers.MemoryUsed();
	total += indexBuffers.MemoryUsed();
	total += silEdges.MemoryUsed();
	total += lods.MemoryUsed();
	total += allLODMeshes.MemoryUsed();
	total += meshes.MemoryUsed();
	total += joints.MemoryUsed();
	total += defaultPose.MemoryUsed();
	total += skinSpaceToLocalMats.MemoryUsed();
	total += commandLine.DynamicMemoryUsed();

	for ( int i = 0; i < lods.Num(); ++i ) {
		total += lods[ i ].meshIndexes.MemoryUsed();
	}

	for ( int i = 0; i < meshes.Num(); ++i ) {
		total += meshes[ i ].materialName.DynamicMemoryUsed();
		total += meshes[ i ].primBatches.MemoryUsed();

		for ( int j = 0; j < meshes[ i ].primBatches.Num(); ++j ) {
			total += meshes[ i ].primBatches[ j ].transformPalette.MemoryUsed();
		}
	}

	for ( int i = 0; i < joints.Num(); ++i ) {
		total += joints[ i ].name.DynamicMemoryUsed();
	}

	return total;
}

/*
========================
rvRenderModelMD5R::WriteAll
========================
*/
void rvRenderModelMD5R::WriteAll( bool compressed ) {
	(void)compressed;

	int modelCount = 0;
	for ( rvRenderModelMD5R *model = modelList; model != NULL; model = model->next ) {
		if ( !model->purged ) {
			++modelCount;
		}
	}

	if ( modelCount > 0 ) {
		common->Warning(
			"rvRenderModelMD5R::WriteAll: MD5R model export is not implemented yet; %d loaded MD5R model(s) were left unchanged",
			modelCount );
	}
}
