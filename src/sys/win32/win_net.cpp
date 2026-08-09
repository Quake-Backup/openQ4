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



#include <iptypes.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>

#include "win_local.h"
#include "../NetworkEndpoint.h"

static WSADATA	winsockdata;
static bool	winsockInitialized = false;
static bool usingSocks = false;

idCVar net_ip( "net_ip", "localhost", CVAR_SYSTEM, "local IP address" );
idCVar net_port( "net_port", "0", CVAR_SYSTEM | CVAR_INTEGER, "local IP port number" );
idCVar net_forceLatency( "net_forceLatency", "0", CVAR_SYSTEM | CVAR_INTEGER, "milliseconds latency" );
idCVar net_forceDrop( "net_forceDrop", "0", CVAR_SYSTEM | CVAR_INTEGER, "percentage packet loss" );

// Retain the legacy CVars for configuration compatibility, but do not expose
// the inherited SOCKS UDP relay as supported. It assumes one global IPv4
// socket and does not satisfy idPort's current lifecycle or test contracts.
idCVar net_socksEnabled( "net_socksEnabled", "0", CVAR_SYSTEM | CVAR_ARCHIVE | CVAR_BOOL, "" );
idCVar net_socksServer( "net_socksServer", "", CVAR_SYSTEM | CVAR_ARCHIVE, "" );
idCVar net_socksPort( "net_socksPort", "1080", CVAR_SYSTEM | CVAR_ARCHIVE | CVAR_INTEGER, "" );
idCVar net_socksUsername( "net_socksUsername", "", CVAR_SYSTEM | CVAR_ARCHIVE, "" );
idCVar net_socksPassword( "net_socksPassword", "", CVAR_SYSTEM | CVAR_ARCHIVE, "" );


static struct sockaddr	socksRelayAddr;

static SOCKET	ip_socket = INVALID_SOCKET;
static SOCKET	socks_socket = INVALID_SOCKET;
static char		socksBuf[4096];

typedef struct {
	uint32_t ip;
	uint32_t mask;
} net_interface;

#define 		MAX_INTERFACES	32
int				num_interfaces = 0;
net_interface	netint[MAX_INTERFACES];

//=============================================================================


/*
====================
NET_ErrorString
====================
*/
char *NET_ErrorString( void ) {
	int		code;

	code = WSAGetLastError();
	switch( code ) {
	case WSAEINTR: return "WSAEINTR";
	case WSAEBADF: return "WSAEBADF";
	case WSAEACCES: return "WSAEACCES";
	case WSAEDISCON: return "WSAEDISCON";
	case WSAEFAULT: return "WSAEFAULT";
	case WSAEINVAL: return "WSAEINVAL";
	case WSAEMFILE: return "WSAEMFILE";
	case WSAEWOULDBLOCK: return "WSAEWOULDBLOCK";
	case WSAEINPROGRESS: return "WSAEINPROGRESS";
	case WSAEALREADY: return "WSAEALREADY";
	case WSAENOTSOCK: return "WSAENOTSOCK";
	case WSAEDESTADDRREQ: return "WSAEDESTADDRREQ";
	case WSAEMSGSIZE: return "WSAEMSGSIZE";
	case WSAEPROTOTYPE: return "WSAEPROTOTYPE";
	case WSAENOPROTOOPT: return "WSAENOPROTOOPT";
	case WSAEPROTONOSUPPORT: return "WSAEPROTONOSUPPORT";
	case WSAESOCKTNOSUPPORT: return "WSAESOCKTNOSUPPORT";
	case WSAEOPNOTSUPP: return "WSAEOPNOTSUPP";
	case WSAEPFNOSUPPORT: return "WSAEPFNOSUPPORT";
	case WSAEAFNOSUPPORT: return "WSAEAFNOSUPPORT";
	case WSAEADDRINUSE: return "WSAEADDRINUSE";
	case WSAEADDRNOTAVAIL: return "WSAEADDRNOTAVAIL";
	case WSAENETDOWN: return "WSAENETDOWN";
	case WSAENETUNREACH: return "WSAENETUNREACH";
	case WSAENETRESET: return "WSAENETRESET";
	case WSAECONNABORTED: return "WSAECONNABORTED";
	case WSAECONNRESET: return "WSAECONNRESET";
	case WSAENOBUFS: return "WSAENOBUFS";
	case WSAEISCONN: return "WSAEISCONN";
	case WSAENOTCONN: return "WSAENOTCONN";
	case WSAESHUTDOWN: return "WSAESHUTDOWN";
	case WSAETOOMANYREFS: return "WSAETOOMANYREFS";
	case WSAETIMEDOUT: return "WSAETIMEDOUT";
	case WSAECONNREFUSED: return "WSAECONNREFUSED";
	case WSAELOOP: return "WSAELOOP";
	case WSAENAMETOOLONG: return "WSAENAMETOOLONG";
	case WSAEHOSTDOWN: return "WSAEHOSTDOWN";
	case WSASYSNOTREADY: return "WSASYSNOTREADY";
	case WSAVERNOTSUPPORTED: return "WSAVERNOTSUPPORTED";
	case WSANOTINITIALISED: return "WSANOTINITIALISED";
	case WSAHOST_NOT_FOUND: return "WSAHOST_NOT_FOUND";
	case WSATRY_AGAIN: return "WSATRY_AGAIN";
	case WSANO_RECOVERY: return "WSANO_RECOVERY";
	case WSANO_DATA: return "WSANO_DATA";
	default: return "NO ERROR";
	}
}

