// Shared SP/MP wheel selection and native presentation. Quake 4 SDK EULA.
void idPlayer::InitWeaponWheel() {
	weaponWheelActive = weaponWheelGuiActive = weaponWheelCanceled = weaponWheelDebug = weaponWheelDebugHeld = false;
	weaponWheelHoveredSlot = weaponWheelGuiSlot = -1;
	weaponWheelLastMouseX = weaponWheelLastMouseY = weaponWheelLastUpdateTime = weaponWheelOpenedTime = 0;
	weaponWheelBlend = 0;
	weaponWheelCursor.Zero(); weaponWheelLastCmdAngles.Zero();
	weaponWheelGui = NULL;
}

void idPlayer::FreeWeaponWheelGui() {
	if ( weaponWheelGui ) uiManager->DeAlloc( weaponWheelGui );
	weaponWheelGui = NULL;
	weaponWheelGuiActive = false;
	weaponWheelGuiSlot = -1;
}

int idPlayer::GetWeaponWheelSlotCount() const {
	if ( !cvarSystem->GetCVarBool( "ui_retained" ) ) return WEAPON_WHEEL_SLOT_COUNT;
	// q4xbase adds core, napalm and freeze in slots 10..12. MP keeps its
	// own eleven-slot roster, irrespective of the campaign selected in SP.
	return !gameLocal.isMultiplayer && spawnArgs.GetString( "def_weapon12" )[0] ? 13 : 11;
}

// The wheels' placement in their 720 dp canvas: ui_weaponWheelOffset, kept
// inside a narrow view without changing the archived preference.
static void WeaponWheelPlacement( float &offset, float &fit ) {
	const float width = 720.0f * gameLocal.GetScreenAspectRatio();
	fit = Min( 1.0f, Max( 0.1f, ( width - 32.0f ) / 560.0f ) );
	const float reach = Max( 0.0f, width * 0.5f - 264.0f * fit - 16.0f );
	const float requested = width * idMath::ClampFloat( -40.0f, 40.0f, cvarSystem->GetCVarFloat( "ui_weaponWheelOffset" ) ) / 100.0f;
	offset = idMath::ClampFloat( -reach, reach, requested );
}

