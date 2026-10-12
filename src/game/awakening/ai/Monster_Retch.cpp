#include "../../Game_local.h"
#include "../../ai/AI_Manager.h"

/*
===============================================================================

	riMonsterRetch

	monster_q4x_retch: a Strogg support unit that buffs its allies.

	When it has no buff running it looks for another AI on its team that is not
	itself a retch and not already buffed, and picks one of two buffs at random:
	a defense buff (the ally takes defenseBoostMult of the damage it would) or
	a damage buff (the ally deals damageBoostMult). The ally and the retch get
	a matching overlay, and the retch plays its "buff" animation while a beam
	(fx_buffBeam) runs from its DM_muzzle1 joint to the ally. The buff lasts
	until either of them dies.

	It also strafes (action_strafe) while it has a clear shot, and holds its
	lightning while it rushes in to melee.

	Its def asks for aas96, which m02_trianfac does not ship and
	m01_stranarus_trench2 ships empty, so the four retches there could never
	move. Its bounds fit aas48, so a retch without its own AAS walks on that.

===============================================================================
*/

class riMonsterRetch : public idAI {
public:
	CLASS_PROTOTYPE( riMonsterRetch );

							riMonsterRetch( void );
	virtual					~riMonsterRetch( void );

	void					Spawn( void );
	void					Save( idSaveGame *savefile ) const;
	void					Restore( idRestoreGame *savefile );
	virtual void			Think( void );

protected:
	virtual bool			CheckActions( void );
	virtual void			OnDeath( void );
	virtual void			OnTacticalChange( aiTactical_t oldTactical );

private:
	bool					FindBuffTarget( void );
	bool					ApplyBuff( idAI *ally );
	void					Event_RestoreBuff( void );
	void					ClearBuff( void );
	void					UpdateBuffBeam( void );
	void					StopBuffBeam( void );
	void					UseFallbackAAS( void );

	bool					CheckAction_Strafe( rvAIAction *action, int animNum );

	rvAIAction				actionStrafe;

	idEntityPtr<idAI>		buffTarget;
	bool					defenseBuff;
	rvClientEntityPtr<rvClientEffect>	buffBeam;

	stateResult_t			State_Buff( const stateParms_t &parms );

	CLASS_STATES_PROTOTYPE( riMonsterRetch );
};

const idEventDef EV_RetchRestoreBuff( "<retchRestoreBuff>", NULL );

CLASS_DECLARATION( idAI, riMonsterRetch )
	EVENT( EV_RetchRestoreBuff, riMonsterRetch::Event_RestoreBuff )
END_CLASS

CLASS_STATES_DECLARATION( riMonsterRetch )
	STATE( "Buff",	riMonsterRetch::State_Buff )
END_CLASS_STATES

static const char * const RETCH_DEFENSE_OVERLAY = "common/defense_boost_overlay";
static const char * const RETCH_DAMAGE_OVERLAY = "common/damage_boost_overlay";
static const float RETCH_BEAM_RANGE = 4096.0f;
static const char * const RETCH_FALLBACK_AAS = "aas48";

// AI_Move.cpp
bool ValidForBounds( const idAASSettings *settings, const idBounds &bounds );

/*
================
riMonsterRetch::riMonsterRetch
================
*/
riMonsterRetch::riMonsterRetch( void ) {
	defenseBuff = false;
}

riMonsterRetch::~riMonsterRetch( void ) {
	ClearBuff();
}

/*
================
riMonsterRetch::Spawn
================
*/
void riMonsterRetch::Spawn( void ) {
	actionStrafe.Init( spawnArgs, "action_strafe", NULL, 0 );
	buffTarget = NULL;
	UseFallbackAAS();
}

/*
================
riMonsterRetch::UseFallbackAAS

idAI's Spawn and Restore leave the AAS unset when the map lacks the def's.
================
*/
void riMonsterRetch::UseFallbackAAS( void ) {
	if ( aas != NULL ) {
		return;
	}
	idAAS *fallback = gameLocal.GetAAS( RETCH_FALLBACK_AAS );
	const idAASSettings *settings = fallback != NULL ? fallback->GetSettings() : NULL;
	if ( settings == NULL || !ValidForBounds( settings, physicsObj.GetBounds() ) ) {
		return;
	}
	aas = fallback;
	physicsObj.SetMaxStepHeight( settings->maxStepHeight );
	gameLocal.DPrintf( "%s: navigating with %s\n", name.c_str(), RETCH_FALLBACK_AAS );
}