/*
====================
Net_NetadrToSockadr
====================
*/
static bool Net_NetadrToSockadr( const netadr_t *a, struct sockaddr_in *s ) {
	if ( a == NULL || s == NULL ) {
		return false;
	}

	memset( s, 0, sizeof( *s ) );
	s->sin_family = AF_INET;
	s->sin_port = htons( a->port );

	if ( a->type == NA_BROADCAST ) {
		s->sin_addr.s_addr = INADDR_BROADCAST;
		return true;
	}
	if ( a->type == NA_LOOPBACK ) {
		s->sin_addr.s_addr = htonl( INADDR_LOOPBACK );
		return true;
	}
	if ( a->type == NA_IP ) {
		memcpy( &s->sin_addr.s_addr, a->ip, sizeof( a->ip ) );
		return true;
	}

	memset( s, 0, sizeof( *s ) );
	return false;
}


/*
====================
Net_SockadrToNetadr
====================
*/
static bool Net_SockadrToNetadr( const struct sockaddr_in *s, netadr_t *a ) {
	if ( s == NULL || a == NULL ) {
		return false;
	}

	memset( a, 0, sizeof( *a ) );
	a->type = NA_BAD;
	if ( s->sin_family != AF_INET ) {
		return false;
	}

	const unsigned int ip = s->sin_addr.s_addr;
	memcpy( a->ip, &ip, sizeof( a->ip ) );
	a->port = ntohs( s->sin_port );
	a->type = ( ntohl( ip ) == INADDR_LOOPBACK ) ? NA_LOOPBACK : NA_IP;
	return true;
}

/*
=============
Net_StringToSockaddr
=============
*/
static bool Net_StringToSockaddr( const char *text, struct sockaddr_in *address, bool doDNSResolve, int defaultPort = 0, int socketType = SOCK_DGRAM ) {
	if ( address == NULL ) {
		return false;
	}
	memset( address, 0, sizeof( *address ) );

	char host[NI_MAXHOST];
	idNetworkEndpoint::endpointParts_t endpoint;
	if ( !idNetworkEndpoint::Split( text, host, sizeof( host ), endpoint ) ) {
		return false;
	}

	const int effectivePort = endpoint.hasPort ? endpoint.port : defaultPort;
	if ( effectivePort < 0 || effectivePort > 65535 ) {
		return false;
	}
	char service[NI_MAXSERV];
	idStr::snPrintf( service, sizeof( service ), "%d", effectivePort );

	struct addrinfo hints;
	memset( &hints, 0, sizeof( hints ) );
	hints.ai_family = AF_INET;
	hints.ai_socktype = socketType;
	hints.ai_protocol = socketType == SOCK_STREAM ? IPPROTO_TCP : IPPROTO_UDP;
	hints.ai_flags = doDNSResolve ? 0 : AI_NUMERICHOST;

	struct addrinfo *results = NULL;
	const int error = getaddrinfo( host, service, &hints, &results );
	if ( error != 0 ) {
		return false;
	}

	bool resolved = false;
	for ( const struct addrinfo *result = results; result != NULL; result = result->ai_next ) {
		if ( result->ai_family != AF_INET || result->ai_addr == NULL || result->ai_addrlen < sizeof( *address ) ) {
			continue;
		}
		memcpy( address, result->ai_addr, sizeof( *address ) );
		resolved = true;
		break;
	}

	freeaddrinfo( results );
	return resolved;
}

/*
====================
NET_IPSocket
====================
*/
SOCKET NET_IPSocket( const char *net_interface, int port, netadr_t *bound_to ) {
	SOCKET				newsocket;
	struct sockaddr_in	address;
	unsigned long		_true = 1;
	int					i = 1;
	int					err;

	if ( bound_to != NULL ) {
		memset( bound_to, 0, sizeof( *bound_to ) );
		bound_to->type = NA_BAD;
	}
	if ( port != PORT_ANY && ( port < 0 || port > 65535 ) ) {
		common->Printf( "WARNING: UDP_OpenSocket: invalid port %d\n", port );
		return INVALID_SOCKET;
	}

	if( net_interface ) {
		common->DPrintf( "Opening IP socket: %s:%i\n", net_interface, port );
	} else {
		common->DPrintf( "Opening IP socket: localhost:%i\n", port );
	}

	if( ( newsocket = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP ) ) == INVALID_SOCKET ) {
		err = WSAGetLastError();
		if( err != WSAEAFNOSUPPORT ) {
			common->Printf( "WARNING: UDP_OpenSocket: socket: %s\n", NET_ErrorString() );
		}
		return INVALID_SOCKET;
	}

	// make it non-blocking
	if( ioctlsocket( newsocket, FIONBIO, &_true ) == SOCKET_ERROR ) {
		common->Printf( "WARNING: UDP_OpenSocket: ioctl FIONBIO: %s\n", NET_ErrorString() );
		closesocket( newsocket );
		return INVALID_SOCKET;
	}

	// make it broadcast capable
	if( setsockopt( newsocket, SOL_SOCKET, SO_BROADCAST, (char *)&i, sizeof(i) ) == SOCKET_ERROR ) {
		common->Printf( "WARNING: UDP_OpenSocket: setsockopt SO_BROADCAST: %s\n", NET_ErrorString() );
		closesocket( newsocket );
		return INVALID_SOCKET;
	}

	memset( &address, 0, sizeof( address ) );
	if( !net_interface || !net_interface[0] || !idStr::Icmp( net_interface, "localhost" ) ) {
		address.sin_addr.s_addr = INADDR_ANY;
	}
	else if ( !Net_StringToSockaddr( net_interface, &address, true ) ) {
		common->Printf( "WARNING: UDP_OpenSocket: invalid interface address '%s'\n", net_interface );
		closesocket( newsocket );
		return INVALID_SOCKET;
	}

	if( port == PORT_ANY ) {
		address.sin_port = 0;
	}
	else {
		address.sin_port = htons( static_cast<unsigned short>( port ) );
	}

	address.sin_family = AF_INET;

	if( bind( newsocket, (const struct sockaddr *)&address, sizeof(address) ) == SOCKET_ERROR ) {
		common->Printf( "WARNING: UDP_OpenSocket: bind: %s\n", NET_ErrorString() );
		closesocket( newsocket );
		return INVALID_SOCKET;
	}

	// if the port was PORT_ANY, we need to query again to know the real port we got bound to
	// ( this used to be in idPort::InitForPort )
	if ( bound_to ) {
		int len = sizeof( address );
		if ( getsockname( newsocket, reinterpret_cast<sockaddr *>( &address ), &len ) == SOCKET_ERROR || !Net_SockadrToNetadr( &address, bound_to ) ) {
			common->Printf( "WARNING: UDP_OpenSocket: getsockname: %s\n", NET_ErrorString() );
			closesocket( newsocket );
			return INVALID_SOCKET;
		}
	}

	return newsocket;
}

