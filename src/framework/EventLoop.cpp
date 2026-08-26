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




idCVar idEventLoop::com_journal( "com_journal", "0", CVAR_INIT|CVAR_SYSTEM, "1 = record journal, 2 = play back journal", 0, 2, idCmdSystem::ArgCompletion_Integer<0,2> );

idEventLoop eventLoopLocal;
idEventLoop *eventLoop = &eventLoopLocal;

static bool EventLoop_IsPrivateConsoleEvent( const sysEvent_t &event ) {
	if ( event.evType != SE_CONSOLE || event.evPtr == NULL || event.evPtrLength <= 0 ||
		cvarSystem == NULL || !cvarSystem->IsInitialized() ) {
		return false;
	}
	if ( memchr( event.evPtr, '\0', static_cast<size_t>( event.evPtrLength ) ) == NULL ) {
		// A malformed console event is not safe to persist as text.
		return true;
	}
	return cvarSystem->CommandContainsPrivateCVar( static_cast<const char *>( event.evPtr ) );
}


/*
=================
idEventLoop::idEventLoop
=================
*/
idEventLoop::idEventLoop( void ) {
	com_journalFile = NULL;
	com_journalDataFile = NULL;
	initialTimeOffset = 0;
}

/*
=================
idEventLoop::~idEventLoop
=================
*/
idEventLoop::~idEventLoop( void ) {
}

/*
=================
idEventLoop::GetRealEvent
=================
*/
sysEvent_t	idEventLoop::GetRealEvent( void ) {
	int			r;
	sysEvent_t	ev;

	// either get an event from the system or the journal file
	if ( com_journal.GetInteger() == 2 ) {
		r = com_journalFile->Read( &ev, sizeof(ev) );
		if ( r != sizeof(ev) ) {
			common->FatalError( "Error reading from journal file" );
		}
		if ( ev.evPtrLength ) {
			ev.evPtr = Mem_ClearedAlloc( ev.evPtrLength );
			r = com_journalFile->Read( ev.evPtr, ev.evPtrLength );
			if ( r != ev.evPtrLength ) {
				common->FatalError( "Error reading from journal file" );
			}
		}
	} else {
		ev = Sys_GetEvent();

		// write the journal value out if needed
		if ( com_journal.GetInteger() == 1 ) {
			static const char PRIVATE_EVENT_TEXT[] = "";
			sysEvent_t journalEvent = ev;
			const void *journalData = ev.evPtr;
			if ( EventLoop_IsPrivateConsoleEvent( ev ) ) {
				journalEvent.evPtrLength = sizeof( PRIVATE_EVENT_TEXT );
				journalData = PRIVATE_EVENT_TEXT;
			}
			r = com_journalFile->Write( &journalEvent, sizeof(journalEvent) );
			if ( r != sizeof(ev) ) {
				common->FatalError( "Error writing to journal file" );
			}
			if ( journalEvent.evPtrLength ) {
				r = com_journalFile->Write( journalData, journalEvent.evPtrLength );
				if ( r != journalEvent.evPtrLength ) {
					common->FatalError( "Error writing to journal file" );
				}
			}
		}
	}

	return ev;
}

/*
=================
idEventLoop::PushEvent
=================
*/
void idEventLoop::PushEvent( sysEvent_t *event ) {
	sysEvent_t		*ev;
	static			bool printedWarning;

	ev = &com_pushedEvents[ com_pushedEventsHead & (MAX_PUSHED_EVENTS-1) ];

	if ( com_pushedEventsHead - com_pushedEventsTail >= MAX_PUSHED_EVENTS ) {

		// don't print the warning constantly, or it can give time for more...
		if ( !printedWarning ) {
			printedWarning = true;
			common->Printf( "WARNING: Com_PushEvent overflow\n" );
		}

		if ( ev->evPtr ) {
			if ( EventLoop_IsPrivateConsoleEvent( *ev ) ) {
				memset( ev->evPtr, 0, ev->evPtrLength );
			}
			Mem_Free( ev->evPtr );
		}
		com_pushedEventsTail++;
	} else {
		printedWarning = false;
	}

	*ev = *event;
	com_pushedEventsHead++;
}

