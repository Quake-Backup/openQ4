
#include "Simd_SSE2.h"

#ifdef ID_SIMD_SSE2_AVAILABLE

#include <emmintrin.h>

//===============================================================
//
//	SSE2 implementation of idSIMDProcessor
//
//===============================================================

/*
============
idSIMD_SSE2::GetName
============
*/
const char * VPCALL idSIMD_SSE2::GetName( void ) const {
	return "SSE2";
}

/*
============
SSE2_StoreVec3

	stores the low three lanes without touching the 4th float of memory,
	which idDrawVert packs with color bytes / adjacent fields
============
*/
ID_INLINE static void SSE2_StoreVec3( float *dst, __m128 v ) {
	_mm_store_sd( (double *)dst, _mm_castps_pd( v ) );
	_mm_store_ss( dst + 2, _mm_shuffle_ps( v, v, _MM_SHUFFLE( 2, 2, 2, 2 ) ) );
}

/*
============
SSE2_JointMatMulVec4

	returns ( row0 . v, row1 . v, row2 . v, junk ) for a row-major 3x4
	joint matrix; the sum order matches idJointMat::operator*( idVec4 )
============
*/
ID_INLINE static __m128 SSE2_JointMatMulVec4( const float *mat, __m128 v ) {
	__m128 r0 = _mm_mul_ps( _mm_loadu_ps( mat + 0 ), v );
	__m128 r1 = _mm_mul_ps( _mm_loadu_ps( mat + 4 ), v );
	__m128 r2 = _mm_mul_ps( _mm_loadu_ps( mat + 8 ), v );
	__m128 r3 = _mm_setzero_ps();
	_MM_TRANSPOSE4_PS( r0, r1, r2, r3 );
	return _mm_add_ps( _mm_add_ps( _mm_add_ps( r0, r1 ), r2 ), r3 );
}

/*
============
SSE2_TransformColumns

	result = c0 * v.x + c1 * v.y + c2 * v.z + c3 * v.w, with the same
	left-to-right sum order as idJointMat::operator*( idVec4 )
============
*/
ID_INLINE static __m128 SSE2_TransformColumns( __m128 c0, __m128 c1, __m128 c2, __m128 c3, __m128 v ) {
	__m128 r = _mm_mul_ps( c0, _mm_shuffle_ps( v, v, _MM_SHUFFLE( 0, 0, 0, 0 ) ) );
	r = _mm_add_ps( r, _mm_mul_ps( c1, _mm_shuffle_ps( v, v, _MM_SHUFFLE( 1, 1, 1, 1 ) ) ) );
	r = _mm_add_ps( r, _mm_mul_ps( c2, _mm_shuffle_ps( v, v, _MM_SHUFFLE( 2, 2, 2, 2 ) ) ) );
	r = _mm_add_ps( r, _mm_mul_ps( c3, _mm_shuffle_ps( v, v, _MM_SHUFFLE( 3, 3, 3, 3 ) ) ) );
	return r;
}

/*
============
idSIMD_SSE2::Dot

	dst[i] = constant * src[i].Normal() + src[i][3];
============
*/
void VPCALL idSIMD_SSE2::Dot( float * RESTRICT dst, const idVec3 &constant, const idPlane * RESTRICT src, const int count ) {
	const __m128 cx = _mm_set1_ps( constant.x );
	const __m128 cy = _mm_set1_ps( constant.y );
	const __m128 cz = _mm_set1_ps( constant.z );

	int i = 0;
	for ( ; i + 4 <= count; i += 4 ) {
		__m128 p0 = _mm_loadu_ps( src[i+0].ToFloatPtr() );
		__m128 p1 = _mm_loadu_ps( src[i+1].ToFloatPtr() );
		__m128 p2 = _mm_loadu_ps( src[i+2].ToFloatPtr() );
		__m128 p3 = _mm_loadu_ps( src[i+3].ToFloatPtr() );
		_MM_TRANSPOSE4_PS( p0, p1, p2, p3 );
		__m128 r = _mm_add_ps( _mm_add_ps( _mm_add_ps(
			_mm_mul_ps( cx, p0 ), _mm_mul_ps( cy, p1 ) ), _mm_mul_ps( cz, p2 ) ), p3 );
		_mm_storeu_ps( dst + i, r );
	}
	for ( ; i < count; i++ ) {
		dst[i] = constant * src[i].Normal() + src[i][3];
	}
}