/*
====================
NET_OpenSocks
====================
*/
void NET_OpenSocks( int port ) {
	struct sockaddr_in	address;
	int					err;
	int					len;
	bool			rfc1929;
	unsigned char		buf[64];

	usingSocks = false;

	common->Printf( "Opening connection to SOCKS server.\n" );

	if ( ( socks_socket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP ) ) == INVALID_SOCKET ) {
		err = WSAGetLastError();
		common->Printf( "WARNING: NET_OpenSocks: socket: %s\n", NET_ErrorString() );
		return;
	}

	if ( !Net_StringToSockaddr( net_socksServer.GetString(), &address, true, net_socksPort.GetInteger(), SOCK_STREAM ) ) {
		common->Printf( "WARNING: NET_OpenSocks: could not resolve IPv4 server '%s'\n", net_socksServer.GetString() );
		closesocket( socks_socket );
		socks_socket = INVALID_SOCKET;
		return;
	}

	if ( connect( socks_socket, (struct sockaddr *)&address, sizeof( address ) ) == SOCKET_ERROR ) {
		err = WSAGetLastError();
		common->Printf( "NET_OpenSocks: connect: %s\n", NET_ErrorString() );
		return;
	}

	// send socks authentication handshake
	if ( *net_socksUsername.GetString() || *net_socksPassword.GetString() ) {
		rfc1929 = true;
	}
	else {
		rfc1929 = false;
	}

	buf[0] = 5;		// SOCKS version
	// method count
	if ( rfc1929 ) {
		buf[1] = 2;
		buf[2] = 0;		// method #1 - method id #00: no authentication
		buf[3] = 2;		// method #2 - method id #02: username/password
		len = 4;
	}
	else {
		buf[1] = 1;
		buf[2] = 0;		// method #1 - method id #00: no authentication
		len = 3;
	}
	if ( send( socks_socket, (const char *)buf, len, 0 ) == SOCKET_ERROR ) {
		err = WSAGetLastError();
		common->Printf( "NET_OpenSocks: send: %s\n", NET_ErrorString() );
		return;
	}

	// get the response
	len = recv( socks_socket, (char *)buf, 64, 0 );
	if ( len == SOCKET_ERROR ) {
		err = WSAGetLastError();
		common->Printf( "NET_OpenSocks: recv: %s\n", NET_ErrorString() );
		return;
	}
	if ( len != 2 || buf[0] != 5 ) {
		common->Printf( "NET_OpenSocks: bad response\n" );
		return;
	}
	switch( buf[1] ) {
	case 0:	// no authentication
		break;
	case 2: // username/password authentication
		break;
	default:
		common->Printf( "NET_OpenSocks: request denied\n" );
		return;
	}

	// do username/password authentication if needed
	if ( buf[1] == 2 ) {
		size_t	ulen;
		size_t	plen;

		// build the request
		ulen = strlen( net_socksUsername.GetString() );
		plen = strlen( net_socksPassword.GetString() );
		if ( ulen > 255 || plen > 255 || 3 + ulen + plen > sizeof( buf ) ) {
			common->Printf( "NET_OpenSocks: username/password too long\n" );
			return;
		}

		buf[0] = 1;		// username/password authentication version
		buf[1] = (unsigned char)ulen;
		if ( ulen ) {
			memcpy( &buf[2], net_socksUsername.GetString(), ulen );
		}
		buf[2 + ulen] = (unsigned char)plen;
		if ( plen ) {
			memcpy( &buf[3 + ulen], net_socksPassword.GetString(), plen );
		}

		// send it
		if ( send( socks_socket, (const char *)buf, (int)( 3 + ulen + plen ), 0 ) == SOCKET_ERROR ) {
			err = WSAGetLastError();
			common->Printf( "NET_OpenSocks: send: %s\n", NET_ErrorString() );
			return;
		}

		// get the response
		len = recv( socks_socket, (char *)buf, 64, 0 );
		if ( len == SOCKET_ERROR ) {
			err = WSAGetLastError();
			common->Printf( "NET_OpenSocks: recv: %s\n", NET_ErrorString() );
			return;
		}
		if ( len != 2 || buf[0] != 1 ) {
			common->Printf( "NET_OpenSocks: bad response\n" );
			return;
		}
		if ( buf[1] != 0 ) {
			common->Printf( "NET_OpenSocks: authentication failed\n" );
			return;
		}
	}

	// send the UDP associate request
	buf[0] = 5;		// SOCKS version
	buf[1] = 3;		// command: UDP associate
	buf[2] = 0;		// reserved
	buf[3] = 1;		// address type: IPV4
	const unsigned int anyAddress = INADDR_ANY;
	const unsigned short networkPort = htons( static_cast<unsigned short>( port ) );
	memcpy( &buf[4], &anyAddress, sizeof( anyAddress ) );
	memcpy( &buf[8], &networkPort, sizeof( networkPort ) );
	if ( send( socks_socket, (const char *)buf, 10, 0 ) == SOCKET_ERROR ) {
		err = WSAGetLastError();
		common->Printf( "NET_OpenSocks: send: %s\n", NET_ErrorString() );
		return;
	}

	// get the response
	len = recv( socks_socket, (char *)buf, 64, 0 );
	if( len == SOCKET_ERROR ) {
		err = WSAGetLastError();
		common->Printf( "NET_OpenSocks: recv: %s\n", NET_ErrorString() );
		return;
	}
	if( len < 2 || buf[0] != 5 ) {
		common->Printf( "NET_OpenSocks: bad response\n" );
		return;
	}
	// check completion code
	if( buf[1] != 0 ) {
		common->Printf( "NET_OpenSocks: request denied: %i\n", buf[1] );
		return;
	}
	if( buf[3] != 1 ) {
		common->Printf( "NET_OpenSocks: relay address is not IPV4: %i\n", buf[3] );
		return;
	}
	((struct sockaddr_in *)&socksRelayAddr)->sin_family = AF_INET;
	memcpy( &((struct sockaddr_in *)&socksRelayAddr)->sin_addr.s_addr, &buf[4], sizeof( ((struct sockaddr_in *)&socksRelayAddr)->sin_addr.s_addr ) );
	memcpy( &((struct sockaddr_in *)&socksRelayAddr)->sin_port, &buf[8], sizeof( ((struct sockaddr_in *)&socksRelayAddr)->sin_port ) );
	memset( ((struct sockaddr_in *)&socksRelayAddr)->sin_zero, 0, 8 );

	usingSocks = true;
}