bool idPlayer::DrawRetainedWeaponWheel() {
	if ( !cvarSystem->GetCVarBool( "ui_retained" ) ) {
		FreeWeaponWheelGui();
		return false;
	}
	const char *family = isStrogg && !gameLocal.isMultiplayer ? "strogg" : "marine";
	idStr path = va( "guis/hud/weapon_wheel_%s%s.q4ui", family, GetWeaponWheelSlotCount() == 13 ? "_awakening" : "" );
	if ( weaponWheelGui && idStr::Icmp( weaponWheelGui->Name(), path ) ) FreeWeaponWheelGui();
	if ( !weaponWheelGui ) {
		if ( weaponWheelGuiSlot == -2 ) return false; // failed load: retry on next open or gate change
		weaponWheelGui = uiManager->FindGui( path, true, true );
		if ( !weaponWheelGui ) { weaponWheelGuiSlot = -2; return false; }
	}
	// Work in the document's isotropic 720 dp canvas.
	float offset, fit;
	WeaponWheelPlacement( offset, fit );
	weaponWheelGui->SetStateFloat( "offset", offset );
	weaponWheelGui->SetStateFloat( "fit", fit );
	weaponWheelGui->SetStateFloat( "blend", weaponWheelBlend );
	weaponWheelGui->SetStateInt( "selected", Max( 0, weaponWheelHoveredSlot ) );
	weaponWheelGui->SetStateInt( "previous", GetWeaponWheelSlotForWeapon( previousWeapon ) );
	weaponWheelGui->SetStateBool( "mp", gameLocal.isMultiplayer );
	weaponWheelGui->SetStateInt( "team", gameLocal.isMultiplayer && gameLocal.IsTeamGame() ? team : -1 );
	for ( int slot = 0; slot < GetWeaponWheelSlotCount(); ++slot ) {
		weaponWheelGui->SetStateString( va( "slot%d.index", slot ), va( "%d", slot + 1 ) );
		const bool owned = HasWeaponWheelSlotWeapon( slot );
		const int count = GetWeaponWheelAmmoCount( slot );
		const idDeclEntityDef *decl = GetWeaponDef( slot );
		const int ammoType = decl ? inventory.AmmoIndexForWeaponClass( spawnArgs.GetString( va( "def_weapon%d", slot ) ) ) : 0;
		const int maximum = ammoType > 0 ? inventory.MaxAmmoForAmmoClass( this, rvWeapon::GetAmmoNameForIndex( ammoType ) ) : 0;
		const int total = ammoType > 0 ? inventory.ammo[ammoType] : -1;
		const int clip = gameLocal.isMultiplayer ? 0 : Max( 0, currentWeapon == slot && weapon ? weapon->AmmoInClip() : inventory.clip[slot] );
		const int reserve = total < 0 ? -1 : Max( 0, total - clip );
		weaponWheelGui->SetStateBool( va( "slot%d.owned", slot ), owned );
		weaponWheelGui->SetStateInt( va( "slot%d.count", slot ), count );
		weaponWheelGui->SetStateString( va( "slot%d.reserve", slot ), reserve < 0 ? "" : va( "%d", reserve ) );
		weaponWheelGui->SetStateFloat( va( "slot%d.fill", slot ), maximum > 0 ? idMath::ClampFloat( 0, 1, static_cast<float>( reserve ) / maximum ) : 0 );
		if ( slot == weaponWheelHoveredSlot ) {
			weaponWheelGui->SetStateString( "weaponName", owned && decl ? common->GetLocalizedString( decl->dict.GetString( "inv_name" ) ) : "" );
			idStr readout;
			if ( owned && total >= 0 ) {
				if ( !gameLocal.isMultiplayer && decl && decl->dict.GetInt( "clipSize" ) > 0 ) readout = va( "%d / %d", clip, reserve );
				else readout = va( "%d", reserve );
			}
			weaponWheelGui->SetStateString( "ammo", readout );
		}
	}
	weaponWheelGui->StateChanged( gameLocal.time );
	if ( weaponWheelGuiActive != weaponWheelActive ) {
		weaponWheelGui->HandleNamedEvent( weaponWheelActive ? "open" : "close" );
		weaponWheelGuiActive = weaponWheelActive;
	}
	if ( weaponWheelGuiSlot != weaponWheelHoveredSlot ) {
		weaponWheelGui->HandleNamedEvent( "selection" );
		weaponWheelGuiSlot = weaponWheelHoveredSlot;
	}
	weaponWheelGui->Redraw( gameLocal.time );
	return true;
}

float idPlayer::GetWeaponWheelBlend() const {
	if ( entityNumber != gameLocal.localClientNum ) return 0;
#ifdef GAME_MPAPI
	// The quick-chat wheel softens the scene as the weapon wheel does.
	return Max( weaponWheelBlend, quickChatBlend );
#else
	return weaponWheelBlend;
#endif
}

void idPlayer::DrawWeaponWheel() {
	if ( weaponWheelBlend <= 0 || entityNumber != gameLocal.localClientNum ) return;
	if ( !DrawRetainedWeaponWheel() ) DrawLegacyWeaponWheel();
}

// Semantic developer commands exercise player selection, not host input. They
// are cheat protected by the console registry and cannot survive a map/save.
void idPlayer::DebugWeaponWheel( const idCmdArgs &args ) {
	const idStr action = args.Argv( 1 );
	if ( action == "open" ) {
		weaponWheelDebug = weaponWheelDebugHeld = true;
		weaponWheelCanceled = false;
		UpdateWeaponWheel();
	} else if ( action == "slot" && args.Argc() > 2 && weaponWheelDebug ) {
		weaponWheelHoveredSlot = idMath::ClampInt( 0, GetWeaponWheelSlotCount() - 1, atoi( args.Argv( 2 ) ) );
	} else if ( action == "release" && weaponWheelDebug ) {
		weaponWheelDebugHeld = false; UpdateWeaponWheel(); weaponWheelDebug = false;
	} else if ( action == "cancel" ) {
		ResetWeaponWheel( true );
	} else if ( action == "empty" && args.Argc() > 2 ) {
		const int slot = idMath::ClampInt( 0, GetWeaponWheelSlotCount() - 1, atoi( args.Argv( 2 ) ) );
		const int ammo = inventory.AmmoIndexForWeaponClass( spawnArgs.GetString( va( "def_weapon%d", slot ) ) );
		if ( ammo > 0 ) inventory.ammo[ammo] = 0;
		inventory.clip[slot] = 0;
	} else if ( action == "family" && !gameLocal.isMultiplayer && args.Argc() > 2 ) {
		isStrogg = !idStr::Icmp( args.Argv( 2 ), "strogg" );
	}
	gameLocal.Printf( "WEAPON_WHEEL active=%d slot=%d slots=%d current=%d ideal=%d retained=%d offset=%d timescale=%.3f gui=%s\n",
		weaponWheelActive, weaponWheelHoveredSlot, GetWeaponWheelSlotCount(), currentWeapon, idealWeapon,
		cvarSystem->GetCVarBool( "ui_retained" ), cvarSystem->GetCVarInteger( "ui_weaponWheelOffset" ), common->GetGameTimeScale(),
		weaponWheelGui ? weaponWheelGui->Name() : "none" );
}

