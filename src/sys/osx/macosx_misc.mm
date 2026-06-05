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

#include "../../idlib/precompiled.h"
#define GL_GLEXT_LEGACY // AppKit.h include pulls in gl.h already
#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <crt_externs.h>
#import <errno.h>
#import <ctype.h>
#import <spawn.h>
#import <string.h>
#import <sys/stat.h>
#import <unistd.h>
#include "../sys_local.h"

static const int MAX_OSX_PROCESS_ARGS = 32;
static const int MAX_OSX_PROCESS_COMMAND = 4096;

static bool OSX_StringHasControlCharacters( const char *text ) {
	if ( text == NULL ) {
		return true;
	}

	for ( const unsigned char *scan = reinterpret_cast<const unsigned char *>( text ); *scan != '\0'; ++scan ) {
		if ( iscntrl( *scan ) ) {
			return true;
		}
	}
	return false;
}

static bool OSX_IsRegularFile( const char *path ) {
	struct stat st;
	return path != NULL && path[0] != '\0' && stat( path, &st ) == 0 && S_ISREG( st.st_mode );
}

static void OSX_EnsureOwnerExecutable( const char *path ) {
	struct stat st;
	if ( path == NULL || path[0] == '\0' || stat( path, &st ) == -1 || !S_ISREG( st.st_mode ) ) {
		return;
	}
	if ( ( st.st_mode & S_IXUSR ) == 0 && chmod( path, st.st_mode | S_IXUSR ) == -1 ) {
		common->Printf( "chmod +x %s failed: %s\n", path, strerror( errno ) );
	}
}

static bool OSX_ParseProcessCommandLine( const char *command, char *buffer, size_t bufferSize, char **argv, int maxArgv ) {
	if ( command == NULL || command[0] == '\0' || buffer == NULL || bufferSize == 0 || argv == NULL || maxArgv < 2 ) {
		return false;
	}
	if ( strlen( command ) >= bufferSize || OSX_StringHasControlCharacters( command ) ) {
		return false;
	}

	const char *read = command;
	char *write = buffer;
	char *end = buffer + bufferSize - 1;
	int argc = 0;

	while ( *read != '\0' ) {
		while ( *read != '\0' && isspace( static_cast<unsigned char>( *read ) ) ) {
			++read;
		}
		if ( *read == '\0' ) {
			break;
		}
		if ( argc >= maxArgv - 1 ) {
			return false;
		}

		argv[argc++] = write;
		char quote = '\0';
		while ( *read != '\0' ) {
			const char ch = *read;
			if ( quote == '\0' && isspace( static_cast<unsigned char>( ch ) ) ) {
				break;
			}
			if ( ch == '"' || ch == '\'' ) {
				if ( quote == '\0' ) {
					quote = ch;
					++read;
					continue;
				}
				if ( quote == ch ) {
					quote = '\0';
					++read;
					continue;
				}
			}
			if ( ch == '\\' && read[1] != '\0' ) {
				++read;
			}
			if ( write >= end ) {
				return false;
			}
			*write++ = *read++;
		}

		if ( quote != '\0' || write >= end ) {
			return false;
		}
		*write++ = '\0';
	}

	argv[argc] = NULL;
	return argc > 0 && argv[0][0] != '\0';
}

static bool OSX_IsSafeURL( const char *url ) {
	if ( url == NULL || url[0] == '\0' || OSX_StringHasControlCharacters( url ) ) {
		return false;
	}
	if ( !isalpha( static_cast<unsigned char>( url[0] ) ) ) {
		return false;
	}
	for ( const char *scan = url + 1; *scan != '\0'; ++scan ) {
		if ( *scan == ':' ) {
			return true;
		}
		if ( !( isalnum( static_cast<unsigned char>( *scan ) ) || *scan == '+' || *scan == '-' || *scan == '.' ) ) {
			return false;
		}
	}
	return false;
}

static bool OSX_StartProcessArgs( char *const argv[], bool dofork ) {
	if ( argv == NULL || argv[0] == NULL || argv[0][0] == '\0' ) {
		return false;
	}

	OSX_EnsureOwnerExecutable( argv[0] );

	if ( !dofork ) {
		common->Printf( "exec %s\n", argv[0] );
		execvp( argv[0], argv );
		common->Printf( "exec %s failed: %s\n", argv[0], strerror( errno ) );
		_exit( 127 );
	}

	pid_t childPid = 0;
	const int result = posix_spawnp( &childPid, argv[0], NULL, NULL, argv, *_NSGetEnviron() );
	if ( result != 0 ) {
		common->Printf( "posix_spawnp %s failed: %s\n", argv[0], strerror( result ) );
		return false;
	}
	(void)childPid;

	return true;
}

/*
==================
idSysLocal::OpenURL
==================
*/
void idSysLocal::OpenURL( const char *url, bool doexit ) {
	static bool	quit_spamguard = false;

	if ( quit_spamguard ) {
		common->DPrintf( "Sys_OpenURL: already in a doexit sequence, ignoring %s\n", url ? url : "" );
		return;
	}

	common->Printf( "Open URL: %s\n", url ? url : "" );
	if ( !OSX_IsSafeURL( url ) ) {
		common->Printf( "OpenURL '%s' rejected: expected a URL with a safe scheme\n", url ? url : "" );
		return;
	}

	NSString *urlString = [ NSString stringWithUTF8String: url ];
	if ( urlString == nil ) {
		common->Printf( "OpenURL '%s' rejected: URL is not valid UTF-8\n", url );
		return;
	}

	NSURL *nsURL = [ NSURL URLWithString: urlString ];
	if ( nsURL == nil || [ nsURL scheme ] == nil ) {
		common->Printf( "OpenURL '%s' rejected: Foundation could not parse URL\n", url );
		return;
	}

	if ( ![[ NSWorkspace sharedWorkspace ] openURL: nsURL ] ) {
		common->Printf( "OpenURL '%s' failed\n", url );
		return;
	}

	if ( doexit ) {
		quit_spamguard = true;
		cmdSystem->BufferCommandText( CMD_EXEC_APPEND, "quit\n" );
	}
}

/*
==================
Sys_DoStartProcess
==================
*/
void Sys_DoStartProcess( const char *exeName, bool dofork ) {
	if ( exeName == NULL || exeName[0] == '\0' ) {
		common->Printf( "Sys_DoStartProcess: empty command\n" );
		return;
	}

	if ( OSX_IsRegularFile( exeName ) && !OSX_StringHasControlCharacters( exeName ) ) {
		char *argv[2];
		argv[0] = const_cast<char *>( exeName );
		argv[1] = NULL;
		(void)OSX_StartProcessArgs( argv, dofork );
		return;
	}

	char commandBuffer[MAX_OSX_PROCESS_COMMAND];
	char *argv[MAX_OSX_PROCESS_ARGS];
	if ( !OSX_ParseProcessCommandLine( exeName, commandBuffer, sizeof( commandBuffer ), argv, MAX_OSX_PROCESS_ARGS ) ) {
		common->Printf( "Sys_DoStartProcess: invalid command line '%s'\n", exeName );
		return;
	}

	(void)OSX_StartProcessArgs( argv, dofork );
}

/*
==================
OSX_GetLocalizedString
==================
*/
const char* OSX_GetLocalizedString( const char* key )
{
	if ( key == NULL ) {
		return "";
	}

	NSString *lookupKey = [ NSString stringWithUTF8String: key ];
	if ( lookupKey == nil ) {
		return key;
	}

	NSString *string = [ [ NSBundle mainBundle ] localizedStringForKey:lookupKey
													 value:@"No translation" table:nil];
	return [string UTF8String];
}