/*
==================
Net_WaitForUDPPacket
==================
*/
bool Net_WaitForUDPPacket( SOCKET netSocket, int timeout ) {
	int					ret;
	fd_set				set;
	struct timeval		tv;

	if ( netSocket == INVALID_SOCKET ) {
		return false;
	}

	if ( timeout <= 0 ) {
		return true;
	}

	FD_ZERO( &set );
	FD_SET( netSocket, &set );

	tv.tv_sec = timeout / 1000;
	tv.tv_usec = ( timeout % 1000 ) * 1000;

	ret = select( 0, &set, NULL, NULL, &tv );

	if ( ret == SOCKET_ERROR ) {
		common->DPrintf( "Net_WaitForUDPPacket select(): %s\n", NET_ErrorString() );
		return false;
	}

	// timeout with no data
	if ( ret == 0 ) {
		return false;
	}

	return true;
}

/*
==================
Net_GetUDPPacket
==================
*/
bool Net_GetUDPPacket( SOCKET netSocket, netadr_t &net_from, char *data, int &size, int maxSize ) {
	int 			ret;
	struct sockaddr_in from;
	int				fromlen;
	int				err;

	size = 0;
	memset( &net_from, 0, sizeof( net_from ) );
	net_from.type = NA_BAD;

	if( netSocket == INVALID_SOCKET ) {
		return false;
	}
	if ( data == NULL || maxSize <= 0 ) {
		return false;
	}

	memset( &from, 0, sizeof( from ) );
	fromlen = sizeof( from );
	ret = recvfrom( netSocket, data, maxSize, 0, reinterpret_cast<struct sockaddr *>( &from ), &fromlen );
	if ( ret == SOCKET_ERROR ) {
		err = WSAGetLastError();

		if( err == WSAEWOULDBLOCK || err == WSAECONNRESET ) {
			return false;
		}
		if ( err == WSAEMSGSIZE ) {
			if ( Net_SockadrToNetadr( &from, &net_from ) ) {
				char buf[1024];
				idStr::snPrintf( buf, sizeof( buf ), "Net_GetUDPPacket: oversize packet from %s\n", Sys_NetAdrToString( net_from ) );
				OutputDebugString( buf );
			}
			memset( &net_from, 0, sizeof( net_from ) );
			net_from.type = NA_BAD;
			return false;
		}
		char	buf[1024];
		idStr::snPrintf( buf, sizeof( buf ), "Net_GetUDPPacket: %s\n", NET_ErrorString() );
		OutputDebugString( buf );
		return false;
	}

	if ( netSocket == ip_socket ) {
		memset( from.sin_zero, 0, sizeof( from.sin_zero ) );
	}

	if ( usingSocks && netSocket == ip_socket && fromlen == sizeof( from ) && memcmp( &from, &socksRelayAddr, sizeof( from ) ) == 0 ) {
		if ( ret < 10 || data[0] != 0 || data[1] != 0 || data[2] != 0 || data[3] != 1 ) {
			return false;
		}
		net_from.type = NA_IP;
		net_from.ip[0] = data[4];
		net_from.ip[1] = data[5];
		net_from.ip[2] = data[6];
		net_from.ip[3] = data[7];
		unsigned short networkPort;
		memcpy( &networkPort, &data[8], sizeof( networkPort ) );
		net_from.port = ntohs( networkPort );
		memmove( data, &data[10], ret - 10 );
		ret -= 10;
	} else if ( !Net_SockadrToNetadr( &from, &net_from ) ) {
		return false;
	}

	size = ret;

	return true;
}

