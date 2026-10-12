/*
 * openQ4 Awakening scene direction. See LICENSES/QUAKE-4-SDK-EULA.rtf.
 * Uses the player's installed map and assets; no recovered content is embedded.
 */

static idCVar g_awakeningM01Cinematic( "g_awakeningM01Cinematic", "1", CVAR_GAME | CVAR_BOOL,
	"Use openQ4's Awakening m01 dropship direction on a fresh map start" );

static bool Awakening_PrepareM01Cinematic( idMapFile *map ) {
	if ( gameLocal.isMultiplayer || !g_awakeningM01Cinematic.GetBool() ||
		idStr::Icmp( cvarSystem->GetCVarString( "fs_game" ), "q4xbase" ) != 0 ) {
		return false;
	}

	idStr mapName = map->GetName();
	mapName.BackSlashesToSlashes();
	mapName.StripFileExtension();
	if ( mapName.Icmp( "maps/game/m01_stranarus_trench1" ) != 0 ) {
		return false;
	}

	// Do not redirect a different revision or a mapper's replacement callbacks.
	bool cabinAnchor = false;
	bool evacuationAnchor = false;
	for ( int i = 1; i < map->GetNumEntities(); ++i ) {
		const idDict &args = map->GetEntity( i )->epairs;
		if ( idStr::Icmp( args.GetString( "classname" ), "trigger_relay" ) != 0 ) {
			continue;
		}
		if ( idStr::Icmp( args.GetString( "name" ), "intro_unit1_seq_5_trigger_seq_6" ) == 0 ) {
			cabinAnchor = idStr::Icmp( args.GetString( "call" ), "intro_unit1_seq_6" ) == 0;
		} else if ( idStr::Icmp( args.GetString( "name" ), "m04_trench_unit1_main" ) == 0 ) {
			evacuationAnchor = idStr::Icmp( args.GetString( "call" ), "m04_trench_unit1_seq_1_main" ) == 0;
		}
	}
	const char *script = "scripts/openq4/awakening/m01_dropship.script";
	if ( !cabinAnchor || !evacuationAnchor || fileSystem->ReadFile( script, NULL, NULL ) <= 0 ) {
		gameLocal.Warning( "Awakening m01 direction unavailable: keeping the map's original callbacks" );
		return false;
	}

	// Compile after worldspawn, as a map script. The ordinary script save/restore
	// file list then owns this addition; the default program and old saves keep
	// their original statement layout. Never mutate the cached map entity string.
	gameLocal.program.CompileFile( script );
	// Parse the helper while level media is being cached, before the first impact.
	gameLocal.FindEntityDef( "oq4_m01_cues", false );
	gameLocal.Printf( "Awakening m01: openQ4 dropship direction enabled\n" );
	return true;
}

static void Awakening_DirectM01Relay( idDict &args ) {
	if ( idStr::Icmp( args.GetString( "name" ), "intro_unit1_seq_5_trigger_seq_6" ) == 0 ) {
		args.Set( "call", "oq4_m01::cabin" );
	} else if ( idStr::Icmp( args.GetString( "name" ), "m04_trench_unit1_main" ) == 0 ) {
		args.Set( "call", "oq4_m01::evacuate" );
	}
}