/*
============
idSIMD_SSE2::Dot

	dst[i] = constant * src[i].xyz;
============
*/
void VPCALL idSIMD_SSE2::Dot( float * RESTRICT dst, const idVec3 &constant, const idDrawVert * RESTRICT src, const int count ) {
	const __m128 cx = _mm_set1_ps( constant.x );
	const __m128 cy = _mm_set1_ps( constant.y );
	const __m128 cz = _mm_set1_ps( constant.z );

	int i = 0;
	for ( ; i + 4 <= count; i += 4 ) {
		// the 16 byte load stays inside the 64 byte idDrawVert
		__m128 v0 = _mm_loadu_ps( src[i+0].xyz.ToFloatPtr() );
		__m128 v1 = _mm_loadu_ps( src[i+1].xyz.ToFloatPtr() );
		__m128 v2 = _mm_loadu_ps( src[i+2].xyz.ToFloatPtr() );
		__m128 v3 = _mm_loadu_ps( src[i+3].xyz.ToFloatPtr() );
		_MM_TRANSPOSE4_PS( v0, v1, v2, v3 );
		__m128 r = _mm_add_ps( _mm_add_ps(
			_mm_mul_ps( cx, v0 ), _mm_mul_ps( cy, v1 ) ), _mm_mul_ps( cz, v2 ) );
		_mm_storeu_ps( dst + i, r );
	}
	for ( ; i < count; i++ ) {
		dst[i] = constant * src[i].xyz;
	}
}

/*
============
idSIMD_SSE2::Dot

	dst[i] = constant.Normal() * src[i] + constant[3];
============
*/
void VPCALL idSIMD_SSE2::Dot( float * RESTRICT dst, const idPlane &constant, const idVec3 * RESTRICT src, const int count ) {
	const __m128 cx = _mm_set1_ps( constant[0] );
	const __m128 cy = _mm_set1_ps( constant[1] );
	const __m128 cz = _mm_set1_ps( constant[2] );
	const __m128 cd = _mm_set1_ps( constant[3] );

	int i = 0;
	// idVec3 is 12 bytes, so a 16 byte load on an element is only safe when
	// another element follows it; always leave the final element to the tail
	for ( ; i + 5 <= count; i += 4 ) {
		__m128 v0 = _mm_loadu_ps( src[i+0].ToFloatPtr() );
		__m128 v1 = _mm_loadu_ps( src[i+1].ToFloatPtr() );
		__m128 v2 = _mm_loadu_ps( src[i+2].ToFloatPtr() );
		__m128 v3 = _mm_loadu_ps( src[i+3].ToFloatPtr() );
		_MM_TRANSPOSE4_PS( v0, v1, v2, v3 );
		__m128 r = _mm_add_ps( _mm_add_ps( _mm_add_ps(
			_mm_mul_ps( cx, v0 ), _mm_mul_ps( cy, v1 ) ), _mm_mul_ps( cz, v2 ) ), cd );
		_mm_storeu_ps( dst + i, r );
	}
	for ( ; i < count; i++ ) {
		dst[i] = constant.Normal() * src[i] + constant[3];
	}
}

/*
============
idSIMD_SSE2::Dot

	dst[i] = constant.Normal() * src[i].xyz + constant[3];
============
*/
void VPCALL idSIMD_SSE2::Dot( float * RESTRICT dst, const idPlane &constant, const idDrawVert * RESTRICT src, const int count ) {
	const __m128 cx = _mm_set1_ps( constant[0] );
	const __m128 cy = _mm_set1_ps( constant[1] );
	const __m128 cz = _mm_set1_ps( constant[2] );
	const __m128 cd = _mm_set1_ps( constant[3] );

	int i = 0;
	for ( ; i + 4 <= count; i += 4 ) {
		__m128 v0 = _mm_loadu_ps( src[i+0].xyz.ToFloatPtr() );
		__m128 v1 = _mm_loadu_ps( src[i+1].xyz.ToFloatPtr() );
		__m128 v2 = _mm_loadu_ps( src[i+2].xyz.ToFloatPtr() );
		__m128 v3 = _mm_loadu_ps( src[i+3].xyz.ToFloatPtr() );
		_MM_TRANSPOSE4_PS( v0, v1, v2, v3 );
		__m128 r = _mm_add_ps( _mm_add_ps( _mm_add_ps(
			_mm_mul_ps( cx, v0 ), _mm_mul_ps( cy, v1 ) ), _mm_mul_ps( cz, v2 ) ), cd );
		_mm_storeu_ps( dst + i, r );
	}
	for ( ; i < count; i++ ) {
		dst[i] = constant.Normal() * src[i].xyz + constant[3];
	}
}