/*
================
riMonsterRetch::Save
================
*/
void riMonsterRetch::Save( idSaveGame *savefile ) const {
	actionStrafe.Save( savefile );
	buffTarget.Save( savefile );
	savefile->WriteBool( defenseBuff );
}

/*
================
riMonsterRetch::Restore

Rebuild effects after the whole object graph has restored. v3 also needs its
buff ownership and scales reconstructed; v4 already persisted them on the ally.
================
*/
void riMonsterRetch::Restore( idRestoreGame *savefile ) {
	actionStrafe.Restore( savefile );
	buffTarget.Restore( savefile );
	savefile->ReadBool( defenseBuff );

	UseFallbackAAS();

	PostEventMS( &EV_RetchRestoreBuff, 0 );
}

void riMonsterRetch::Event_RestoreBuff( void ) {
	idAI *ally = buffTarget.GetEntity();
	if ( aifl.dead || ally == NULL || ally->aifl.dead || !ApplyBuff( ally ) ) {
		ClearBuff();
	}
}

void riMonsterRetch::Think( void ) {
	idAI *ally = buffTarget.GetEntity();
	if ( GetRetchBuffOwner() == this && ( ally == NULL || ally->aifl.dead ) ) {
		ClearBuff();
	}
	idAI::Think();
}

/*
================
riMonsterRetch::ApplyBuff
================
*/
bool riMonsterRetch::ApplyBuff( idAI *ally ) {
	const idMaterial *overlay = declManager->FindMaterial( defenseBuff ? RETCH_DEFENSE_OVERLAY : RETCH_DAMAGE_OVERLAY );
	const float dealt = defenseBuff ? 1.0f : spawnArgs.GetFloat( "damageBoostMult", "1" );
	const float taken = defenseBuff ? spawnArgs.GetFloat( "defenseBoostMult", "1" ) : 1.0f;
	// A v4 restore already owns its saved modifiers. Recreate its effects
	// without recalculating an active buff from possibly changed spawn args.
	if ( ally->GetRetchBuffOwner() != this && !ally->ApplyRetchBuff( this, dealt, taken, overlay ) ) {
		return false;
	}
	if ( GetRetchBuffOwner() != this ) {
		ApplyRetchBuff( this, 1.0f, 1.0f, overlay );
	}
	if ( DebugActions() ) {
		gameLocal.Printf( "%d %s: buffs %s (%s)\n", gameLocal.time, name.c_str(), ally->GetName(), defenseBuff ? "defense" : "damage" );
	}
	if ( defenseBuff ) {
		ally->PlayEffect( "fx_defenseBoost", ally->GetAnimator()->GetJointHandle( "origin" ), true );
		PlayEffect( "fx_defenseBoost", animator.GetJointHandle( "origin" ), true );
	} else {
		ally->PlayEffect( "fx_damageBoost", ally->GetAnimator()->GetJointHandle( "origin" ), true );
		PlayEffect( "fx_damageBoost", animator.GetJointHandle( "origin" ), true );
	}
	return true;
}

/*
================
riMonsterRetch::ClearBuff
================
*/
void riMonsterRetch::ClearBuff( void ) {
	idAI *ally = buffTarget.GetEntity();
	const char *effect = defenseBuff ? "fx_defenseBoost" : "fx_damageBoost";
	if ( ally != NULL && ally->GetRetchBuffOwner() == this ) {
		ally->ClearRetchBuff( this );
		ally->StopEffect( effect );
	}
	StopEffect( effect );
	ClearRetchBuff( this );
	buffTarget = NULL;
	StopBuffBeam();
}

