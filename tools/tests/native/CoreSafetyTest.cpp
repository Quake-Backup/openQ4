#include "src/idlib/NumericString.h"
#include "src/idlib/StrAllocation.h"
#include "src/sys/NetworkEndpoint.h"

#include <climits>
#include <cstdio>
#include <limits>

static int failures = 0;

static void ExpectDecimal( const char *text, const bool expected, const char *label ) {
	const bool actual = idNumericString::IsDecimal( text );
	if ( actual != expected ) {
		std::fprintf( stderr, "IsDecimal failed for %s: expected %d, got %d\n", label, expected, actual );
		failures++;
	}
}

static void ExpectBounded( const char *text, const int maximum, const bool expected,
		const int expectedValue, const char *label ) {
	int value = -7331;
	const bool actual = idNumericString::ParseUnsignedBounded( text, maximum, value );
	if ( actual != expected ) {
		std::fprintf( stderr, "ParseUnsignedBounded failed for %s: expected %d, got %d\n", label, expected, actual );
		failures++;
		return;
	}
	if ( actual && value != expectedValue ) {
		std::fprintf( stderr, "ParseUnsignedBounded value failed for %s: expected %d, got %d\n", label, expectedValue, value );
		failures++;
	}
	if ( !actual && value != -7331 ) {
		std::fprintf( stderr, "ParseUnsignedBounded modified output on failure for %s\n", label );
		failures++;
	}
}

static void ExpectAllocation( const bool condition, const char *label ) {
	if ( !condition ) {
		std::fprintf( stderr, "String allocation arithmetic failed for %s\n", label );
		failures++;
	}
}

static void ExpectEndpoint( const char *text, const bool expected, const char *expectedHost,
		const bool expectedHasPort, const unsigned short expectedPort, const char *label ) {
	char host[256] = "untouched";
	idNetworkEndpoint::endpointParts_t parts;
	const bool actual = idNetworkEndpoint::Split( text, host, sizeof( host ), parts );
	if ( actual != expected ) {
		std::fprintf( stderr, "Network endpoint split failed for %s: expected %d, got %d\n", label, expected, actual );
		failures++;
		return;
	}
	if ( !actual ) {
		if ( host[0] != '\0' || parts.hasPort || parts.port != 0 ) {
			std::fprintf( stderr, "Network endpoint split left output on failure for %s\n", label );
			failures++;
		}
		return;
	}
	if ( std::strcmp( host, expectedHost ) != 0 || parts.hasPort != expectedHasPort || parts.port != expectedPort ) {
		std::fprintf(
			stderr,
			"Network endpoint split value failed for %s: got host='%s', hasPort=%d, port=%u\n",
			label,
			host,
			parts.hasPort,
			static_cast<unsigned int>( parts.port )
		);
		failures++;
	}
}