bool idPlayer::CanUseWeaponWheel( void ) {
	return entityNumber == gameLocal.localClientNum &&
		!spectating &&
		health > 0 &&
		!pfl.dead &&
		weaponEnabled &&
		!gameLocal.inCinematic &&
		!privateCameraView &&
		gameLocal.GetCamera() == NULL &&
		!GuiActive() &&
		!objectiveSystemOpen &&
		!vehicleController.IsDriving();
}

void idPlayer::ApplyWeaponWheelInputMask( void ) {
	if ( !weaponEnabled || spectating || health <= 0 || gameLocal.inCinematic || privateCameraView || gameLocal.GetCamera() || objectiveSystemOpen || vehicleController.IsDriving() ) {
		return;
	}

	if ( ( usercmd.buttons & BUTTON_WEAPONWHEEL ) == 0 ) {
		return;
	}

	const int maskedButtons = BUTTON_ATTACK | BUTTON_ZOOM | BUTTON_SCORES;
	buttonMask |= maskedButtons;
	usercmd.buttons &= ~maskedButtons;
	if ( !gameLocal.isMultiplayer ) {
		usercmd.forwardmove = 0; usercmd.rightmove = 0; usercmd.upmove = 0;
	}
}

void idPlayer::ResetWeaponWheel( bool instantRestore ) {
	weaponWheelActive = false;
	weaponWheelCanceled = ( usercmd.buttons & BUTTON_WEAPONWHEEL ) != 0;
	weaponWheelDebug = false;
	weaponWheelHoveredSlot = GetDefaultWeaponWheelSlot();
	weaponWheelCursor.Zero();
	weaponWheelLastMouseX = usercmd.mx;
	weaponWheelLastMouseY = usercmd.my;
	weaponWheelLastCmdAngles[ PITCH ] = SHORT2ANGLE( usercmd.angles[ PITCH ] );
	weaponWheelLastCmdAngles[ YAW ] = SHORT2ANGLE( usercmd.angles[ YAW ] );
	weaponWheelLastCmdAngles[ ROLL ] = SHORT2ANGLE( usercmd.angles[ ROLL ] );
	weaponWheelLastUpdateTime = Sys_Milliseconds();

	if ( instantRestore ) {
		weaponWheelBlend = 0.0f;
		weaponWheelGuiActive = false;
		if ( weaponWheelGui ) weaponWheelGui->HandleNamedEvent( "close" );
		#ifndef GAME_MPAPI
		gameLocal.SetSpecialEffect( SPECIAL_EFFECT_BLUR, false );
		#endif
		if ( !gameLocal.isMultiplayer ) {
			common->SetGameTimeScale( 1.0f );
		}
	}
}

int idPlayer::GetWeaponWheelSlotForWeapon( int weaponIndex ) const {
	for ( int i = 0; i < GetWeaponWheelSlotCount(); ++i ) {
		if ( GetWeaponWheelWeaponForSlot( i ) == weaponIndex ) {
			return i;
		}
	}

	return -1;
}

int idPlayer::GetWeaponWheelWeaponForSlot( int slot ) const {
	if ( slot < 0 || slot >= GetWeaponWheelSlotCount() ) {
		return -1;
	}

	return cvarSystem->GetCVarBool( "ui_retained" ) ? slot : weaponWheelSlotWeapons[ slot ];
}