/*
============
idSIMD_SSE2::MinMax
============
*/
void VPCALL idSIMD_SSE2::MinMax( idVec3 &min, idVec3 &max, const idDrawVert * RESTRICT src, const int count ) {
	__m128 minAcc = _mm_set1_ps( idMath::INFINITY );
	__m128 maxAcc = _mm_set1_ps( -idMath::INFINITY );

	for ( int i = 0; i < count; i++ ) {
		__m128 v = _mm_loadu_ps( src[i].xyz.ToFloatPtr() );
		minAcc = _mm_min_ps( minAcc, v );
		maxAcc = _mm_max_ps( maxAcc, v );
	}

	SSE2_StoreVec3( min.ToFloatPtr(), minAcc );
	SSE2_StoreVec3( max.ToFloatPtr(), maxAcc );
}

/*
============
idSIMD_SSE2::MinMax
============
*/
void VPCALL idSIMD_SSE2::MinMax( idVec3 &min, idVec3 &max, const idDrawVert * RESTRICT src, const int *indexes, const int count ) {
	__m128 minAcc = _mm_set1_ps( idMath::INFINITY );
	__m128 maxAcc = _mm_set1_ps( -idMath::INFINITY );

	for ( int i = 0; i < count; i++ ) {
		__m128 v = _mm_loadu_ps( src[indexes[i]].xyz.ToFloatPtr() );
		minAcc = _mm_min_ps( minAcc, v );
		maxAcc = _mm_max_ps( maxAcc, v );
	}

	SSE2_StoreVec3( min.ToFloatPtr(), minAcc );
	SSE2_StoreVec3( max.ToFloatPtr(), maxAcc );
}

/*
============
idSIMD_SSE2::ConvertJointQuatsToJointMats

	scalar, but with the idMat3 round trip of the generic version removed;
	the expressions mirror idQuat::ToMat3 followed by idJointMat::SetRotation
	(which transposes) and SetTranslation, so the results are bit identical
============
*/
void VPCALL idSIMD_SSE2::ConvertJointQuatsToJointMats( idJointMat * RESTRICT jointMats, const idJointQuat * RESTRICT jointQuats, const int numJoints ) {
	for ( int i = 0; i < numJoints; i++ ) {
		const idJointQuat &jq = jointQuats[i];
		float *m = jointMats[i].ToFloatPtr();

		const float x = jq.q.x;
		const float y = jq.q.y;
		const float z = jq.q.z;
		const float w = jq.q.w;

		const float x2 = x + x;
		const float y2 = y + y;
		const float z2 = z + z;

		const float xx = x * x2;
		const float xy = x * y2;
		const float xz = x * z2;

		const float yy = y * y2;
		const float yz = y * z2;
		const float zz = z * z2;

		const float wx = w * x2;
		const float wy = w * y2;
		const float wz = w * z2;

		m[0 * 4 + 0] = 1.0f - ( yy + zz );
		m[0 * 4 + 1] = xy + wz;
		m[0 * 4 + 2] = xz - wy;
		m[0 * 4 + 3] = jq.t[0];

		m[1 * 4 + 0] = xy - wz;
		m[1 * 4 + 1] = 1.0f - ( xx + zz );
		m[1 * 4 + 2] = yz + wx;
		m[1 * 4 + 3] = jq.t[1];

		m[2 * 4 + 0] = xz + wy;
		m[2 * 4 + 1] = yz - wx;
		m[2 * 4 + 2] = 1.0f - ( xx + yy );
		m[2 * 4 + 3] = jq.t[2];
	}
}

/*
============
SSE2_ComposeJointMat

	dst = a * t for row-major 3x4 affine joint matrices, with the same sum
	order as idJointMat::operator*= / idJointMat::Multiply
============
*/
ID_INLINE static void SSE2_ComposeJointMat( float *dst, const float *a, __m128 t0, __m128 t1, __m128 t2 ) {
	for ( int row = 0; row < 3; row++ ) {
		__m128 n = _mm_mul_ps( _mm_set1_ps( a[row * 4 + 0] ), t0 );
		n = _mm_add_ps( n, _mm_mul_ps( _mm_set1_ps( a[row * 4 + 1] ), t1 ) );
		n = _mm_add_ps( n, _mm_mul_ps( _mm_set1_ps( a[row * 4 + 2] ), t2 ) );
		n = _mm_add_ps( n, _mm_set_ps( a[row * 4 + 3], 0.0f, 0.0f, 0.0f ) );
		_mm_storeu_ps( dst + row * 4, n );
	}
}