/*
==================
Net_SendUDPPacket
==================
*/
void Net_SendUDPPacket( SOCKET netSocket, int length, const void *data, const netadr_t to ) {
	int				ret;
	struct sockaddr_in addr;
	const bool verbosePacket = cvarSystem != NULL && cvarSystem->GetCVarInteger( "net_verbose" ) >= 2;

	if( netSocket == INVALID_SOCKET ) {
		return;
	}
	if ( length < 0 || ( data == NULL && length > 0 ) ) {
		return;
	}

	if ( !Net_NetadrToSockadr( &to, &addr ) ) {
		return;
	}
	const char emptyPacket = '\0';
	const char *packetData = data != NULL ? static_cast<const char *>( data ) : &emptyPacket;

	if( usingSocks && to.type == NA_IP ) {
		if ( length > (int)sizeof( socksBuf ) - 10 ) {
			char	buf[1024];
			idStr::snPrintf( buf, sizeof( buf ), "Net_SendUDPPacket: oversized SOCKS packet to %s\n", Sys_NetAdrToString( to ) );
			OutputDebugString( buf );
			return;
		}
		socksBuf[0] = 0;	// reserved
		socksBuf[1] = 0;
		socksBuf[2] = 0;	// fragment (not fragmented)
		socksBuf[3] = 1;	// address type: IPV4
		memcpy( &socksBuf[4], &addr.sin_addr.s_addr, sizeof( addr.sin_addr.s_addr ) );
		memcpy( &socksBuf[8], &addr.sin_port, sizeof( addr.sin_port ) );
		memcpy( &socksBuf[10], packetData, length );
		ret = sendto( netSocket, socksBuf, length+10, 0, &socksRelayAddr, sizeof(socksRelayAddr) );
	} else {
		ret = sendto( netSocket, packetData, length, 0, reinterpret_cast<const struct sockaddr *>( &addr ), sizeof( addr ) );
	}
	if( ret == SOCKET_ERROR ) {
		int err = WSAGetLastError();

		// wouldblock is silent
		if( err == WSAEWOULDBLOCK ) {
			return;
		}

		// some PPP links do not allow broadcasts and return an error
		if( ( err == WSAEADDRNOTAVAIL ) && ( to.type == NA_BROADCAST ) ) {
			return;
		}

		const char *errorString = NET_ErrorString();
		char	buf[1024];
		idStr::snPrintf( buf, sizeof( buf ), "Net_SendUDPPacket: %s\n", errorString );
		OutputDebugString( buf );
		common->Printf( "Net_SendUDPPacket ERROR: to %s: %s\n", Sys_NetAdrToString( to ), errorString );
		return;
	}

	if ( verbosePacket ) {
		common->DPrintf( "Net_SendUDPPacket: sent %d bytes to %s\n", ret, Sys_NetAdrToString( to ) );
	}
}

/*
====================
Sys_InitNetworking
====================
*/
void Sys_InitNetworking( void ) {
	int		r;

	if ( winsockInitialized ) {
		return;
	}

	r = WSAStartup( MAKEWORD( 2, 2 ), &winsockdata );
	if( r ) {
		common->Printf( "WARNING: Winsock initialization failed, returned %d\n", r );
		return;
	}
	if ( LOBYTE( winsockdata.wVersion ) != 2 || HIBYTE( winsockdata.wVersion ) != 2 ) {
		common->Printf( "WARNING: Winsock 2.2 is unavailable (received %u.%u)\n", LOBYTE( winsockdata.wVersion ), HIBYTE( winsockdata.wVersion ) );
		WSACleanup();
		return;
	}

	winsockInitialized = true;
	common->Printf( "Winsock Initialized\n" );

	PIP_ADAPTER_INFO pAdapterInfo;
	PIP_ADAPTER_INFO pAdapter = NULL;
	DWORD dwRetVal = 0;
	PIP_ADDR_STRING pIPAddrString;
	ULONG ulOutBufLen;
	bool foundloopback;

	num_interfaces = 0;
	foundloopback = false;

	pAdapterInfo = (IP_ADAPTER_INFO *)malloc( sizeof( IP_ADAPTER_INFO ) );
	if( !pAdapterInfo ) {
		common->FatalError( "Sys_InitNetworking: Couldn't malloc( %zu )", sizeof( IP_ADAPTER_INFO ) );
	}
	ulOutBufLen = sizeof( IP_ADAPTER_INFO );

	// Make an initial call to GetAdaptersInfo to get
	// the necessary size into the ulOutBufLen variable
	if( GetAdaptersInfo( pAdapterInfo, &ulOutBufLen ) == ERROR_BUFFER_OVERFLOW ) {
		free( pAdapterInfo );
		pAdapterInfo = (IP_ADAPTER_INFO *)malloc( ulOutBufLen ); 
		if( !pAdapterInfo ) {
			common->FatalError( "Sys_InitNetworking: Couldn't malloc( %lu )", ulOutBufLen );
		}
	}

	if( ( dwRetVal = GetAdaptersInfo( pAdapterInfo, &ulOutBufLen) ) != NO_ERROR ) {
		// happens if you have no network connection
		common->Printf( "Sys_InitNetworking: GetAdaptersInfo failed (%lu).\n", dwRetVal );
	} else {
		pAdapter = pAdapterInfo;
		while( pAdapter ) {
			common->Printf( "Found interface: %s %s - ", pAdapter->AdapterName, pAdapter->Description );
			pIPAddrString = &pAdapter->IpAddressList;
			while( pIPAddrString ) {
				uint32_t ip_a, ip_m;
				struct sockaddr_in interfaceAddress;
				struct sockaddr_in interfaceMask;
				if ( !Net_StringToSockaddr( pIPAddrString->IpAddress.String, &interfaceAddress, false ) ||
						!Net_StringToSockaddr( pIPAddrString->IpMask.String, &interfaceMask, false ) ) {
					common->Printf( "%s/%s invalid IPv4 data - skipped\n", pIPAddrString->IpAddress.String, pIPAddrString->IpMask.String );
					pIPAddrString = pIPAddrString->Next;
					continue;
				}
				ip_a = ntohl( interfaceAddress.sin_addr.s_addr );
				ip_m = ntohl( interfaceMask.sin_addr.s_addr );
				if ( ( ip_a & 0xff000000u ) == 0x7f000000u ) {
					foundloopback = true;
				}
				//skip null netmasks
				if( !ip_m ) {
					common->Printf( "%s NULL netmask - skipped\n", pIPAddrString->IpAddress.String );
					pIPAddrString = pIPAddrString->Next;
					continue;
				}
				common->Printf( "%s/%s\n", pIPAddrString->IpAddress.String, pIPAddrString->IpMask.String );
				netint[num_interfaces].ip = ip_a;
				netint[num_interfaces].mask = ip_m;
				num_interfaces++;
				if( num_interfaces >= MAX_INTERFACES ) {
					common->Printf( "Sys_InitNetworking: MAX_INTERFACES(%d) hit.\n", MAX_INTERFACES );
					free( pAdapterInfo );
					return;
				}
				pIPAddrString = pIPAddrString->Next;
			}
			pAdapter = pAdapter->Next;
		}
	}
	// for some retarded reason, win32 doesn't count loopback as an adapter...
	if( !foundloopback && num_interfaces < MAX_INTERFACES ) {
		common->Printf( "Sys_InitNetworking: adding loopback interface\n" );
		netint[num_interfaces].ip = 0x7f000001u;
		netint[num_interfaces].mask = 0xff000000u;
		num_interfaces++;
	}
	free( pAdapterInfo );
}


