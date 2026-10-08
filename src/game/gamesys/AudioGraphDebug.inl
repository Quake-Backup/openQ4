// Shared by the in-tree SP/MP game modules. Quake 4 SDK EULA.
// Exercises the real radio channel and HUD without synthesizing host input.
static void Cmd_TestRadioChatter_f( const idCmdArgs &args ) {
	idPlayer *player = gameLocal.GetLocalPlayer();
	if ( player == NULL || args.Argc() < 2 ) {
		gameLocal.Printf( "testRadioChatter <sound shader>|show|stop|probe|pause|resume|distract <sound shader>\n" );
		return;
	}
	const idStr action = args.Argv( 1 );
	idSoundWorld *gameSoundWorld = soundSystem->GetSoundWorldFromId( SOUNDWORLD_GAME );
	if ( gameSoundWorld == NULL ) {
		return;
	}
	if ( action == "probe" ) {
		float peaks[40] = {};
		idSoundEmitter *emitter = gameSoundWorld->EmitterForIndex( player->GetSoundEmitter() );
		if ( emitter != NULL ) {
			emitter->AmplitudeHistory( SND_CHANNEL_RADIO, peaks, 40 );
		}
		gameLocal.Printf( "AUDIO_GRAPH" );
		for ( int i = 0; i < 40; ++i ) {
			gameLocal.Printf( " %.4f", peaks[i] );
		}
		gameLocal.Printf( "\n" );
	} else if ( action == "show" ) {
		player->StartRadioChatter();
	} else if ( action == "stop" ) {
		player->StopSound( SND_CHANNEL_RADIO, false );
		player->StopRadioChatter();
	} else if ( action == "pause" ) {
		gameSoundWorld->Pause();
	} else if ( action == "resume" ) {
		gameSoundWorld->UnPause();
	} else {
		const bool distract = action == "distract";
		if ( distract && args.Argc() != 3 ) {
			return;
		}
		const idSoundShader *shader = declManager->FindSound( args.Argv( distract ? 2 : 1 ) );
		player->StartSoundShader( shader, distract ? SND_CHANNEL_BODY : SND_CHANNEL_RADIO, SSF_GLOBAL, false, NULL );
		if ( !distract ) {
			player->StartRadioChatter();
		}
	}
}