bool idPlayer::HasWeaponWheelSlotWeapon( int slot ) const {
	const int weaponIndex = GetWeaponWheelWeaponForSlot( slot );
	return weaponIndex >= 0 && weaponIndex < MAX_WEAPONS && ( inventory.weapons & ( 1 << weaponIndex ) ) != 0;
}

int idPlayer::GetDefaultWeaponWheelSlot( void ) const {
	const int currentSlot = GetWeaponWheelSlotForWeapon( currentWeapon );
	if ( HasWeaponWheelSlotWeapon( currentSlot ) ) {
		return currentSlot;
	}

	for ( int slot = 0; slot < GetWeaponWheelSlotCount(); ++slot ) {
		if ( HasWeaponWheelSlotWeapon( slot ) ) {
			return slot;
		}
	}

	return -1;
}

int idPlayer::GetClosestAvailableWeaponWheelSlot( float angle ) const {
	int bestSlot = -1;
	float bestDelta = idMath::INFINITY;
	const float slice = idMath::TWO_PI / GetWeaponWheelSlotCount();

	for ( int slot = 0; slot < GetWeaponWheelSlotCount(); ++slot ) {
		if ( !HasWeaponWheelSlotWeapon( slot ) ) {
			continue;
		}

		const float slotAngle = slot * slice;
		const float delta = idMath::Fabs( WeaponWheelNormalizeAngle( angle - slotAngle ) );
		if ( delta < bestDelta ) {
			bestDelta = delta;
			bestSlot = slot;
		}
	}

	if ( HasWeaponWheelSlotWeapon( weaponWheelHoveredSlot ) ) {
		const float previousDelta = idMath::Fabs( WeaponWheelNormalizeAngle( angle - weaponWheelHoveredSlot * slice ) );
		if ( previousDelta <= bestDelta + DEG2RAD( 4.0f ) ) return weaponWheelHoveredSlot;
	}
	return bestSlot;
}

int idPlayer::GetWeaponWheelAmmoCount( int weaponIndex ) {
	if ( weaponIndex < 0 || weaponIndex >= MAX_WEAPONS ) {
		return -1;
	}

	const char* weaponName = spawnArgs.GetString( va( "def_weapon%d", weaponIndex ) );
	if ( weaponName == NULL || weaponName[ 0 ] == '\0' ) {
		return -1;
	}

	int ammoRequired = 0;
	const int ammoIndex = inventory.AmmoIndexForWeaponClass( weaponName, &ammoRequired );
	if ( ammoIndex <= 0 || ammoRequired <= 0 ) {
		return -1;
	}

	return inventory.HasAmmo( ammoIndex, ammoRequired );
}

bool idPlayer::IsWeaponWheelSlotLowAmmo( int slot ) {
	const int weaponIndex = GetWeaponWheelWeaponForSlot( slot );
	if ( weaponIndex < 0 || weaponIndex >= MAX_WEAPONS ) {
		return false;
	}

	if ( ( inventory.weapons & ( 1 << weaponIndex ) ) == 0 ) {
		return false;
	}

	const idDeclEntityDef* weaponDecl = GetWeaponDef( weaponIndex );
	if ( weaponDecl == NULL ) {
		return false;
	}

	const int ammoCount = GetWeaponWheelAmmoCount( weaponIndex );
	if ( ammoCount <= 0 ) {
		return false;
	}

	int lowAmmo = weaponDecl->dict.GetInt( "lowAmmo", "0" );
	if ( lowAmmo <= 0 ) {
		lowAmmo = 2;
	}

	return ammoCount <= lowAmmo;
}

