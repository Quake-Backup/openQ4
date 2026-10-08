// openQ4 additions to the SDK game sources; see src/game/LICENSE.
// Shared SP/MP implementation, included after the local Game_local.h.

idPlayerView::~idPlayerView() {
	if ( presentationFx ) uiManager->DeAlloc( presentationFx );
}

void idPlayerView::ResetRetainedViewEffects() {
	deathFxStart = -1;
	recoveryFxUntil = 0;
	recoverySequence = false;
	if ( presentationFx ) { uiManager->DeAlloc( presentationFx ); presentationFx = NULL; }
}

void idPlayerView::RetainedRecoveryFade( const idVec4 &color, int duration ) {
	if ( !player || gameLocal.isMultiplayer || !cvarSystem->GetCVarBool( "ui_retained" ) ) return;
	idStr map = gameLocal.GetMapName(); map.StripFileExtension(); map.StripPath();
	if ( map.Icmp( "mcc_2" ) ) return;
	const bool inBed = player->IsInVehicle() && player->GetVehicleController().GetVehicle() &&
		!idStr::Icmp( player->GetVehicleController().GetVehicle()->GetName(), "mcc2_playerBed" );
	if ( inBed ) recoverySequence = true;
	// Only the scripted treatment sequence qualifies. Its white fade-ins
	// remain intact; acuity takes a little longer to recover after each blink.
	if ( recoverySequence && color[0] > .95f && color[1] > .95f && color[2] > .95f && color[3] < .01f ) {
		recoveryFxUntil = gameLocal.time + Max( duration, 0 ) + 4000;
		// The last treatment fade follows the bed exit. Consume that sequence
		// here so subsequent white fades elsewhere in the map remain unchanged.
		if ( !inBed ) recoverySequence = false;
		if ( cvarSystem->GetCVarBool( "ui_retainedTrace" ) ) gameLocal.Printf( "RETAINED_RECOVERY until=%d inBed=%d\n", recoveryFxUntil, inBed );
	}
}

void idPlayerView::DrawRetainedViewEffects( bool wheelOnly ) {
	if ( !player || player->spectating || !cvarSystem->GetCVarBool( "ui_retained" ) ) {
		ResetRetainedViewEffects(); return;
	}
	float death = 0, recovery = 0, shock = 0, wheel = 0;
	if ( wheelOnly ) {
		wheel = idMath::ClampFloat( 0, 1, player->GetWeaponWheelBlend() );
		wheel = wheel * wheel * ( 3 - 2 * wheel );
	} else {
		const int now = gameLocal.time;
		const bool dead = player->health <= 0;
		if ( !dead ) deathFxStart = -1;
		else if ( deathFxStart < 0 || now < deathFxStart ) deathFxStart = now;
		death = dead ? idMath::ClampFloat( 0, 1, ( now - deathFxStart ) / 1800.0f ) : 0;
		death = death * death * ( 3 - 2 * death );
		const float elapsed = dead ? ( now - deathFxStart ) / 1000.0f : 0;
		shock = dead ? .34f * idMath::ClampFloat( 0, 1, elapsed / .12f ) * idMath::ClampFloat( 0, 1, ( 1.4f - elapsed ) / 1.28f ) : 0;
		recovery = dead ? 0 : idMath::ClampFloat( 0, 1, ( recoveryFxUntil - now ) / 6000.0f );
		recovery = recovery * recovery * ( 3 - 2 * recovery );
	}
	if ( death <= 0 && recovery <= 0 && shock <= 0 && wheel <= 0 ) return;
	if ( !presentationFx ) presentationFx = uiManager->FindGui( "guis/hud/view_effects.q4ui", true, true );
	if ( !presentationFx ) return;
	presentationFx->SetStateFloat( "death_amount", death );
	presentationFx->SetStateFloat( "death_shock", cvarSystem->GetCVarBool( "ui_retainedReducedMotion" ) ? 0 : shock );
	presentationFx->SetStateFloat( "recovery_amount", recovery );
	// Separate submissions share a document but never blur the HUD or apply
	// recovery/death twice when InfluenceVision has already drawn its GUI.
	presentationFx->SetStateFloat( "wheel_amount", wheel );
	presentationFx->StateChanged( gameLocal.time );
	presentationFx->Redraw( gameLocal.time );
}