int main() {
	const char *validDecimals[] = {
		"0", "-0", "123", "-123", "1.0", ".5", "-.5", "5."
	};
	for ( const char *value : validDecimals ) {
		ExpectDecimal( value, true, value );
	}

	const char *invalidDecimals[] = {
		"", "-", ".", "-.", "+1", "1..0", "1e3", " 1", "1 ", "--1", "one"
	};
	ExpectDecimal( nullptr, false, "null" );
	for ( const char *value : invalidDecimals ) {
		ExpectDecimal( value, false, value[ 0 ] == '\0' ? "empty" : value );
	}

	ExpectBounded( "0", 127, true, 0, "zero" );
	ExpectBounded( "127", 127, true, 127, "maximum" );
	ExpectBounded( "000127", 127, true, 127, "leading zeroes" );
	ExpectBounded( "128", 127, false, 0, "one above maximum" );
	ExpectBounded( "999999999999999999999999999999", 127, false, 0, "overflow-sized input" );
	ExpectBounded( "", 127, false, 0, "empty" );
	ExpectBounded( nullptr, 127, false, 0, "null" );
	ExpectBounded( "-1", 127, false, 0, "negative" );
	ExpectBounded( "12x", 127, false, 0, "non-digit" );
	ExpectBounded( "0", -1, false, 0, "negative maximum" );

	using namespace idStrAllocationDetail;
	const size_t sizeMaximum = ( std::numeric_limits<size_t>::max )();
	ExpectAllocation( SaturatingAdd( 17, 25 ) == 42, "ordinary addition" );
	ExpectAllocation( SaturatingAdd( sizeMaximum, 1 ) == sizeMaximum, "saturating addition" );
	ExpectAllocation( SaturatingMultiply( 6, 7 ) == 42, "ordinary multiplication" );
	ExpectAllocation( SaturatingMultiply( sizeMaximum, 2 ) == sizeMaximum, "saturating multiplication" );

	int roundedAmount = 0;
	const size_t maximumRoundedAllocation = static_cast<size_t>( INT_MAX ) / 32 * 32;
	ExpectAllocation( TryRoundUpToInt( 1, 32, roundedAmount ) && roundedAmount == 32, "small round-up" );
	ExpectAllocation(
		TryRoundUpToInt( maximumRoundedAllocation, 32, roundedAmount ) &&
			roundedAmount == static_cast<int>( maximumRoundedAllocation ),
		"largest representable rounded allocation"
	);
	ExpectAllocation( !TryRoundUpToInt( maximumRoundedAllocation + 1, 32, roundedAmount ), "first unrepresentable round-up" );
	ExpectAllocation( !TryRoundUpToInt( static_cast<size_t>( INT_MAX ), 32, roundedAmount ), "INT_MAX round-up" );
	ExpectAllocation( !TryRoundUpToInt( sizeMaximum, 32, roundedAmount ), "SIZE_MAX round-up" );
	ExpectAllocation( !TryRoundUpToInt( 1, 0, roundedAmount ), "zero granularity" );

	ExpectEndpoint( "127.0.0.1", true, "127.0.0.1", false, 0, "numeric IPv4" );
	ExpectEndpoint( "127.0.0.1:65535", true, "127.0.0.1", true, 65535, "maximum IPv4 port" );
	ExpectEndpoint( "4.example.com:27960", true, "4.example.com", true, 27960, "digit-leading hostname" );
	ExpectEndpoint( "255.255.255.255", true, "255.255.255.255", false, 0, "IPv4 broadcast literal" );
	ExpectEndpoint( "hostname:0", true, "hostname", true, 0, "explicit zero port" );
	ExpectEndpoint( "::1", true, "::1", false, 0, "unbracketed IPv6 literal" );
	ExpectEndpoint( "[::1]:27960", true, "::1", true, 27960, "bracketed IPv6 endpoint" );
	ExpectEndpoint( "[fe80::1%3]", true, "fe80::1%3", false, 0, "scoped IPv6 literal" );

	ExpectEndpoint( nullptr, false, "", false, 0, "null endpoint" );
	ExpectEndpoint( "", false, "", false, 0, "empty endpoint" );
	ExpectEndpoint( ":27960", false, "", false, 0, "missing host" );
	ExpectEndpoint( "host:", false, "", false, 0, "missing port" );
	ExpectEndpoint( "host:-1", false, "", false, 0, "negative port" );
	ExpectEndpoint( "host:65536", false, "", false, 0, "out-of-range port" );
	ExpectEndpoint( "host:184467440737095516160", false, "", false, 0, "overflowing decimal port" );
	ExpectEndpoint( "host:12x", false, "", false, 0, "non-numeric port" );
	ExpectEndpoint( "[::1", false, "", false, 0, "missing close bracket" );
	ExpectEndpoint( "[]:27960", false, "", false, 0, "empty bracketed host" );
	ExpectEndpoint( "[::1]junk", false, "", false, 0, "text after close bracket" );

	if ( failures != 0 ) {
		std::fprintf( stderr, "core safety native test: %d failure(s)\n", failures );
		return 1;
	}

	std::printf( "core safety native test: ok\n" );
	return 0;
}