// The wheels' selector: the mouse's motion, else the look stick's turn, added
// to a cursor that stays within the ring.
static void WeaponWheelAdvanceCursor( const usercmd_t &cmd, idVec2 &cursor, int &lastMouseX, int &lastMouseY, idAngles &lastAngles ) {
	const int mouseDx = cmd.mx - lastMouseX;
	const int mouseDy = cmd.my - lastMouseY;
	idAngles currentCmdAngles;
	currentCmdAngles[ PITCH ] = SHORT2ANGLE( cmd.angles[ PITCH ] );
	currentCmdAngles[ YAW ] = SHORT2ANGLE( cmd.angles[ YAW ] );
	currentCmdAngles[ ROLL ] = SHORT2ANGLE( cmd.angles[ ROLL ] );
	const float lookDx = idMath::AngleNormalize180( currentCmdAngles[ YAW ] - lastAngles[ YAW ] );
	const float lookDy = idMath::AngleNormalize180( currentCmdAngles[ PITCH ] - lastAngles[ PITCH ] );

	lastMouseX = cmd.mx;
	lastMouseY = cmd.my;
	lastAngles = currentCmdAngles;

	cursor.x += mouseDx * WEAPON_WHEEL_MOUSE_SENSITIVITY;
	cursor.y += mouseDy * WEAPON_WHEEL_MOUSE_SENSITIVITY;
	if ( mouseDx == 0 && mouseDy == 0 ) {
		const float sensitivity = idMath::ClampFloat( 0.25f, 16.0f, cvarSystem->GetCVarFloat( "in_weaponWheelSensitivity" ) );
		// Turning right decreases yaw; wheel coordinates increase to the right.
		cursor.x -= lookDx * sensitivity;
		cursor.y += lookDy * sensitivity;
	}

	const float cursorLength = cursor.Length();
	if ( cursorLength > WEAPON_WHEEL_CURSOR_RADIUS ) {
		cursor *= ( WEAPON_WHEEL_CURSOR_RADIUS / cursorLength );
	}
}

void idPlayer::UpdateWeaponWheelCursor( void ) {
	WeaponWheelAdvanceCursor( usercmd, weaponWheelCursor, weaponWheelLastMouseX, weaponWheelLastMouseY, weaponWheelLastCmdAngles );
}

void idPlayer::UpdateWeaponWheelEffects( void ) {
#ifndef GAME_MPAPI
	const float blend = idMath::ClampFloat( 0.0f, 1.0f, weaponWheelBlend );
	if ( gameLocal.isMultiplayer ) {
		gameLocal.SetSpecialEffect( SPECIAL_EFFECT_BLUR, false );
		return;
	}

	if ( blend <= 0.0f ) {
		common->SetGameTimeScale( 1.0f );
		gameLocal.SetSpecialEffect( SPECIAL_EFFECT_BLUR, false );
		return;
	}

	common->SetGameTimeScale( WeaponWheelLerp( 1.0f, WEAPON_WHEEL_TIMESCALE_SCALE, blend ) );

	if ( cvarSystem->GetCVarBool( "ui_retained" ) ) {
		gameLocal.SetSpecialEffect( SPECIAL_EFFECT_BLUR, false );
		return; // the native pre-HUD pass supplies Gaussian scene soft focus
	}
	gameLocal.SetSpecialEffect( SPECIAL_EFFECT_BLUR, true );
	gameLocal.SetSpecialEffectParm( SPECIAL_EFFECT_BLUR, 0, WeaponWheelLerp( 0.694f, 0.820f, blend ) );
	gameLocal.SetSpecialEffectParm( SPECIAL_EFFECT_BLUR, 1, WeaponWheelLerp( 0.694f, 0.860f, blend ) );
	gameLocal.SetSpecialEffectParm( SPECIAL_EFFECT_BLUR, 2, WeaponWheelLerp( 0.694f, 0.930f, blend ) );
	gameLocal.SetSpecialEffectParm( SPECIAL_EFFECT_BLUR, 3, 0.0f );
	gameLocal.SetSpecialEffectParm( SPECIAL_EFFECT_BLUR, 4, WeaponWheelLerp( 96.0f, 64.0f, blend ) );
	gameLocal.SetSpecialEffectParm( SPECIAL_EFFECT_BLUR, 5, WeaponWheelLerp( 24.0f, 16.0f, blend ) );
	gameLocal.SetSpecialEffectParm( SPECIAL_EFFECT_BLUR, 6, blend );
	gameLocal.SetSpecialEffectParm( SPECIAL_EFFECT_BLUR, 7, Max( cvarSystem->GetCVarFloat( "r_znear" ), 0.25f ) );
#endif
}