/*
====================
Sys_ShutdownNetworking
====================
*/
void Sys_ShutdownNetworking( void ) {
	if ( !winsockInitialized ) {
		return;
	}
	WSACleanup();
	winsockInitialized = false;
}

/*
=============
Sys_StringToNetAdr
=============
*/
bool Sys_StringToNetAdr( const char *s, netadr_t *a, bool doDNSResolve ) {
	if ( a == NULL ) {
		return false;
	}
	memset( a, 0, sizeof( *a ) );
	a->type = NA_BAD;

	struct sockaddr_in sadr;
	if ( !Net_StringToSockaddr( s, &sadr, doDNSResolve ) ) {
		return false;
	}

	return Net_SockadrToNetadr( &sadr, a );
}

/*
=============
Sys_NetAdrToString
=============
*/
const char *Sys_NetAdrToString( const netadr_t a ) {
	static int index = 0;
	static char buf[ 4 ][ 64 ];	// flip/flop
	char *s;

	s = buf[index];
	index = (index + 1) & 3;
	s[0] = '\0';

	if ( a.type == NA_LOOPBACK ) {
		if ( a.port ) {
			idStr::snPrintf( s, 64, "localhost:%i", a.port );
		} else {
			idStr::snPrintf( s, 64, "localhost" );
		}
	} else if ( a.type == NA_IP ) {
		idStr::snPrintf( s, 64, "%i.%i.%i.%i:%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3], a.port );
	} else if ( a.type == NA_IP6 ) {
		idStr::snPrintf( s, 64, "[ipv6]:%i", a.port );
	} else if ( a.type == NA_BROADCAST ) {
		if ( a.port ) {
			idStr::snPrintf( s, 64, "broadcast:%i", a.port );
		} else {
			idStr::Copynz( s, "broadcast", 64 );
		}
	} else if ( a.type == NA_BOT ) {
		idStr::Copynz( s, "bot", 64 );
	} else {
		idStr::Copynz( s, "bad", 64 );
	}
	return s;
}

/*
==================
Sys_IsLANAddress
==================
*/
bool Sys_IsLANAddress( const netadr_t adr ) {
#if ID_NOLANADDRESS
	common->Printf( "Sys_IsLANAddress: ID_NOLANADDRESS\n" );
	return false;
#endif
	if( adr.type == NA_LOOPBACK ) {
		return true;
	}

	if( adr.type != NA_IP ) {
		return false;
	}

	if( num_interfaces ) {
		int i;
		uint32_t packedIP;
		memcpy( &packedIP, adr.ip, sizeof( packedIP ) );
		const uint32_t ip = ntohl( packedIP );
                
		for( i=0; i < num_interfaces; i++ ) {
			if( ( netint[i].ip & netint[i].mask ) == ( ip & netint[i].mask ) ) {
				return true;
			}
		} 
	}	
	return false;
}

/*
===================
Sys_CompareNetAdrBase

Compares without the port
===================
*/
bool Sys_CompareNetAdrBase( const netadr_t a, const netadr_t b ) {
	if ( a.type != b.type ) {
		return false;
	}

	if ( a.type == NA_LOOPBACK ) {
		return true;
	}

	if ( a.type == NA_IP ) {
		if ( a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2] && a.ip[3] == b.ip[3] ) {
			return true;
		}
		return false;
	}

	if ( a.type == NA_IP6 ) {
		return memcmp( a.ip6, b.ip6, sizeof( a.ip6 ) ) == 0 && a.scopeId == b.scopeId;
	}

	common->Printf( "Sys_CompareNetAdrBase: bad address type\n" );
	return false;
}

//=============================================================================


#define MAX_UDP_MSG_SIZE	1400

typedef struct udpMsg_s {
	byte				data[MAX_UDP_MSG_SIZE];
	netadr_t			address;
	int					size;
	int					time;
	struct udpMsg_s *	next;
} udpMsg_t;

class idUDPLag {
public:
						idUDPLag( void );
						~idUDPLag( void );

	udpMsg_t *			sendFirst;
	udpMsg_t *			sendLast;
	udpMsg_t *			recieveFirst;
	udpMsg_t *			recieveLast;
	idBlockAlloc<udpMsg_t, 64, 0> udpMsgAllocator;
};

idUDPLag::idUDPLag( void ) {
	sendFirst = sendLast = recieveFirst = recieveLast = NULL;
}

idUDPLag::~idUDPLag( void ) {
	udpMsgAllocator.Shutdown();
}

