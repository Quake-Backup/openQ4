/*
===========================================================================

openQ4 cryptographic hash primitives
Copyright (C) 2026 DarkMatter Productions

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This is an original implementation of the published FIPS 180-4 SHA-256,
RFC 2104 HMAC, and RFC 8018 PBKDF2 specifications. No source code from the
Quake 4 SDK game module or another cryptographic implementation is used here.

===========================================================================
*/

#include "CryptoHash.h"

#include <cstring>

namespace idCrypto {
namespace {

struct sha256Context_t {
	std::uint32_t state[ 8 ];
	std::uint64_t totalBytes;
	std::uint8_t buffer[ SHA256_BLOCK_BYTES ];
	std::size_t bufferedBytes;
};

static constexpr std::uint32_t SHA256_ROUND_CONSTANTS[ 64 ] = {
	0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
	0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
	0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
	0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
	0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
	0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
	0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
	0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
	0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
	0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
	0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
	0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
	0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
	0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
	0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
	0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

static inline std::uint32_t RotateRight( std::uint32_t value, unsigned int count ) {
	return ( value >> count ) | ( value << ( 32u - count ) );
}

static inline std::uint32_t ReadBigEndian32( const std::uint8_t *bytes ) {
	return ( static_cast<std::uint32_t>( bytes[ 0 ] ) << 24 ) |
		( static_cast<std::uint32_t>( bytes[ 1 ] ) << 16 ) |
		( static_cast<std::uint32_t>( bytes[ 2 ] ) << 8 ) |
		static_cast<std::uint32_t>( bytes[ 3 ] );
}

static inline void WriteBigEndian32( std::uint8_t *bytes, std::uint32_t value ) {
	bytes[ 0 ] = static_cast<std::uint8_t>( value >> 24 );
	bytes[ 1 ] = static_cast<std::uint8_t>( value >> 16 );
	bytes[ 2 ] = static_cast<std::uint8_t>( value >> 8 );
	bytes[ 3 ] = static_cast<std::uint8_t>( value );
}

static void SHA256Transform( sha256Context_t &context,
		const std::uint8_t block[ SHA256_BLOCK_BYTES ] ) {
	std::uint32_t schedule[ 64 ];
	for ( int index = 0; index < 16; ++index ) {
		schedule[ index ] = ReadBigEndian32( block + index * 4 );
	}
	for ( int index = 16; index < 64; ++index ) {
		const std::uint32_t sigma0 = RotateRight( schedule[ index - 15 ], 7 ) ^
			RotateRight( schedule[ index - 15 ], 18 ) ^ ( schedule[ index - 15 ] >> 3 );
		const std::uint32_t sigma1 = RotateRight( schedule[ index - 2 ], 17 ) ^
			RotateRight( schedule[ index - 2 ], 19 ) ^ ( schedule[ index - 2 ] >> 10 );
		schedule[ index ] = schedule[ index - 16 ] + sigma0 +
			schedule[ index - 7 ] + sigma1;
	}

	std::uint32_t a = context.state[ 0 ];
	std::uint32_t b = context.state[ 1 ];
	std::uint32_t c = context.state[ 2 ];
	std::uint32_t d = context.state[ 3 ];
	std::uint32_t e = context.state[ 4 ];
	std::uint32_t f = context.state[ 5 ];
	std::uint32_t g = context.state[ 6 ];
	std::uint32_t h = context.state[ 7 ];

	for ( int index = 0; index < 64; ++index ) {
		const std::uint32_t sum1 = RotateRight( e, 6 ) ^ RotateRight( e, 11 ) ^ RotateRight( e, 25 );
		const std::uint32_t choose = ( e & f ) ^ ( ( ~e ) & g );
		const std::uint32_t temp1 = h + sum1 + choose +
			SHA256_ROUND_CONSTANTS[ index ] + schedule[ index ];
		const std::uint32_t sum0 = RotateRight( a, 2 ) ^ RotateRight( a, 13 ) ^ RotateRight( a, 22 );
		const std::uint32_t majority = ( a & b ) ^ ( a & c ) ^ ( b & c );
		const std::uint32_t temp2 = sum0 + majority;

		h = g;
		g = f;
		f = e;
		e = d + temp1;
		d = c;
		c = b;
		b = a;
		a = temp1 + temp2;
	}

	context.state[ 0 ] += a;
	context.state[ 1 ] += b;
	context.state[ 2 ] += c;
	context.state[ 3 ] += d;
	context.state[ 4 ] += e;
	context.state[ 5 ] += f;
	context.state[ 6 ] += g;
	context.state[ 7 ] += h;
	idCrypto::SecureZero( schedule, sizeof( schedule ) );
}

static void SHA256Init( sha256Context_t &context ) {
	context.state[ 0 ] = 0x6a09e667u;
	context.state[ 1 ] = 0xbb67ae85u;
	context.state[ 2 ] = 0x3c6ef372u;
	context.state[ 3 ] = 0xa54ff53au;
	context.state[ 4 ] = 0x510e527fu;
	context.state[ 5 ] = 0x9b05688cu;
	context.state[ 6 ] = 0x1f83d9abu;
	context.state[ 7 ] = 0x5be0cd19u;
	context.totalBytes = 0;
	context.bufferedBytes = 0;
	std::memset( context.buffer, 0, sizeof( context.buffer ) );
}

static void SHA256Update( sha256Context_t &context, const void *data, std::size_t dataBytes ) {
	const std::uint8_t *cursor = static_cast<const std::uint8_t *>( data );
	context.totalBytes += static_cast<std::uint64_t>( dataBytes );

	if ( context.bufferedBytes != 0 ) {
		const std::size_t wanted = SHA256_BLOCK_BYTES - context.bufferedBytes;
		const std::size_t copied = dataBytes < wanted ? dataBytes : wanted;
		if ( copied != 0 ) {
			std::memcpy( context.buffer + context.bufferedBytes, cursor, copied );
			context.bufferedBytes += copied;
			cursor += copied;
			dataBytes -= copied;
		}
		if ( context.bufferedBytes == SHA256_BLOCK_BYTES ) {
			SHA256Transform( context, context.buffer );
			context.bufferedBytes = 0;
		}
	}

	while ( dataBytes >= SHA256_BLOCK_BYTES ) {
		SHA256Transform( context, cursor );
		cursor += SHA256_BLOCK_BYTES;
		dataBytes -= SHA256_BLOCK_BYTES;
	}
	if ( dataBytes != 0 ) {
		std::memcpy( context.buffer, cursor, dataBytes );
		context.bufferedBytes = dataBytes;
	}
}

static void SHA256Final( sha256Context_t &context,
		std::uint8_t digest[ SHA256_DIGEST_BYTES ] ) {
	const std::uint64_t totalBits = context.totalBytes * 8u;
	context.buffer[ context.bufferedBytes++ ] = 0x80u;
	if ( context.bufferedBytes > 56 ) {
		std::memset( context.buffer + context.bufferedBytes, 0,
			SHA256_BLOCK_BYTES - context.bufferedBytes );
		SHA256Transform( context, context.buffer );
		context.bufferedBytes = 0;
	}
	std::memset( context.buffer + context.bufferedBytes, 0, 56 - context.bufferedBytes );
	for ( int index = 0; index < 8; ++index ) {
		context.buffer[ 56 + index ] = static_cast<std::uint8_t>( totalBits >> ( 56 - index * 8 ) );
	}
	SHA256Transform( context, context.buffer );
	for ( int index = 0; index < 8; ++index ) {
		WriteBigEndian32( digest + index * 4, context.state[ index ] );
	}
	SecureZero( &context, sizeof( context ) );
}

struct hmacSHA256Prepared_t {
	sha256Context_t inner;
	sha256Context_t outer;
};

static void HMACPrepare( const void *key, std::size_t keyBytes,
		hmacSHA256Prepared_t &prepared ) {
	std::uint8_t normalizedKey[ SHA256_BLOCK_BYTES ] = {};
	if ( keyBytes > SHA256_BLOCK_BYTES ) {
		SHA256( key, keyBytes, normalizedKey );
	} else if ( keyBytes != 0 ) {
		std::memcpy( normalizedKey, key, keyBytes );
	}

	std::uint8_t innerPad[ SHA256_BLOCK_BYTES ];
	std::uint8_t outerPad[ SHA256_BLOCK_BYTES ];
	for ( std::size_t index = 0; index < SHA256_BLOCK_BYTES; ++index ) {
		innerPad[ index ] = normalizedKey[ index ] ^ 0x36u;
		outerPad[ index ] = normalizedKey[ index ] ^ 0x5cu;
	}
	SHA256Init( prepared.inner );
	SHA256Update( prepared.inner, innerPad, sizeof( innerPad ) );
	SHA256Init( prepared.outer );
	SHA256Update( prepared.outer, outerPad, sizeof( outerPad ) );
	SecureZero( normalizedKey, sizeof( normalizedKey ) );
	SecureZero( innerPad, sizeof( innerPad ) );
	SecureZero( outerPad, sizeof( outerPad ) );
}

static void HMACFinish( const hmacSHA256Prepared_t &prepared,
		const void *data, std::size_t dataBytes,
		std::uint8_t digest[ SHA256_DIGEST_BYTES ] ) {
	sha256Context_t inner = prepared.inner;
	sha256Context_t outer = prepared.outer;
	std::uint8_t innerDigest[ SHA256_DIGEST_BYTES ];
	SHA256Update( inner, data, dataBytes );
	SHA256Final( inner, innerDigest );
	SHA256Update( outer, innerDigest, sizeof( innerDigest ) );
	SHA256Final( outer, digest );
	SecureZero( innerDigest, sizeof( innerDigest ) );
}

} // namespace

void SHA256( const void *data, std::size_t dataBytes,
		std::uint8_t digest[ SHA256_DIGEST_BYTES ] ) {
	sha256Context_t context;
	SHA256Init( context );
	if ( dataBytes != 0 ) {
		SHA256Update( context, data, dataBytes );
	}
	SHA256Final( context, digest );
}

void HMACSHA256( const void *key, std::size_t keyBytes,
		const void *data, std::size_t dataBytes,
		std::uint8_t digest[ SHA256_DIGEST_BYTES ] ) {
	hmacSHA256Prepared_t prepared;
	HMACPrepare( key, keyBytes, prepared );
	HMACFinish( prepared, data, dataBytes, digest );
	SecureZero( &prepared, sizeof( prepared ) );
}

bool PBKDF2HMACSHA256( const void *password, std::size_t passwordBytes,
		const void *salt, std::size_t saltBytes, std::uint32_t iterations,
		void *output, std::size_t outputBytes ) {
	if ( ( passwordBytes != 0 && password == nullptr ) ||
		( saltBytes != 0 && salt == nullptr ) || output == nullptr ||
		outputBytes == 0 || outputBytes > SHA256_DIGEST_BYTES || iterations == 0 ) {
		return false;
	}

	hmacSHA256Prepared_t prepared;
	HMACPrepare( password, passwordBytes, prepared );
	sha256Context_t firstInner = prepared.inner;
	const std::uint8_t blockIndex[ 4 ] = { 0, 0, 0, 1 };
	std::uint8_t iteration[ SHA256_DIGEST_BYTES ];
	std::uint8_t aggregate[ SHA256_DIGEST_BYTES ];

	if ( saltBytes != 0 ) {
		SHA256Update( firstInner, salt, saltBytes );
	}
	SHA256Update( firstInner, blockIndex, sizeof( blockIndex ) );
	std::uint8_t firstDigest[ SHA256_DIGEST_BYTES ];
	SHA256Final( firstInner, firstDigest );
	sha256Context_t firstOuter = prepared.outer;
	SHA256Update( firstOuter, firstDigest, sizeof( firstDigest ) );
	SHA256Final( firstOuter, iteration );
	std::memcpy( aggregate, iteration, sizeof( aggregate ) );
	SecureZero( firstDigest, sizeof( firstDigest ) );

	for ( std::uint32_t round = 1; round < iterations; ++round ) {
		std::uint8_t next[ SHA256_DIGEST_BYTES ];
		HMACFinish( prepared, iteration, sizeof( iteration ), next );
		for ( std::size_t index = 0; index < sizeof( aggregate ); ++index ) {
			aggregate[ index ] ^= next[ index ];
		}
		std::memcpy( iteration, next, sizeof( iteration ) );
		SecureZero( next, sizeof( next ) );
	}

	std::memcpy( output, aggregate, outputBytes );
	SecureZero( iteration, sizeof( iteration ) );
	SecureZero( aggregate, sizeof( aggregate ) );
	SecureZero( &prepared, sizeof( prepared ) );
	return true;
}

bool ConstantTimeEquals( const void *left, const void *right, std::size_t bytes ) {
	if ( bytes == 0 ) {
		return true;
	}
	if ( left == nullptr || right == nullptr ) {
		return false;
	}
	const std::uint8_t *leftBytes = static_cast<const std::uint8_t *>( left );
	const std::uint8_t *rightBytes = static_cast<const std::uint8_t *>( right );
	volatile std::uint8_t difference = 0;
	for ( std::size_t index = 0; index < bytes; ++index ) {
		difference |= leftBytes[ index ] ^ rightBytes[ index ];
	}
	return difference == 0;
}

void SecureZero( void *memory, std::size_t bytes ) {
	volatile std::uint8_t *cursor = static_cast<volatile std::uint8_t *>( memory );
	while ( bytes-- != 0 ) {
		*cursor++ = 0;
	}
}

} // namespace idCrypto
