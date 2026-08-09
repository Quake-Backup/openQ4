/*
===========================================================================

openQ4 network endpoint parsing helpers.

These helpers intentionally have no engine dependencies so the address grammar
used by the platform socket layers can also be covered by the native tests.

===========================================================================
*/

#ifndef __NETWORK_ENDPOINT_H__
#define __NETWORK_ENDPOINT_H__

#include <cstddef>
#include <cstring>

namespace idNetworkEndpoint {

struct endpointParts_t {
	bool			hasPort;
	unsigned short	port;
};

inline bool ParsePort( const char *begin, const char *end, unsigned short &port ) {
	if ( begin == NULL || end == NULL || begin == end ) {
		return false;
	}

	unsigned int parsedPort = 0;
	for ( const char *cursor = begin; cursor != end; ++cursor ) {
		if ( *cursor < '0' || *cursor > '9' ) {
			return false;
		}
		const unsigned int digit = static_cast<unsigned int>( *cursor - '0' );
		// Check before multiplying so an arbitrarily long decimal string cannot
		// wrap the accumulator back into the valid 16-bit range.
		if ( parsedPort > 6553 || ( parsedPort == 6553 && digit > 5 ) ) {
			return false;
		}
		parsedPort = parsedPort * 10 + digit;
	}

	port = static_cast<unsigned short>( parsedPort );
	return true;
}

inline bool CopyHost( const char *begin, const char *end, char *host, size_t hostSize ) {
	if ( begin == NULL || end == NULL || host == NULL || hostSize == 0 || begin == end || end < begin ) {
		return false;
	}

	const size_t hostLength = static_cast<size_t>( end - begin );
	if ( hostLength >= hostSize ) {
		return false;
	}

	memcpy( host, begin, hostLength );
	host[hostLength] = '\0';
	return true;
}

// Accepts host, host:port, an unbracketed IPv6 literal without a port, or the
// standard [IPv6]:port form. A single colon always introduces a port, which
// keeps the legacy IPv4/hostname grammar strict and unambiguous.
inline bool Split( const char *text, char *host, size_t hostSize, endpointParts_t &parts ) {
	parts.hasPort = false;
	parts.port = 0;
	if ( host != NULL && hostSize > 0 ) {
		host[0] = '\0';
	}
	if ( text == NULL || text[0] == '\0' || host == NULL || hostSize == 0 ) {
		return false;
	}

	const char *textEnd = text + strlen( text );
	if ( text[0] == '[' ) {
		const char *closeBracket = strchr( text + 1, ']' );
		if ( closeBracket == NULL || !CopyHost( text + 1, closeBracket, host, hostSize ) ) {
			return false;
		}
		if ( closeBracket + 1 == textEnd ) {
			return true;
		}
		if ( closeBracket[1] != ':' || !ParsePort( closeBracket + 2, textEnd, parts.port ) ) {
			host[0] = '\0';
			return false;
		}
		parts.hasPort = true;
		return true;
	}

	const char *firstColon = strchr( text, ':' );
	const char *lastColon = strrchr( text, ':' );
	if ( firstColon != NULL && firstColon == lastColon ) {
		if ( !CopyHost( text, firstColon, host, hostSize ) || !ParsePort( firstColon + 1, textEnd, parts.port ) ) {
			host[0] = '\0';
			return false;
		}
		parts.hasPort = true;
		return true;
	}

	return CopyHost( text, textEnd, host, hostSize );
}

} // namespace idNetworkEndpoint

#endif /* !__NETWORK_ENDPOINT_H__ */