/*
==================
idPort::idPort
==================
*/
idPort::idPort() {
	netSocket = INVALID_SOCKET;
	netSocket6 = INVALID_SOCKET;
	platformData = NULL;
	packetsRead = 0;
	bytesRead = 0;
	packetsWritten = 0;
	bytesWritten = 0;
	memset( &bound_to, 0, sizeof( bound_to ) );
}

/*
==================
idPort::~idPort
==================
*/
idPort::~idPort() {
	Close();
}

/*
==================
InitForPort
==================
*/
bool idPort::InitForPort( int portNumber ) {
	Close();
	if ( portNumber != PORT_ANY && ( portNumber < 0 || portNumber > 65535 ) ) {
		common->Warning( "idPort::InitForPort: invalid IPv4 port %d", portNumber );
		return false;
	}

	netSocket = NET_IPSocket( net_ip.GetString(), portNumber, &bound_to );
	if ( netSocket == INVALID_SOCKET ) {
		netSocket = INVALID_SOCKET;
		memset( &bound_to, 0, sizeof( bound_to ) );
		return false;
	}

#if 0
	// Intentionally unsupported; see the compatibility note above the CVars.
	if ( net_socksEnabled.GetBool() ) {
		NET_OpenSocks( portNumber );
	}
#endif

	platformData = new idUDPLag;

	return true;
}

/*
==================
idPort::Close
==================
*/
void idPort::Close() {
	if ( platformData != NULL ) {
		delete static_cast<idUDPLag *>( platformData );
		platformData = NULL;
	}
	if ( netSocket != INVALID_SOCKET ) {
		closesocket( netSocket );
		netSocket = INVALID_SOCKET;
	}
	if ( netSocket6 != INVALID_SOCKET ) {
		closesocket( netSocket6 );
		netSocket6 = INVALID_SOCKET;
	}
	memset( &bound_to, 0, sizeof( bound_to ) );
}

/*
==================
idPort::GetPacket
==================
*/
bool idPort::GetPacket( netadr_t &from, void *data, int &size, int maxSize ) {
	udpMsg_t *msg;
	bool ret;
	idUDPLag *lag = static_cast<idUDPLag *>( platformData );
	size = 0;
	memset( &from, 0, sizeof( from ) );
	from.type = NA_BAD;
	if ( netSocket == INVALID_SOCKET || lag == NULL || data == NULL || maxSize <= 0 ) {
		return false;
	}

	while( 1 ) {

		ret = Net_GetUDPPacket( netSocket, from, (char *)data, size, maxSize );
		if ( !ret ) {
			break;
		}

		if ( net_forceDrop.GetInteger() > 0 ) {
			if ( rand() < net_forceDrop.GetInteger() * RAND_MAX / 100 ) {
				continue;
			}
		}

		packetsRead++;
		bytesRead += size;

		if ( net_forceLatency.GetInteger() > 0 ) {
			if ( size > MAX_UDP_MSG_SIZE ) {
				common->DPrintf( "idPort::GetPacket: packet exceeds latency queue capacity\n" );
				continue;
			}
			msg = lag->udpMsgAllocator.Alloc();
			memcpy( msg->data, data, size );
			msg->size = size;
			msg->address = from;
			msg->time = Sys_Milliseconds();
			msg->next = NULL;
			if ( lag->recieveLast ) {
				lag->recieveLast->next = msg;
			} else {
				lag->recieveFirst = msg;
			}
			lag->recieveLast = msg;
		} else {
			break;
		}
	}

	if ( net_forceLatency.GetInteger() > 0 || lag->recieveFirst != NULL ) {

		msg = lag->recieveFirst;
		if ( msg && msg->time <= Sys_Milliseconds() - net_forceLatency.GetInteger() ) {
			if ( msg->size > maxSize ) {
				lag->recieveFirst = msg->next;
				if ( !lag->recieveFirst ) {
					lag->recieveLast = NULL;
				}
				lag->udpMsgAllocator.Free( msg );
				size = 0;
				memset( &from, 0, sizeof( from ) );
				from.type = NA_BAD;
				common->DPrintf( "idPort::GetPacket: delayed packet exceeds caller buffer capacity\n" );
				return false;
			}
			memcpy( data, msg->data, msg->size );
			size = msg->size;
			from = msg->address;
			lag->recieveFirst = lag->recieveFirst->next;
			if ( !lag->recieveFirst ) {
				lag->recieveLast = NULL;
			}
			lag->udpMsgAllocator.Free( msg );
			return true;
		}
		return false;

	} else {
		return ret;
	}
}

/*
==================
idPort::GetPacketBlocking
==================
*/
bool idPort::GetPacketBlocking( netadr_t &from, void *data, int &size, int maxSize, int timeout ) {
	size = 0;
	memset( &from, 0, sizeof( from ) );
	from.type = NA_BAD;
	if ( timeout < 0 ) {
		return GetPacket( from, data, size, maxSize );
	}

	Net_WaitForUDPPacket( netSocket, timeout );

	if ( GetPacket( from, data, size, maxSize ) ) {
		return true;
	}

	return false;
}