/*
=================
idEventLoop::GetEvent
=================
*/
sysEvent_t idEventLoop::GetEvent( void ) {
	if ( com_pushedEventsHead > com_pushedEventsTail ) {
		com_pushedEventsTail++;
		return com_pushedEvents[ (com_pushedEventsTail-1) & (MAX_PUSHED_EVENTS-1) ];
	}
	return GetRealEvent();
}

/*
=================
idEventLoop::ProcessEvent
=================
*/
void idEventLoop::ProcessEvent( sysEvent_t ev ) {
	// track key up / down states
	if ( ev.evType == SE_KEY ) {
		idKeyInput::PreliminaryKeyEvent( ev.evValue, ( ev.evValue2 != 0 ) );
	} else if ( ev.evType == SE_MOUSE ) {
		idKeyInput::PreliminaryMouseEvent( ev.evValue, ev.evValue2 );
	} else if ( ev.evType == SE_JOYSTICK_AXIS ) {
		idKeyInput::PreliminaryJoystickEvent( ev.evValue2 );
	}

	if ( ev.evType == SE_CONSOLE ) {
		// from a text console outside the game window
		cmdSystem->BufferCommandText( CMD_EXEC_APPEND, (char *)ev.evPtr );
		cmdSystem->BufferCommandText( CMD_EXEC_APPEND, "\n" );
	} else {
		session->ProcessEvent( &ev );
	}

	// free any block data
	if ( ev.evPtr ) {
		if ( EventLoop_IsPrivateConsoleEvent( ev ) ) {
			memset( ev.evPtr, 0, ev.evPtrLength );
		}
		Mem_Free( ev.evPtr );
	}
}

/*
===============
idEventLoop::RunEventLoop
===============
*/
int idEventLoop::RunEventLoop( bool commandExecution ) {
	sysEvent_t	ev;

	while ( 1 ) {

		if ( commandExecution ) {
			// execute any bound commands before processing another event
			cmdSystem->ExecuteCommandBuffer();
		}

		ev = GetEvent();

		// if no more events are available
		if ( ev.evType == SE_NONE ) {
			return 0;
		}
		ProcessEvent( ev );
	}

	return 0;	// never reached
}

/*
=============
idEventLoop::Init
=============
*/
void idEventLoop::Init( void ) {

	initialTimeOffset = Sys_Milliseconds();

	common->StartupVariable( "journal", false );

	if ( com_journal.GetInteger() == 1 ) {
		common->Printf( "Journaling events\n" );
		com_journalFile = fileSystem->OpenFileWrite( "journal.dat" );
		com_journalDataFile = fileSystem->OpenFileWrite( "journaldata.dat" );
	} else if ( com_journal.GetInteger() == 2 ) {
		common->Printf( "Replaying journaled events\n" );
		com_journalFile = fileSystem->OpenFileRead( "journal.dat" );
		com_journalDataFile = fileSystem->OpenFileRead( "journaldata.dat" );
	}

	if ( com_journal.GetInteger() != 0 && ( !com_journalFile || !com_journalDataFile ) ) {
		com_journal.SetInteger( 0 );
		com_journalFile = 0;
		com_journalDataFile = 0;
		common->Printf( "Couldn't open journal files\n" );
	}
}

/*
=============
idEventLoop::Shutdown
=============
*/
void idEventLoop::Shutdown( void ) {
	if ( com_journalFile ) {
		fileSystem->CloseFile( com_journalFile );
		com_journalFile = NULL;
	}
	if ( com_journalDataFile ) {
		fileSystem->CloseFile( com_journalDataFile );
		com_journalDataFile = NULL;
	}
}

/*
================
idEventLoop::JournalLevel
================
*/
int idEventLoop::JournalLevel( void ) const {
	return com_journal.GetInteger();
}