/*
============
idSIMD_SSE2::TransformJoints
============
*/
void VPCALL idSIMD_SSE2::TransformJoints( idJointMat * RESTRICT jointMats, const int * RESTRICT parents, const int firstJoint, const int lastJoint ) {
	for ( int i = firstJoint; i <= lastJoint; i++ ) {
		assert( parents[i] < i );
		float *m = jointMats[i].ToFloatPtr();
		const float *p = jointMats[parents[i]].ToFloatPtr();

		__m128 t0 = _mm_loadu_ps( m + 0 );
		__m128 t1 = _mm_loadu_ps( m + 4 );
		__m128 t2 = _mm_loadu_ps( m + 8 );

		SSE2_ComposeJointMat( m, p, t0, t1, t2 );
	}
}

/*
============
idSIMD_SSE2::MultiplyJoints
============
*/
void VPCALL idSIMD_SSE2::MultiplyJoints( idJointMat * RESTRICT result, const idJointMat * RESTRICT joints1, const idJointMat * RESTRICT joints2, const int numJoints ) {
	for ( int i = 0; i < numJoints; i++ ) {
		const float *m2 = joints2[i].ToFloatPtr();

		__m128 t0 = _mm_loadu_ps( m2 + 0 );
		__m128 t1 = _mm_loadu_ps( m2 + 4 );
		__m128 t2 = _mm_loadu_ps( m2 + 8 );

		SSE2_ComposeJointMat( result[i].ToFloatPtr(), joints1[i].ToFloatPtr(), t0, t1, t2 );
	}
}

/*
============
idSIMD_SSE2::TransformVerts
============
*/
void VPCALL idSIMD_SSE2::TransformVerts( idDrawVert *verts, const int numVerts, const idJointMat *joints, const idVec4 *weights, const int *index, const int numWeights ) {
	const byte *jointsPtr = (const byte *)joints;

	for ( int j = 0, i = 0; i < numVerts; i++ ) {
		__m128 v = SSE2_JointMatMulVec4(
			(const float *)( jointsPtr + index[j * 2 + 0] ), _mm_loadu_ps( weights[j].ToFloatPtr() ) );
		while ( index[j * 2 + 1] == 0 ) {
			j++;
			v = _mm_add_ps( v, SSE2_JointMatMulVec4(
				(const float *)( jointsPtr + index[j * 2 + 0] ), _mm_loadu_ps( weights[j].ToFloatPtr() ) ) );
		}
		j++;

		SSE2_StoreVec3( verts[i].xyz.ToFloatPtr(), v );
	}
}

/*
============
idSIMD_SSE2::TransformVertsNew
============
*/
void VPCALL idSIMD_SSE2::TransformVertsNew( idDrawVert * RESTRICT verts, const int numVerts, idBounds &bounds, const idJointMat * RESTRICT joints, const idVec4 * RESTRICT base, const jointWeight_t * RESTRICT weights, const int numWeights ) {
	const byte * RESTRICT jointsPtr = (const byte * RESTRICT)joints;

	// mirrors the bounds.Zero() starting state of the generic version,
	// which always keeps the origin inside the bounds
	__m128 minAcc = _mm_setzero_ps();
	__m128 maxAcc = _mm_setzero_ps();

	for ( int j = 0, i = 0; i < numVerts; i++, j++ ) {
		__m128 v = SSE2_JointMatMulVec4(
			(const float *)( jointsPtr + weights[j].jointMatOffset ), _mm_loadu_ps( base[j].ToFloatPtr() ) );
		while ( weights[j].nextVertexOffset != JOINTWEIGHT_SIZE ) {
			j++;
			v = _mm_add_ps( v, SSE2_JointMatMulVec4(
				(const float *)( jointsPtr + weights[j].jointMatOffset ), _mm_loadu_ps( base[j].ToFloatPtr() ) ) );
		}

		SSE2_StoreVec3( verts[i].xyz.ToFloatPtr(), v );

		minAcc = _mm_min_ps( minAcc, v );
		maxAcc = _mm_max_ps( maxAcc, v );
	}

	SSE2_StoreVec3( bounds[0].ToFloatPtr(), minAcc );
	SSE2_StoreVec3( bounds[1].ToFloatPtr(), maxAcc );
}