/*
==================
idPort::SendPacket
==================
*/
void idPort::SendPacket( const netadr_t to, const void *data, int size ) {
	udpMsg_t *msg;
	idUDPLag *lag = static_cast<idUDPLag *>( platformData );

	if ( netSocket == INVALID_SOCKET || lag == NULL ) {
		return;
	}
	if ( to.type != NA_IP && to.type != NA_LOOPBACK && to.type != NA_BROADCAST ) {
		common->Warning( "idPort::SendPacket: unsupported IPv4 address type - ignored" );
		return;
	}
	if ( size < 0 || ( data == NULL && size > 0 ) ) {
		common->Warning( "idPort::SendPacket: invalid packet buffer - ignored" );
		return;
	}

	packetsWritten++;
	bytesWritten += size;

	if ( net_forceDrop.GetInteger() > 0 ) {
		if ( rand() < net_forceDrop.GetInteger() * RAND_MAX / 100 ) {
			return;
		}
	}

	if ( net_forceLatency.GetInteger() > 0 || lag->sendFirst != NULL ) {
		if ( size > MAX_UDP_MSG_SIZE ) {
			common->Warning( "idPort::SendPacket: packet exceeds latency queue capacity - ignored" );
			return;
		}
		msg = lag->udpMsgAllocator.Alloc();
		if ( size > 0 ) {
			memcpy( msg->data, data, size );
		}
		msg->size = size;
		msg->address = to;
		msg->time = Sys_Milliseconds();
		msg->next = NULL;
		if ( lag->sendLast ) {
			lag->sendLast->next = msg;
		} else {
			lag->sendFirst = msg;
		}
		lag->sendLast = msg;

		for ( msg = lag->sendFirst; msg && msg->time <= Sys_Milliseconds() - net_forceLatency.GetInteger(); msg = lag->sendFirst ) {
			Net_SendUDPPacket( netSocket, msg->size, msg->data, msg->address );
			lag->sendFirst = lag->sendFirst->next;
			if ( !lag->sendFirst ) {
				lag->sendLast = NULL;
			}
			lag->udpMsgAllocator.Free( msg );
		}

	} else {
		Net_SendUDPPacket( netSocket, size, data, to );
	}
}


//=============================================================================

/*
==================
idTCP::idTCP
==================
*/
idTCP::idTCP() {
	fd = INVALID_SOCKET;
	memset( &address, 0, sizeof( address ) );
}

/*
==================
idTCP::~idTCP
==================
*/
idTCP::~idTCP() {
	Close();
}

/*
==================
idTCP::Init
==================
*/
bool idTCP::Init( const char *host, int port ) {
	unsigned long	_true = 1;
	struct sockaddr_in sadr;

	// Net_StringToSockaddr validates the default only when the endpoint omits
	// its own port, so an explicit host:port correctly overrides this argument.
	if ( !Net_StringToSockaddr( host, &sadr, true, port, SOCK_STREAM ) || !Net_SockadrToNetadr( &sadr, &address ) ) {
		common->Printf( "Couldn't resolve IPv4 server name \"%s\"\n", host != NULL ? host : "" );
		return false;
	}
	common->Printf( "\"%s\" resolved to %s\n", host != NULL ? host : "", Sys_NetAdrToString( address ) );

	if ( fd != INVALID_SOCKET ) {
		common->Warning( "idTCP::Init: already initialized - closing the previous connection" );
		Close();
	}
		
	if ( ( fd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP ) ) == INVALID_SOCKET ) {
		fd = INVALID_SOCKET;
		common->Printf( "ERROR: idTCP::Init: socket: %s\n", NET_ErrorString() );
		return false;
	}
	
	if ( connect( fd, reinterpret_cast<const struct sockaddr *>( &sadr ), sizeof( sadr ) ) == SOCKET_ERROR ) {
		common->Printf( "ERROR: idTCP::Init: connect: %s\n", NET_ErrorString() );
		closesocket( fd );
		fd = INVALID_SOCKET;
		return false;
	}
	
	// make it non-blocking
	if( ioctlsocket( fd, FIONBIO, &_true ) == SOCKET_ERROR ) {
		common->Printf( "ERROR: idTCP::Init: ioctl FIONBIO: %s\n", NET_ErrorString() );
		closesocket( fd );
		fd = INVALID_SOCKET;
		return false;
	}
	
	common->DPrintf( "Opened TCP connection\n" );
	return true;
}

/*
==================
idTCP::Close
==================
*/
void idTCP::Close() {
	if ( fd != INVALID_SOCKET ) {
		closesocket( fd );
	}
	fd = INVALID_SOCKET;
}

/*
==================
idTCP::Read
==================
*/
int idTCP::Read( void *data, int size ) {
	int nbytes;
	
	if ( fd == INVALID_SOCKET ) {
		common->Printf("idTCP::Read: not initialized\n");
		return -1;
	}
	if ( size <= 0 ) {
		return 0;
	}
	if ( data == NULL ) {
		common->Printf( "idTCP::Read: invalid buffer\n" );
		return -1;
	}
	
	if ( ( nbytes = recv( fd, (char *)data, size, 0 ) ) == SOCKET_ERROR ) {
		if ( WSAGetLastError() == WSAEWOULDBLOCK ) {
			return 0;
		}
		common->Printf( "ERROR: idTCP::Read: %s\n", NET_ErrorString() );
		Close();
		return -1;
	}

	// a successful read of 0 bytes indicates remote has closed the connection
	if ( nbytes == 0 ) {
		common->DPrintf( "idTCP::Read: read 0 bytes - assume connection closed\n" );
		Close();
		return -1;
	}

	return nbytes;
}

/*
==================
idTCP::Write
==================
*/
int idTCP::Write( void *data, int size ) {
	int nbytes;
	
	if ( fd == INVALID_SOCKET ) {
		common->Printf("idTCP::Write: not initialized\n");
		return -1;
	}
	if ( size <= 0 ) {
		return 0;
	}
	if ( data == NULL ) {
		common->Printf( "idTCP::Write: invalid buffer\n" );
		return -1;
	}

	if ( ( nbytes = send( fd, (char *)data, size, 0 ) ) == SOCKET_ERROR ) {
		if ( WSAGetLastError() == WSAEWOULDBLOCK ) {
			return 0;
		}
		common->Printf( "ERROR: idTCP::Write: %s\n", NET_ErrorString() );
		Close();
		return -1;
	}
	
	return nbytes;
}