/*
================
riMonsterRetch::FindBuffTarget
================
*/
bool riMonsterRetch::FindBuffTarget( void ) {
	for ( idActor *actor = aiManager.GetAllyTeam( (aiTeam_t)team ); actor != NULL; actor = actor->teamNode.Next() ) {
		if ( actor == this || !actor->IsType( idAI::GetClassType() ) || actor->IsType( riMonsterRetch::GetClassType() ) ) {
			continue;
		}
		idAI *ally = static_cast<idAI *>( actor );
		if ( ally->aifl.dead || ally->IsFrozenSolid() || ally->GetRetchBuffOwner() != NULL ) {
			continue;
		}

		defenseBuff = ( gameLocal.random.RandomInt( 2 ) == 0 );
		if ( !ApplyBuff( ally ) ) {
			continue;
		}
		buffTarget = ally;
		TurnToward( ally->GetPhysics()->GetOrigin() );
		return true;
	}
	return false;
}

/*
================
riMonsterRetch::UpdateBuffBeam

Runs the beam from the retch's muzzle to the middle of its ally.
================
*/
void riMonsterRetch::UpdateBuffBeam( void ) {
	idAI *ally = buffTarget.GetEntity();
	if ( ally == NULL ) {
		StopBuffBeam();
		return;
	}

	idVec3 start;
	idMat3 axis;
	GetJointWorldTransform( animator.GetJointHandle( "DM_muzzle1" ), gameLocal.time, start, axis );

	idVec3 dir = ally->GetPhysics()->GetAbsBounds().GetCenter() - start;
	dir.Normalize();
	trace_t tr;
	gameLocal.TracePoint( this, tr, start, start + dir * RETCH_BEAM_RANGE, MASK_SHOT_RENDERMODEL, this );

	rvClientEffect *beam = buffBeam.GetEntity();
	if ( beam != NULL ) {
		beam->SetOrigin( start );
		beam->SetAxis( dir.ToMat3() );
		beam->SetEndOrigin( tr.endpos );
		return;
	}
	buffBeam = PlayEffect( "fx_buffBeam", start, dir.ToMat3(), true, tr.endpos );
}

/*
================
riMonsterRetch::StopBuffBeam
================
*/
void riMonsterRetch::StopBuffBeam( void ) {
	rvClientEffect *beam = buffBeam.GetEntity();
	if ( beam != NULL ) {
		beam->Stop();
	}
	buffBeam = NULL;
}

/*
================
riMonsterRetch::CheckAction_Strafe
================
*/
bool riMonsterRetch::CheckAction_Strafe( rvAIAction *action, int animNum ) {
	if ( !enemy.fl.visible || !enemy.fl.inFov || !move.fl.done ) {
		return false;
	}
	return animNum == -1 || TestAnimMove( animNum );
}

/*
================
riMonsterRetch::CheckActions
================
*/
bool riMonsterRetch::CheckActions( void ) {
	idAI *ally = buffTarget.GetEntity();

	if ( ally == NULL && FindBuffTarget() ) {
		PerformAction( "Buff", 2, true );
		return true;
	}

	// the buff ends when the ally dies
	if ( ally != NULL && ally->aifl.dead ) {
		ClearBuff();
	}

	if ( PerformAction( &actionStrafe, (checkAction_t)&riMonsterRetch::CheckAction_Strafe ) ) {
		return true;
	}
	return idAI::CheckActions();
}

/*
================
riMonsterRetch::OnTacticalChange
================
*/
void riMonsterRetch::OnTacticalChange( aiTactical_t oldTactical ) {
	actionRangedAttack.fl.disabled = ( combat.tacticalCurrent == AITACTICAL_MELEE );
}

/*
================
riMonsterRetch::OnDeath
================
*/
void riMonsterRetch::OnDeath( void ) {
	ClearBuff();
	idAI::OnDeath();
}

/*
================
riMonsterRetch::State_Buff
================
*/
stateResult_t riMonsterRetch::State_Buff( const stateParms_t &parms ) {
	enum {
		STAGE_INIT,
		STAGE_WAIT
	};
	switch ( parms.stage ) {
		case STAGE_INIT:
			OverrideAnim( ANIMCHANNEL_LEGS );
			PlayAnim( ANIMCHANNEL_TORSO, "buff", parms.blendFrames );
			return SRESULT_STAGE( STAGE_WAIT );

		case STAGE_WAIT:
			UpdateBuffBeam();
			if ( AnimDone( ANIMCHANNEL_TORSO, parms.blendFrames ) ) {
				StopBuffBeam();
				return SRESULT_DONE;
			}
			return SRESULT_WAIT;
	}
	return SRESULT_ERROR;
}