/*
============
idSIMD_SSE2::TransformVertsAndTangents
============
*/
void VPCALL idSIMD_SSE2::TransformVertsAndTangents( idDrawVert * RESTRICT verts, const int numVerts, idBounds &bounds, const idJointMat * RESTRICT joints, const idVec4 * RESTRICT base, const jointWeight_t * RESTRICT weights, const int numWeights ) {
	const byte * RESTRICT jointsPtr = (const byte * RESTRICT)joints;

	__m128 minAcc = _mm_setzero_ps();
	__m128 maxAcc = _mm_setzero_ps();

	for ( int j = 0, i = 0; i < numVerts; i++, j++ ) {
		// blend the joint matrices for this vertex
		{
			const float *m = (const float *)( jointsPtr + weights[j].jointMatOffset );
			__m128 ws = _mm_set1_ps( weights[j].weight );
			__m128 m0 = _mm_mul_ps( ws, _mm_loadu_ps( m + 0 ) );
			__m128 m1 = _mm_mul_ps( ws, _mm_loadu_ps( m + 4 ) );
			__m128 m2 = _mm_mul_ps( ws, _mm_loadu_ps( m + 8 ) );
			while ( weights[j].nextVertexOffset != JOINTWEIGHT_SIZE ) {
				j++;
				m = (const float *)( jointsPtr + weights[j].jointMatOffset );
				ws = _mm_set1_ps( weights[j].weight );
				m0 = _mm_add_ps( m0, _mm_mul_ps( ws, _mm_loadu_ps( m + 0 ) ) );
				m1 = _mm_add_ps( m1, _mm_mul_ps( ws, _mm_loadu_ps( m + 4 ) ) );
				m2 = _mm_add_ps( m2, _mm_mul_ps( ws, _mm_loadu_ps( m + 8 ) ) );
			}

			// transpose the blended matrix to columns and run all four base
			// vectors of the vertex through it
			__m128 c0 = m0, c1 = m1, c2 = m2, c3 = _mm_setzero_ps();
			_MM_TRANSPOSE4_PS( c0, c1, c2, c3 );

			const float *bp = base[i * 4].ToFloatPtr();

			__m128 pos = SSE2_TransformColumns( c0, c1, c2, c3, _mm_loadu_ps( bp + 0 ) );
			__m128 nrm = SSE2_TransformColumns( c0, c1, c2, c3, _mm_loadu_ps( bp + 4 ) );
			__m128 tan0 = SSE2_TransformColumns( c0, c1, c2, c3, _mm_loadu_ps( bp + 8 ) );
			__m128 tan1 = SSE2_TransformColumns( c0, c1, c2, c3, _mm_loadu_ps( bp + 12 ) );

			SSE2_StoreVec3( verts[i].xyz.ToFloatPtr(), pos );
			SSE2_StoreVec3( verts[i].normal.ToFloatPtr(), nrm );
			SSE2_StoreVec3( verts[i].tangents[0].ToFloatPtr(), tan0 );
			SSE2_StoreVec3( verts[i].tangents[1].ToFloatPtr(), tan1 );

			minAcc = _mm_min_ps( minAcc, pos );
			maxAcc = _mm_max_ps( maxAcc, pos );
		}
	}

	SSE2_StoreVec3( bounds[0].ToFloatPtr(), minAcc );
	SSE2_StoreVec3( bounds[1].ToFloatPtr(), maxAcc );
}