void idPlayer::UpdateWeaponWheel( void ) {
	if ( entityNumber != gameLocal.localClientNum ) return;
	const bool canUseWheel = CanUseWeaponWheel();
	const bool requested = weaponWheelDebug ? weaponWheelDebugHeld : ( usercmd.buttons & BUTTON_WEAPONWHEEL ) != 0;
	if ( !requested ) weaponWheelCanceled = false;
	const bool held = canUseWheel && requested && !weaponWheelCanceled;
	const bool wasBlended = weaponWheelBlend > 0.0f;
	const int now = Sys_Milliseconds();
	const bool reducedMotion = cvarSystem->GetCVarBool( "ui_retained" ) && cvarSystem->GetCVarBool( "ui_retainedReducedMotion" );
	const float blendInSpeed = reducedMotion ? 12.5f : WEAPON_WHEEL_BLEND_IN_SPEED;
	const float blendOutSpeed = reducedMotion ? 12.5f : WEAPON_WHEEL_BLEND_OUT_SPEED;

	if ( weaponWheelLastUpdateTime == 0 ) {
		weaponWheelLastUpdateTime = now;
	}

	float dt = MS2SEC( idMath::ClampInt( 0, 100, now - weaponWheelLastUpdateTime ) );
	weaponWheelLastUpdateTime = now;

	if ( !canUseWheel ) {
		weaponWheelActive = false;
		weaponWheelBlend = Max( 0.0f, weaponWheelBlend - dt * blendOutSpeed );
		if ( weaponWheelBlend > 0.0f || wasBlended ) {
			UpdateWeaponWheelEffects();
		}
		if ( weaponWheelBlend <= 0.0f && wasBlended ) {
			ResetWeaponWheel( true );
		}
		return;
	}

	if ( held && !weaponWheelActive ) {
		weaponWheelActive = true;
		weaponWheelOpenedTime = now;
		if ( !weaponWheelGui ) weaponWheelGuiSlot = -1;
		weaponWheelCursor.Zero();
		weaponWheelLastMouseX = usercmd.mx;
		weaponWheelLastMouseY = usercmd.my;
		weaponWheelHoveredSlot = GetDefaultWeaponWheelSlot();
		for ( int axis = 0; axis < 3; ++axis ) {
			weaponWheelLastCmdAngles[ axis ] = SHORT2ANGLE( usercmd.angles[ axis ] );
		}
	}

	if ( weaponWheelActive ) {
		if ( !weaponWheelDebug ) UpdateWeaponWheelCursor();

		if ( weaponWheelCursor.Length() > WEAPON_WHEEL_DEADZONE_RADIUS ) {
			float angle = atan2f( weaponWheelCursor.x, -weaponWheelCursor.y );
			if ( angle < 0.0f ) {
				angle += idMath::TWO_PI;
			}

			weaponWheelHoveredSlot = GetClosestAvailableWeaponWheelSlot( angle );
		} else if ( weaponWheelHoveredSlot < 0 ) {
			weaponWheelHoveredSlot = GetDefaultWeaponWheelSlot();
		}
	} else {
		weaponWheelLastMouseX = usercmd.mx;
		weaponWheelLastMouseY = usercmd.my;
	}

	if ( !held && weaponWheelActive ) {
		const int selectedWeapon = ( now - weaponWheelOpenedTime < 180 && weaponWheelCursor.Length() <= WEAPON_WHEEL_DEADZONE_RADIUS && !weaponWheelDebug )
			? previousWeapon : GetWeaponWheelWeaponForSlot( weaponWheelHoveredSlot );
		weaponWheelActive = false;
		weaponWheelCursor.Zero();
		weaponWheelLastMouseX = usercmd.mx;
		weaponWheelLastMouseY = usercmd.my;

		if ( selectedWeapon >= 0 && selectedWeapon < MAX_WEAPONS && ( inventory.weapons & ( 1 << selectedWeapon ) ) != 0 ) {
			// Use the existing reliable impulse route in MP, including prediction and ACK.
			if ( selectedWeapon != currentWeapon && GetWeaponWheelAmmoCount( selectedWeapon ) != 0 ) PerformImpulse( selectedWeapon );
		}
	}

	const float targetBlend = held ? 1.0f : 0.0f;
	if ( weaponWheelBlend < targetBlend ) {
		weaponWheelBlend = Min( 1.0f, weaponWheelBlend + dt * blendInSpeed );
	} else if ( weaponWheelBlend > targetBlend ) {
		weaponWheelBlend = Max( 0.0f, weaponWheelBlend - dt * blendOutSpeed );
	}

	if ( weaponWheelBlend > 0.0f || wasBlended ) {
		UpdateWeaponWheelEffects();
	}
}