/*
============
idSIMD_SSE2::TransformVertsAndTangentsFast
============
*/
void VPCALL idSIMD_SSE2::TransformVertsAndTangentsFast( idDrawVert * RESTRICT verts, const int numVerts, idBounds &bounds, const idJointMat * RESTRICT joints, const idVec4 * RESTRICT base, const jointWeight_t * RESTRICT weights, const int numWeights ) {
	const byte * RESTRICT jointsPtr = (const byte * RESTRICT)joints;
	const byte * RESTRICT weightsPtr = (const byte * RESTRICT)weights;

	__m128 minAcc = _mm_setzero_ps();
	__m128 maxAcc = _mm_setzero_ps();

	for ( int i = 0; i < numVerts; i++ ) {
		const jointWeight_t *weight = (const jointWeight_t *)weightsPtr;
		const float *m = (const float *)( jointsPtr + weight->jointMatOffset );

		weightsPtr += weight->nextVertexOffset;

		__m128 c0 = _mm_loadu_ps( m + 0 );
		__m128 c1 = _mm_loadu_ps( m + 4 );
		__m128 c2 = _mm_loadu_ps( m + 8 );
		__m128 c3 = _mm_setzero_ps();
		_MM_TRANSPOSE4_PS( c0, c1, c2, c3 );

		const float *bp = base[i * 4].ToFloatPtr();

		__m128 pos = SSE2_TransformColumns( c0, c1, c2, c3, _mm_loadu_ps( bp + 0 ) );
		__m128 nrm = SSE2_TransformColumns( c0, c1, c2, c3, _mm_loadu_ps( bp + 4 ) );
		__m128 tan0 = SSE2_TransformColumns( c0, c1, c2, c3, _mm_loadu_ps( bp + 8 ) );
		__m128 tan1 = SSE2_TransformColumns( c0, c1, c2, c3, _mm_loadu_ps( bp + 12 ) );

		SSE2_StoreVec3( verts[i].xyz.ToFloatPtr(), pos );
		SSE2_StoreVec3( verts[i].normal.ToFloatPtr(), nrm );
		SSE2_StoreVec3( verts[i].tangents[0].ToFloatPtr(), tan0 );
		SSE2_StoreVec3( verts[i].tangents[1].ToFloatPtr(), tan1 );

		minAcc = _mm_min_ps( minAcc, pos );
		maxAcc = _mm_max_ps( maxAcc, pos );
	}

	SSE2_StoreVec3( bounds[0].ToFloatPtr(), minAcc );
	SSE2_StoreVec3( bounds[1].ToFloatPtr(), maxAcc );
}

/*
============
idSIMD_SSE2::CreateShadowCache
============
*/
int VPCALL idSIMD_SSE2::CreateShadowCache( idVec4 * RESTRICT vertexCache, int * RESTRICT vertRemap, const idVec3 &lightOrigin, const idDrawVert * RESTRICT verts, const int numVerts ) {
	const __m128 xyzMask = _mm_castsi128_ps( _mm_set_epi32( 0, -1, -1, -1 ) );
	const __m128 oneW = _mm_set_ps( 1.0f, 0.0f, 0.0f, 0.0f );
	const __m128 lo = _mm_set_ps( 0.0f, lightOrigin.z, lightOrigin.y, lightOrigin.x );

	int outVerts = 0;
	for ( int i = 0; i < numVerts; i++ ) {
		if ( vertRemap[i] ) {
			continue;
		}
		__m128 v = _mm_and_ps( _mm_loadu_ps( verts[i].xyz.ToFloatPtr() ), xyzMask );
		_mm_storeu_ps( vertexCache[outVerts+0].ToFloatPtr(), _mm_or_ps( v, oneW ) );

		// R_SetupProjection() builds the projection matrix with a slight crunch
		// for depth, which keeps this w=0 division from rasterizing right at the
		// wrap around point and causing depth fighting with the rear caps
		_mm_storeu_ps( vertexCache[outVerts+1].ToFloatPtr(), _mm_sub_ps( v, lo ) );
		vertRemap[i] = outVerts;
		outVerts += 2;
	}
	return outVerts;
}

/*
============
idSIMD_SSE2::CreateVertexProgramShadowCache
============
*/
int VPCALL idSIMD_SSE2::CreateVertexProgramShadowCache( idVec4 * RESTRICT vertexCache, const idDrawVert * RESTRICT verts, const int numVerts ) {
	const __m128 xyzMask = _mm_castsi128_ps( _mm_set_epi32( 0, -1, -1, -1 ) );
	const __m128 oneW = _mm_set_ps( 1.0f, 0.0f, 0.0f, 0.0f );

	for ( int i = 0; i < numVerts; i++ ) {
		__m128 v = _mm_and_ps( _mm_loadu_ps( verts[i].xyz.ToFloatPtr() ), xyzMask );
		_mm_storeu_ps( vertexCache[i*2+0].ToFloatPtr(), _mm_or_ps( v, oneW ) );
		_mm_storeu_ps( vertexCache[i*2+1].ToFloatPtr(), v );
	}
	return numVerts * 2;
}

#endif /* ID_SIMD_SSE2_AVAILABLE */
