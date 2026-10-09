//----------------------------------------------------------------
// NavMesh.cpp
//
// Runtime navigation mesh generation and routing.  See NavMesh.h for why this
// exists instead of AAS.
//----------------------------------------------------------------

#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "../Game_local.h"
#include "NavMesh.h"

rvNavMesh					navMesh;

/*
===============================================================================

	Generation constants.

	Everything that describes the agent is derived from the movement cvars the
	server is actually running, so a mod that widens the player also widens the
	navmesh without touching this file.

===============================================================================
*/

// Only CONTENTS_SOLID and CONTENTS_PLAYERCLIP: MASK_PLAYERSOLID also carries
// CONTENTS_BODY, and generation must not see players standing in doorways.
#define NAV_CLIP_MASK			( CONTENTS_SOLID | CONTENTS_PLAYERCLIP )

static const float	NAV_WALKABLE_NORMAL_Z	= 0.7f;		// ~45 degrees, matches idPhysics_Player
// Furthest one-way ledge drop we will route over.  Multiplayer falls are never
// fatal - CrashLand's fatal threshold is off and a 375-unit fall costs 10 HP -
// and stock maps hide power items behind 224-576 unit drops (the Red Armor in
// q4ctf6's sunken centre, the Quad in q4xdm13's basin), so 192 left those
// unreachable and turned ordinary drops into detours up to fifteen times
// longer.  Kill pits below this are refused by their own hazard volumes.
static const float	NAV_MAX_DROP			= 512.0f;
// Falls this far deal the soft and hard landing damage in multiplayer.
static const float	NAV_SOFT_FALL			= 250.0f;
static const float	NAV_HARD_FALL			= 375.0f;
static const float	NAV_FALL_DAMAGE_PENALTY	= 64.0f;
static const float	NAV_MERGE_HEIGHT		= 28.0f;	// two floors this close in a cell are one node
static const float	NAV_GROUND_OFFSET		= 1.0f;		// lift traces off the surface they start on
static const int	NAV_MAX_NODES			= 65536;
static const int	NAV_HASH_BUCKETS		= 16384;	// cell hash size; idHashIndex never rehashes
static const int	NAV_MAX_WALKABLE_PROBES	= 48;		// collision probes one snap query may spend
static const int	NAV_SMOOTH_LOOKAHEAD	= 8;		// nodes a single string-pull step may skip
static const float	NAV_SMOOTH_MAX_DIP		= 48.0f;	// how far a shortcut may float above the ground it crosses
static const int	NAV_MAX_ROUTE_REPAIRS	= 16;		// blocked graph edges excluded before giving up

// Extra cost so a route prefers walking around to falling or jumping, when
// more than one works.
static const float	NAV_DROP_PENALTY		= 96.0f;
static const float	NAV_JUMP_PENALTY		= 64.0f;
// Volume links are near instant to traverse but should not be spammed.
static const float	NAV_JUMPPAD_COST		= 128.0f;
static const float	NAV_TELEPORT_COST		= 64.0f;

// "No route" as a finite number, so landmark arithmetic stays defined.
static const float	NAV_INFINITE_COST		= 1.0e30f;
static const float	NAV_UNREACHED			= 1.0e29f;	// anything at or above this is infinite
// Stepping onto ground inside a survivable pain volume.  Large enough that a
// route detours around a pain floor whenever a sane detour exists.
static const float	NAV_HAZARD_PENALTY		= 384.0f;
// One touch of a trigger_hurt this strong kills a fresh spawn outright, and a
// volume burning this fast kills anyone who lingers - q4xdm14's lava pools are
// 40 HP a second of trigger_hurt over clip, with no lava contents at all.
static const float	NAV_LETHAL_HURT_DAMAGE	= 100.0f;
static const float	NAV_LETHAL_HURT_RATE	= 30.0f;
// A mover this close above a lethal volume is a trapdoor over it.
static const float	NAV_TRAPDOOR_DEPTH		= 192.0f;

// Running jumps across gaps.  The ballistic proof assumes a fraction of full
// run speed and a takeoff a little before the edge node, so a jump the graph
// offers is one a bot actually makes.
static const float	NAV_GAP_SPEED_FRACTION	= 0.85f;
static const float	NAV_GAP_TAKEOFF_SLACK	= 16.0f;
static const float	NAV_GAP_MAX_FALL		= 128.0f;	// landing at most this far below the takeoff
static const float	NAV_GAP_PENALTY			= 128.0f;	// a gap jump can be missed; prefer walking round
static const float	NAV_GAP_RUNUP			= 56.0f;	// straight approach a bot takes before a gap jump
static const float	NAV_TRANSPORT_GOAL_REACH = 64.0f;	// a goal touching a pad ends its route this close
static const float	NAV_SWEEP_SLACK			= 2.0f;		// a sweep stopped this short of its end arrived
static const int	NAV_MAX_LANDMARKS		= 8;
static const int	NAV_LANDMARK_STRIDE		= 2 * NAV_MAX_LANDMARKS;	// floats per node row: from, to per landmark
// The condensation closure is components^2 bits; past this it is skipped and
// cross-component queries fall back to searching.
static const int	NAV_MAX_CLOSURE_COMPONENTS = 4096;

static const int	navNeighbourX[8] = { 1,  1,  0, -1, -1, -1,  0,  1 };
static const int	navNeighbourY[8] = { 0,  1,  1,  1,  0, -1, -1, -1 };

// Loaded once per build and reused for every probe; rebuilding a trace model
// per trace dominates generation time otherwise.
static idClipModel *		navAgentModel = NULL;
static idTraceModel			navAgentTrace;
static idBounds				navAgentBounds;

/*
================
NavTravelVolumeBounds

Jump pads move their brush clip out of the entity physics object and into the
force-field evaluator during Spawn.  Teleporter triggers retain ordinary
physics bounds.  This keeps both generation and execution aimed at the volume
that actually activates.
================
*/
static bool NavTravelVolumeBounds( idEntity *ent, idBounds &bounds ) {
	if ( !ent ) {
		return false;
	}

	if ( ent->IsType( rvJumpPad::GetClassType() ) ) {
		return static_cast<rvJumpPad *>( ent )->GetForceFieldBounds( bounds );
	}

	if ( !ent->GetPhysics() ) {
		return false;
	}

	bounds = ent->GetPhysics()->GetAbsBounds();
	return true;
}

/*
================
NavDoorIsWalkActivated

Only an ordinary unlocked proximity door is equivalent to open floor.  Script,
key, shoot, and locked doors need decisions the movement layer cannot make and
must remain solid until their game logic opens them.
================
*/
static bool NavDoorIsWalkActivated( idEntity *ent ) {
	if ( !ent || !ent->IsType( idDoor::GetClassType() ) ) {
		return false;
	}

	idDoor *door = static_cast<idDoor *>( ent );
	return !door->IsLocked() && !door->IsNoTouch() &&
		   !door->spawnArgs.GetString( "requires", "" )[0];
}

// openQ4 BEGIN
/*
================
NavPointInLethalLiquid

Lava and slime are not solid, so every generation trace passes straight through them and the graph
would happily lay a walkable node on the bottom of a lava pool - then route bots across it. Refusing
those cells keeps routing out of the hazard entirely, which is what Quake 3's AAS achieves by
classifying liquid areas up front.

Water is deliberately not excluded. It is crossable, so it stays in the graph and swimming is
handled at run time by rvBot::UpdateLiquidMovement.
================
*/
static bool NavPointInLethalLiquid( const idVec3 &point ) {
	const idVec3 probe = point + idVec3( 0.0f, 0.0f, NAV_GROUND_OFFSET );
	return ( gameLocal.LiquidContentsAtPoint( probe, NULL ) & ( CONTENTS_LAVA | CONTENTS_SLIME ) ) != 0;
}
// openQ4 END

/*
================
NavHurtDamagePerTouch

What one touch of a trigger_hurt deals, from the live damage declaration so a
mod's retune is honoured.  A gib declaration kills regardless of its number.
================
*/
static float NavHurtDamagePerTouch( const idEntity *ent, bool &gibs ) {
	gibs = false;

	const char *damageName = ent->spawnArgs.GetString( "def_damage", "damage_painTrigger" );
	const idDict *damage = damageName[0] ? gameLocal.FindEntityDefDict( damageName, false ) : NULL;
	if ( !damage ) {
		return 0.0f;
	}

	gibs = damage->GetBool( "gib", "0" );
	return Max( 0.0f, damage->GetFloat( "damage", "0" ) );
}

/*
================
NavAgentTranslation

The agent sweep behind every navmesh probe: the world and every solid entity
except bodies.  Multiplayer players carry CONTENTS_SOLID for their combat box,
so the stock trace collided with them - with the bot asking for a route most
of all, whose own box overlaps every sweep it starts - and route snaps were
refused on the very floor the bot stood on: a third of all failed searches on
a busy q4dm5.  Players and corpses move; they are never architecture.

A navmesh built while a normal multiplayer door is closed must still contain
the floor on its far side.  Players activate those doors simply by approaching
them, so they are traversable rather than permanent walls.  Locked, keyed,
no-touch and scripted doors remain obstructions, as does every other solid
entity: movers are reconsidered by route searches at run time.
================
*/
static bool NavAgentTranslation( trace_t &trace, const idVec3 &start, const idVec3 &end ) {
	static idClipModel *touching[MAX_GENTITIES];

	// The navmesh is built over the main instance's clip world.
	const idClip *clipWorld = gameLocal.GetEntityClipWorld( NULL );
	collisionModelManager->Translation( &trace, start, end, &navAgentTrace, mat3_identity, NAV_CLIP_MASK,
										clipWorld->GetWorldCollisionModel(), vec3_origin, mat3_identity );
	trace.c.entityNum = ( trace.fraction < 1.0f ) ? ENTITYNUM_WORLD : ENTITYNUM_NONE;
	if ( trace.fraction <= 0.0f ) {
		return false;
	}

	idBounds swept;
	swept.FromBoundsTranslation( navAgentBounds, start, mat3_identity, trace.endpos - start );
	const int num = clipWorld->ClipModelsTouchingBounds( swept, NAV_CLIP_MASK, touching, MAX_GENTITIES );

	for ( int i = 0; i < num; i++ ) {
		const idClipModel *touch = touching[i];
		idEntity *ent = touch ? touch->GetEntity() : NULL;
		if ( !ent || !touch->GetCollisionModel() ||
			 ( touch->GetContents() & ( CONTENTS_BODY | CONTENTS_CORPSE ) ) ||
			 ent->IsType( idActor::GetClassType() ) || NavDoorIsWalkActivated( ent ) ) {
			continue;
		}

		trace_t hit;
		collisionModelManager->Translation( &hit, start, end, &navAgentTrace, mat3_identity, NAV_CLIP_MASK,
											touch->GetCollisionModel(), touch->GetOrigin(), touch->GetAxis() );
		if ( hit.fraction < trace.fraction ) {
			trace = hit;
			trace.c.entityNum = ent->entityNumber;
			if ( trace.fraction <= 0.0f ) {
				break;
			}
		}
	}

	return trace.fraction >= 1.0f;
}

/*
================
NavIsDynamicSolid

Solids that can be somewhere else at run time than at build time.  Stock maps
are mostly func_static brush entities, which never move and which generation
already proved; only these can make a proven link stale.
================
*/
static bool NavIsDynamicSolid( const idEntity *ent ) {
	return ent && ( ent->IsType( idMover::GetClassType() ) ||
					ent->IsType( idMover_Binary::GetClassType() ) ||
					ent->IsType( idMover_Periodic::GetClassType() ) ||
					ent->IsType( idMoveable::GetClassType() ) );
}

/*
================
rvNavMesh::GetAgentBounds

The player's own standing bounding box, origin at the feet.
================
*/
const idBounds &rvNavMesh::GetAgentBounds( void ) {
	const float halfWidth = pm_bboxwidth.GetFloat() * 0.5f;

	navAgentBounds[0].Set( -halfWidth, -halfWidth, 0.0f );
	navAgentBounds[1].Set( halfWidth, halfWidth, pm_normalheight.GetFloat() );

	return navAgentBounds;
}

/*
===============================================================================

	rvNavPath

===============================================================================
*/

/*
================
rvNavPath::Append
================
*/
void rvNavPath::Append( const idVec3 &origin, int travelType, int actionEntityNum ) {
	navCorner_t corner;

	corner.origin			= origin;
	corner.travelType		= (short)travelType;
	corner.actionEntityNum	= (short)actionEntityNum;

	corners.Append( corner );
}

/*
================
rvNavPath::Length
================
*/
float rvNavPath::Length( void ) const {
	float length = 0.0f;

	for ( int i = 1; i < corners.Num(); i++ ) {
		length += ( corners[i].origin - corners[i - 1].origin ).Length();
	}

	return length;
}

/*
================
rvNavPath::LengthFrom

Length() describes only the geometry between stored corners.  A route created
by FindPath also retains its graph cost, including the exact first/last legs,
jump/drop penalties, and cheap authored transports.  The geometric fallback
keeps manually assembled paths useful from another origin.
================
*/
float rvNavPath::LengthFrom( const idVec3 &origin ) const {
	if ( corners.Num() == 0 ) {
		return 0.0f;
	}
	if ( hasTravelCost && ( origin - travelStart ).LengthSqr() <= 1.0f ) {
		return travelCost;
	}

	return ( corners[0].origin - origin ).Length() + Length();
}

/*
================
rvNavPath::SetTravelCost

String pulling removes intermediate nodes, but their jump/drop penalties and
cheap transport edges still matter when comparing goals.  Keep the exact graph
cost alongside the steering corners without changing the movement interface.
================
*/
void rvNavPath::SetTravelCost( const idVec3 &origin, float cost ) {
	travelStart = origin;
	travelCost = Max( 0.0f, cost );
	hasTravelCost = true;
}

/*
===============================================================================

	rvNavMesh construction

===============================================================================
*/

/*
================
rvNavMesh::rvNavMesh
================
*/
// The cell hash is sized for the mesh rather than left on idHashIndex's default
// 1024 buckets.  idHashIndex never rehashes - Add only ever grows the index
// chain - so a default-constructed table stays 64x oversubscribed at
// NAV_MAX_NODES, and every FindNode during generation walks a chain that long.
// The buckets are not allocated until the first Add, so a small map pays
// nothing for this.
rvNavMesh::rvNavMesh( void ) : nodeHash( NAV_HASH_BUCKETS, NAV_HASH_BUCKETS ) {
	gridOrigin.Zero();
	cellSize		= 24.0f;
	numAreas		= 0;
	lethalLiquidSeen = false;
	transportNodes	= 0;
	numComponents	= 0;
	componentWords	= 0;
	numLandmarks	= 0;
	buildMsec		= 0;
	gapJumpLinks	= 0;
	heuristicScale = 1.0f;
	searchSerial	= 0;
	fieldSerial		= 0;
	fieldPriced		= false;
	nextSnapReportTime = 0;
	memset( &stats, 0, sizeof( stats ) );

	nodes.SetGranularity( 1024 );
	links.SetGranularity( 4096 );
}

/*
================
rvNavMesh::~rvNavMesh
================
*/
rvNavMesh::~rvNavMesh( void ) {
	Clear();
}

/*
================
rvNavMesh::Clear
================
*/
void rvNavMesh::Clear( void ) {
	nodes.Clear();
	links.Clear();
	nodeHash.Clear();
	hazards.Clear();
	transports.Clear();

	componentReach.Clear();
	landmarkNodes.Clear();
	landmarkCost.Clear();
	reverseOffset.Clear();
	reverseSource.Clear();
	reverseCost.Clear();
	entitySnaps.Clear();
	dynamicSolids.Clear();
	dynamicSolidSpawnIds.Clear();
	fieldTargetTag.Clear();
	fieldPricedTag.Clear();
	fieldPricedCost.Clear();

	gScore.Clear();
	fScore.Clear();
	cameFrom.Clear();
	openTag.Clear();
	closedTag.Clear();
	heap.Clear();

	numAreas		= 0;
	lethalLiquidSeen = false;
	transportNodes	= 0;
	numComponents	= 0;
	componentWords	= 0;
	numLandmarks	= 0;
	buildMsec		= 0;
	gapJumpLinks	= 0;
	heuristicScale = 1.0f;
	searchSerial	= 0;
	fieldSerial		= 0;
	fieldPriced		= false;
	nextSnapReportTime = 0;

	if ( navAgentModel ) {
		delete navAgentModel;
		navAgentModel = NULL;
	}
}

/*
================
rvNavMesh::CellCoords
================
*/
void rvNavMesh::CellCoords( const idVec3 &origin, int &gx, int &gy ) const {
	gx = (int)idMath::Floor( ( origin.x - gridOrigin.x ) / cellSize );
	gy = (int)idMath::Floor( ( origin.y - gridOrigin.y ) / cellSize );
}

/*
================
rvNavMesh::CellCentre
================
*/
idVec3 rvNavMesh::CellCentre( int gx, int gy, float z ) const {
	return idVec3( gridOrigin.x + ( gx + 0.5f ) * cellSize,
				   gridOrigin.y + ( gy + 0.5f ) * cellSize,
				   z );
}

/*
================
rvNavMesh::FindNode

Nodes are hashed by grid cell.  A cell can hold several floors - a catwalk over
a corridor - so the z has to be matched as well.
================
*/
int rvNavMesh::FindNode( int gx, int gy, float z ) const {
	const int key = nodeHash.GenerateKey( gx, gy );

	for ( int i = nodeHash.First( key ); i != -1; i = nodeHash.Next( i ) ) {
		if ( nodes[i].gx != gx || nodes[i].gy != gy ) {
			continue;
		}
		if ( idMath::Fabs( nodes[i].origin.z - z ) <= NAV_MERGE_HEIGHT ) {
			return i;
		}
	}

	return -1;
}

/*
================
rvNavMesh::AddNode
================
*/
int rvNavMesh::AddNode( int gx, int gy, const idVec3 &origin ) {
	navNode_t node;

	node.origin		= origin;
	node.gx			= gx;
	node.gy			= gy;
	node.firstLink	= -1;
	node.area		= -1;
	node.component	= -1;
	node.flags		= ( HazardAtFloor( origin ) >= 0 ) ? NAVNODE_HAZARD : 0;

	const int index = nodes.Append( node );
	nodeHash.Add( nodeHash.GenerateKey( gx, gy ), index );

	return index;
}

/*
================
rvNavMesh::LinkNodes
================
*/
void rvNavMesh::LinkNodes( int from, int to, float cost, int travelType, int actionEntityNum ) {
	navLink_t link;
	const float geometricCost = ( nodes[to].origin - nodes[from].origin ).Length();
	const float edgeScale = ( geometricCost > idMath::FLOAT_EPSILON )
		? Max( 0.0f, cost / geometricCost )
		: 1.0f;

	// Do not duplicate a link that already exists with a cheaper cost.
	for ( int i = nodes[from].firstLink; i != -1; i = links[i].next ) {
		if ( links[i].node == to ) {
			if ( cost < links[i].cost ) {
				links[i].cost				= cost;
				links[i].travelType			= (short)travelType;
				links[i].actionEntityNum	= (short)actionEntityNum;
				heuristicScale = Min( heuristicScale, edgeScale );
			}
			return;
		}
	}

	link.node				= to;
	link.next				= nodes[from].firstLink;
	link.cost				= cost;
	link.travelType			= (short)travelType;
	link.actionEntityNum	= (short)actionEntityNum;

	nodes[from].firstLink = links.Append( link );
	heuristicScale = Min( heuristicScale, edgeScale );
}

/*
================
rvNavMesh::SampleFloor

Drops the agent box straight down and reports where it comes to rest.  Fails on
a pit deeper than maxDrop and on anything too steep to stand on.
================
*/
bool rvNavMesh::SampleFloor( const idVec3 &start, float maxDrop, idVec3 &floorOut ) const {
	trace_t	trace;
	idVec3	end;

	end = start;
	end.z -= maxDrop;

	NavAgentTranslation( trace, start, end );

	if ( trace.fraction >= 1.0f ) {
		return false;
	}

	// A sweep that begins inside solid also reports fraction 0, with endpos at
	// the start and whatever normal the first plane happened to have.  Callers
	// always probe from at least NAV_GROUND_OFFSET above a surface, so a zero
	// fraction here means the probe point itself was solid, not that it landed.
	if ( trace.fraction <= 0.0f ) {
		return false;
	}

	if ( trace.c.normal.z < NAV_WALKABLE_NORMAL_Z ) {
		return false;
	}

	floorOut = trace.endpos;

	return true;
}

/*
================
rvNavMesh::TryStep

Reproduces one step of player movement into the neighbouring cell: try to walk
there flat, and if that is blocked try again from step height, exactly as
idPhysics_Player does when it climbs a stair.  Whatever the horizontal move
reaches is then dropped onto the floor.
================
*/
bool rvNavMesh::TryStep( const idVec3 &fromFloor, const idVec3 &toCentre, idVec3 &toFloor, int &travelType ) const {
	const float	stepSize = pm_stepsize.GetFloat();
	const float	jumpSize = pm_jumpheight.GetFloat();

	// Try the cheapest traversal that works: walk flat, step up, then jump up.
	// The lift heights are the same ones idPhysics_Player will allow.
	const float lifts[3] = { NAV_GROUND_OFFSET, NAV_GROUND_OFFSET + stepSize, NAV_GROUND_OFFSET + jumpSize };

	for ( int attempt = 0; attempt < 3; attempt++ ) {
		trace_t	trace;
		idVec3	start = fromFloor;
		idVec3	end;

		start.z += NAV_GROUND_OFFSET;

		// Clear headroom for the lift before moving sideways at that height.
		if ( attempt > 0 ) {
			end = start;
			end.z = fromFloor.z + lifts[attempt];

			if ( !NavAgentTranslation( trace, start, end ) ) {
				return false;
			}
			start = trace.endpos;
		}

		end = toCentre;
		end.z = start.z;

		if ( !NavAgentTranslation( trace, start, end ) ) {
			continue;
		}

		// Land on whatever is under the cell centre.
		if ( !SampleFloor( trace.endpos, lifts[attempt] + NAV_MAX_DROP, toFloor ) ) {
			continue;
		}

		const float rise = toFloor.z - fromFloor.z;
		if ( rise > jumpSize ) {
			continue;
		}

		if ( attempt == 2 ) {
			// Only the jump-height lift got through, so the bot has to jump
			// whatever the destination floor turns out to be.  A railing it has
			// to hop over lands it back at the same height, and calling that a
			// walk would leave the bot pressed against the railing forever.
			travelType = NAVTRAVEL_JUMP;
		} else if ( rise > stepSize ) {
			// Reachable only by jumping, but that is not what was tried here.
			continue;
		} else {
			travelType = ( fromFloor.z - toFloor.z > stepSize ) ? NAVTRAVEL_DROP : NAVTRAVEL_WALK;
		}

		return true;
	}

	return false;
}

/*
================
rvNavMesh::GatherSeeds

Seeds are places the map itself guarantees a player can stand: spawn points,
items, jump pad landings and teleport exits.  Flooding out from those covers
the playable space without probing the solid two thirds of the world bounds.
================
*/
void rvNavMesh::GatherSeeds( idList<idVec3> &seeds ) const {
	idEntity *ent;

	for ( ent = gameLocal.spawnedEntities.Next(); ent != NULL; ent = ent->spawnNode.Next() ) {
		if ( ent->IsType( idPlayerStart::GetClassType() ) || ent->IsType( idItem::GetClassType() ) ) {
			seeds.Append( ent->GetPhysics()->GetOrigin() );
			continue;
		}

		// A jump pad launches into space; its target is where a player lands.
		if ( ent->IsType( rvJumpPad::GetClassType() ) ) {
			idBounds padBounds;
			if ( NavTravelVolumeBounds( ent, padBounds ) ) {
				seeds.Append( padBounds.GetCenter() );
			}
			for ( int i = 0; i < ent->targets.Num(); i++ ) {
				if ( ent->targets[i].GetEntity() ) {
					seeds.Append( ent->targets[i].GetEntity()->GetPhysics()->GetOrigin() );
				}
			}
		}
	}

	// Anyone already in the map is standing somewhere legal.
	for ( int i = 0; i < gameLocal.numClients; i++ ) {
		idPlayer *player = gameLocal.GetClientByNum( i );
		if ( player && !player->spectating ) {
			seeds.Append( player->GetPhysics()->GetOrigin() );
		}
	}
}

/*
================
rvNavMesh::GatherHazards

A trigger_hurt is not solid, so the generator's collision probes walk straight
into it: stock maps put 1000-damage and gib volumes at the bottom of their
pits, and without this the graph happily lays floor there and routes, roams and
recovers bots into them.  A volume that kills on one touch refuses ground
outright, like lava.  A survivable pain volume, one that starts switched off,
or one riding a mover (it is somewhere else by the time anyone arrives) only
makes the ground it covers expensive.
================
*/
void rvNavMesh::GatherHazards( void ) {
	hazards.Clear();

	for ( idEntity *ent = gameLocal.spawnedEntities.Next(); ent != NULL; ent = ent->spawnNode.Next() ) {
		if ( !ent->IsType( idTrigger_Hurt::GetClassType() ) || !ent->GetPhysics() ) {
			continue;
		}

		bool gibs = false;
		const float damage = NavHurtDamagePerTouch( ent, gibs );
		if ( damage <= 0.0f && !gibs ) {
			continue;
		}

		navHazard_t hazard;
		hazard.bounds = ent->GetPhysics()->GetAbsBounds();
		if ( hazard.bounds.IsCleared() ) {
			continue;
		}
		// idTrigger_Hurt applies its damage once per "delay" seconds of contact.
		const float delay = Max( 0.05f, ent->spawnArgs.GetFloat( "delay", "1.0" ) );
		const bool deadly = gibs || damage >= NAV_LETHAL_HURT_DAMAGE ||
							damage / delay >= NAV_LETHAL_HURT_RATE;
		hazard.lethal = deadly && ent->spawnArgs.GetBool( "on", "1" ) && !ent->IsBound();
		hazards.Append( hazard );
	}

	// A mover is floor that can leave.  One sitting just above a lethal volume
	// is a trapdoor - q4xdm14's pit panels open under whoever stands on them -
	// and one that crushes kills whoever it closes on.  The graph cannot know
	// when either happens, so the ground on and over them is made expensive:
	// usable when it is the only way, never somewhere to loiter or route
	// through for convenience.
	const int lethalVolumes = hazards.Num();
	const float agentHeight = GetAgentBounds()[1].z;
	for ( idEntity *ent = gameLocal.spawnedEntities.Next(); ent != NULL; ent = ent->spawnNode.Next() ) {
		if ( !ent->IsType( idMover::GetClassType() ) || !ent->GetPhysics() ) {
			continue;
		}

		const idBounds moverBounds = ent->GetPhysics()->GetAbsBounds();
		if ( moverBounds.IsCleared() ) {
			continue;
		}

		bool dangerous = ent->spawnArgs.GetInt( "damage", "0" ) > 0;
		for ( int i = 0; i < lethalVolumes && !dangerous; i++ ) {
			const idBounds &below = hazards[i].bounds;
			if ( !hazards[i].lethal ||
				 below[1].x < moverBounds[0].x || below[0].x > moverBounds[1].x ||
				 below[1].y < moverBounds[0].y || below[0].y > moverBounds[1].y ) {
				continue;
			}
			const float depth = moverBounds[0].z - below[1].z;
			dangerous = depth >= -4.0f && depth <= NAV_TRAPDOOR_DEPTH;
		}
		if ( !dangerous ) {
			continue;
		}

		navHazard_t hazard;
		hazard.bounds = moverBounds;
		hazard.bounds[1].z += agentHeight + 8.0f;	// standing on it counts
		hazard.lethal = false;
		hazards.Append( hazard );
	}
}

/*
================
rvNavMesh::HazardAtFloor

The agent's standing box at this floor point against every hazard volume.  A
lethal volume wins over a survivable one so callers can refuse on it.
================
*/
int rvNavMesh::HazardAtFloor( const idVec3 &floor ) const {
	if ( hazards.Num() == 0 ) {
		return -1;
	}

	idBounds agent = GetAgentBounds();
	agent.TranslateSelf( floor );

	int found = -1;
	for ( int i = 0; i < hazards.Num(); i++ ) {
		if ( !hazards[i].bounds.IntersectsBounds( agent ) ) {
			continue;
		}
		if ( hazards[i].lethal ) {
			return i;
		}
		found = i;
	}

	return found;
}

/*
================
rvNavMesh::IsHazardous
================
*/
bool rvNavMesh::IsHazardous( const idVec3 &floor, bool lethalOnly ) const {
	const int hazard = HazardAtFloor( floor );
	if ( hazard >= 0 && ( hazards[hazard].lethal || !lethalOnly ) ) {
		return true;
	}

	return NavPointInLethalLiquid( floor );
}

/*
================
rvNavMesh::FloodFill
================
*/
void rvNavMesh::FloodFill( const idList<idVec3> &seeds ) {
	idList<int>	openList;
	int			i;

	openList.SetGranularity( 1024 );

	for ( i = 0; i < seeds.Num(); i++ ) {
		idVec3 probe = seeds[i];
		idVec3 floor;

		// Seeds sit a little above the floor, and items hover.  Look up a touch
		// first so a seed embedded in the surface still resolves.
		probe.z += pm_stepsize.GetFloat();

		if ( !SampleFloor( probe, 256.0f, floor ) ) {
			continue;
		}

		int gx, gy;
		CellCoords( floor, gx, gy );

		// Re-drop at the cell centre so the node is where the grid says it is.
		idVec3 centre = CellCentre( gx, gy, floor.z + pm_stepsize.GetFloat() );
		idVec3 snapped;
		if ( !SampleFloor( centre, pm_stepsize.GetFloat() + NAV_MAX_DROP, snapped ) ) {
			continue;
		}

		if ( FindNode( gx, gy, snapped.z ) != -1 ) {
			continue;
		}

// openQ4 BEGIN
		if ( NavPointInLethalLiquid( snapped ) ) {
			lethalLiquidSeen = true;
			continue;
		}
// openQ4 END

		// An item or spawn point inside a kill volume is still not ground.
		const int seedHazard = HazardAtFloor( snapped );
		if ( seedHazard >= 0 && hazards[seedHazard].lethal ) {
			continue;
		}

		openList.Append( AddNode( gx, gy, snapped ) );
	}

	// Breadth first so the mesh grows evenly and the open list stays small.
	//
	// Hitting the node limit stops the graph GROWING; it must not stop the
	// expansion loop.  Everything still queued was discovered by some earlier
	// node and therefore already has an inbound link, so abandoning it leaves it
	// with firstLink == -1 - a sink.  BuildAreas walks the links as undirected
	// and folds a sink into the same area as the rest of the map, so the cheap
	// area rejection says "reachable", A* then pops the node, finds nowhere to
	// go, and reports no route from it at all.  Draining the queue without
	// adding to it costs one more pass over a list that can no longer grow, and
	// leaves a smaller graph that is properly closed instead of a larger one
	// with a rim nobody can move off.
	bool atNodeLimit = false;
	const float stepSize = pm_stepsize.GetFloat();

	// A level walk is the same sweep in both directions, so a proven walk links
	// back at once and the neighbour skips probing toward this node when it is
	// expanded.  Only a neighbour that has not been expanded yet gets the
	// reverse link: one that already probed this way and failed keeps its
	// verdict, so a genuinely one-way step stays one way.  This halves the
	// collision sweeps of a build, which is most of its time.
	idList<bool> expanded;
	expanded.SetGranularity( 4096 );

	for ( i = 0; i < openList.Num(); i++ ) {
		const int current = openList[i];
		expanded.AssureSize( nodes.Num(), false );
		expanded[current] = true;

		if ( !atNodeLimit && nodes.Num() >= NAV_MAX_NODES ) {
			atNodeLimit = true;
			gameLocal.Warning( "rvNavMesh: node limit (%d) reached at bot_navCellSize %.0f. Part of "
							   "this map has no navigation; raise the cell size to cover it all",
							   NAV_MAX_NODES, cellSize );
		}

		// Copied, because AddNode below can reallocate the node list.
		const idVec3	fromFloor	= nodes[current].origin;
		const int		fromGX		= nodes[current].gx;
		const int		fromGY		= nodes[current].gy;

		for ( int dir = 0; dir < 8; dir++ ) {
			const int gx = fromGX + navNeighbourX[dir];
			const int gy = fromGY + navNeighbourY[dir];

			idVec3	toFloor;
			int		travelType;

			// Already linked into that cell by a neighbour's reverse walk.
			bool linkedAlready = false;
			for ( int l = nodes[current].firstLink; l != -1; l = links[l].next ) {
				const navNode_t &linked = nodes[links[l].node];
				if ( linked.gx == gx && linked.gy == gy ) {
					linkedAlready = true;
					break;
				}
			}
			if ( linkedAlready ) {
				continue;
			}

			if ( !TryStep( fromFloor, CellCentre( gx, gy, fromFloor.z ), toFloor, travelType ) ) {
				continue;
			}

			int neighbour = FindNode( gx, gy, toFloor.z );
			if ( neighbour == -1 ) {
				// Out of nodes: link only to ground that already exists.
				if ( atNodeLimit ) {
					continue;
				}
// openQ4 BEGIN
				if ( NavPointInLethalLiquid( toFloor ) ) {
					lethalLiquidSeen = true;
					continue;
				}
// openQ4 END
				const int hazard = HazardAtFloor( toFloor );
				if ( hazard >= 0 && hazards[hazard].lethal ) {
					continue;
				}
				neighbour = AddNode( gx, gy, toFloor );
				openList.Append( neighbour );
			}

			const float distance = ( toFloor - fromFloor ).Length();
			float cost = distance;
			if ( travelType == NAVTRAVEL_DROP ) {
				cost += NAV_DROP_PENALTY;
				const float fall = fromFloor.z - toFloor.z;
				if ( fall > NAV_SOFT_FALL ) {
					cost += NAV_FALL_DAMAGE_PENALTY;
				}
				if ( fall > NAV_HARD_FALL ) {
					cost += NAV_FALL_DAMAGE_PENALTY;
				}
			} else if ( travelType == NAVTRAVEL_JUMP ) {
				cost += NAV_JUMP_PENALTY;
			}
			if ( nodes[neighbour].flags & NAVNODE_HAZARD ) {
				cost += NAV_HAZARD_PENALTY;
			}

			LinkNodes( current, neighbour, cost, travelType );

			// A drop is one way unless the reverse step is walkable on its own,
			// which the neighbour discovers when it is expanded.  A level walk is
			// linked back now, before the neighbour spends a sweep proving it.
			expanded.AssureSize( nodes.Num(), false );
			if ( travelType == NAVTRAVEL_WALK && !expanded[neighbour] &&
				 idMath::Fabs( toFloor.z - fromFloor.z ) <= stepSize ) {
				const float reverseCost = distance +
					( ( nodes[current].flags & NAVNODE_HAZARD ) ? NAV_HAZARD_PENALTY : 0.0f );
				LinkNodes( neighbour, current, reverseCost, NAVTRAVEL_WALK );
			}
		}
	}
}

/*
================
rvNavMesh::LevelNodeInCell

The node in a cell standing closest to height z, within tolerance, or -1.
================
*/
int rvNavMesh::LevelNodeInCell( int gx, int gy, float z, float tolerance ) const {
	const int key = nodeHash.GenerateKey( gx, gy );
	int best = -1;
	float bestDelta = tolerance;

	for ( int i = nodeHash.First( key ); i != -1; i = nodeHash.Next( i ) ) {
		if ( nodes[i].gx != gx || nodes[i].gy != gy ) {
			continue;
		}
		const float delta = idMath::Fabs( nodes[i].origin.z - z );
		if ( delta <= bestDelta ) {
			bestDelta = delta;
			best = i;
		}
	}

	return best;
}

/*
================
rvNavMesh::GapJumpClear

Proves a running jump from one floor point to another: the parabola of a jump
at a conservative run speed reaches the landing's height no earlier than it
is over the landing, the real hull clears everything along that arc from a
takeoff moved back a little, and there is floor where it comes down.
================
*/
bool rvNavMesh::GapJumpClear( const idVec3 &start, const idVec3 &end, float runSpeed,
							  float jumpSpeed, float gravity ) const {
	idVec3 direction = end - start;
	direction.z = 0.0f;
	const float distance = direction.Normalize();
	if ( distance <= 0.0f ) {
		return false;
	}

	// The edge must not be right under the takeoff node: a bot reaches the
	// node a corner radius early and may need a frame to leave the ground.
	idVec3 lip;
	if ( !SampleFloor( start + direction * 8.0f + idVec3( 0.0f, 0.0f, NAV_GROUND_OFFSET + 8.0f ), 24.0f, lip ) ||
		 idMath::Fabs( lip.z - start.z ) > pm_stepsize.GetFloat() ) {
		return false;
	}

	const idVec3 takeoff = start - direction * NAV_GAP_TAKEOFF_SLACK;
	const float span = distance + NAV_GAP_TAKEOFF_SLACK;
	const float rise = end.z - start.z;
	const float discriminant = jumpSpeed * jumpSpeed - 2.0f * gravity * rise;
	if ( discriminant < 0.0f ) {
		return false;		// too high to reach at all
	}
	const float landTime = ( jumpSpeed + idMath::Sqrt( discriminant ) ) / gravity;
	if ( runSpeed * landTime < span ) {
		return false;		// comes down short of the landing
	}

	// Sweep the arc in short segments up to the moment it is over the landing.
	const float overTime = span / runSpeed;
	const int segments = Max( 2, (int)idMath::Ceil( span / 16.0f ) );
	idVec3 previous = takeoff + idVec3( 0.0f, 0.0f, NAV_GROUND_OFFSET );
	trace_t trace;

	for ( int i = 1; i <= segments; i++ ) {
		const float t = overTime * (float)i / (float)segments;
		const idVec3 point = takeoff + direction * ( runSpeed * t ) +
			idVec3( 0.0f, 0.0f, NAV_GROUND_OFFSET + jumpSpeed * t - 0.5f * gravity * t * t );
		if ( !NavAgentTranslation( trace, previous, point ) ) {
			return false;
		}
		previous = point;
	}

	idVec3 landed;
	if ( !SampleFloor( previous, previous.z - end.z + 32.0f, landed ) ) {
		return false;
	}
	return idMath::Fabs( landed.z - end.z ) <= pm_stepsize.GetFloat();
}

/*
================
rvNavMesh::AddGapJumpLinks

Running jumps across pits and between platforms that flood fill, which only
steps, hops and drops between neighbouring cells, cannot connect.  From a
ledge node - no level floor in front of it - with a straight run-up behind it,
each grid direction is searched for the first floor beyond the gap within a
running jump's reach, and the jump is linked only if GapJumpClear proves it.
Stock maps hold real items behind such jumps: q4xdm14's mega health alcove
over the lava pit, q4dm5's red armour ledge.
================
*/
void rvNavMesh::AddGapJumpLinks( void ) {
	const idVec3 &gravityVector = gameLocal.GetGravity();
	const float gravity = ( gravityVector.LengthSqr() > 1.0f ) ? gravityVector.Length() : 800.0f;
	const float stepSize = pm_stepsize.GetFloat();
	const float jumpHeight = pm_jumpheight.GetFloat();
	const float runSpeed = Max( 1.0f, pm_speed.GetFloat() ) * NAV_GAP_SPEED_FRACTION;
	const float jumpSpeed = idMath::Sqrt( 2.0f * gravity * jumpHeight );
	const float longestAir = ( jumpSpeed + idMath::Sqrt( jumpSpeed * jumpSpeed + 2.0f * gravity * NAV_GAP_MAX_FALL ) ) / gravity;
	const float maxReach = runSpeed * longestAir;
	const int numBuilt = nodes.Num();
	int added = 0;

	for ( int takeoff = 0; takeoff < numBuilt; takeoff++ ) {
		const idVec3 origin = nodes[takeoff].origin;
		const int gx = nodes[takeoff].gx;
		const int gy = nodes[takeoff].gy;

		for ( int dir = 0; dir < 8; dir++ ) {
			const int dx = navNeighbourX[dir];
			const int dy = navNeighbourY[dir];

			// A ledge: nothing level to walk onto in this direction...
			if ( LevelNodeInCell( gx + dx, gy + dy, origin.z, stepSize ) >= 0 ) {
				continue;
			}
			// ...and a straight run-up behind it.
			const int runup = LevelNodeInCell( gx - dx, gy - dy, origin.z, stepSize );
			if ( runup < 0 || FindLink( runup, takeoff ) < 0 ) {
				continue;
			}

			const float cellStep = cellSize * ( ( dx != 0 && dy != 0 ) ? idMath::SQRT_TWO : 1.0f );
			const int maxCells = (int)( maxReach / cellStep );
			for ( int k = 2; k <= maxCells; k++ ) {
				// The highest floor in the landing band: no more than a jump's
				// height above the takeoff, no more than NAV_GAP_MAX_FALL below.
				const int cx = gx + dx * k;
				const int cy = gy + dy * k;
				const int key = nodeHash.GenerateKey( cx, cy );
				int landing = -1;
				for ( int n = nodeHash.First( key ); n != -1; n = nodeHash.Next( n ) ) {
					if ( nodes[n].gx != cx || nodes[n].gy != cy ) {
						continue;
					}
					const float rise = nodes[n].origin.z - origin.z;
					if ( rise > jumpHeight - 8.0f || rise < -NAV_GAP_MAX_FALL ) {
						continue;
					}
					if ( landing < 0 || nodes[n].origin.z > nodes[landing].origin.z ) {
						landing = n;
					}
				}
				if ( landing < 0 ) {
					continue;	// still over the gap
				}

				// The first floor beyond the gap decides this direction.
				if ( FindLink( takeoff, landing ) < 0 &&
					 GapJumpClear( origin, nodes[landing].origin, runSpeed, jumpSpeed, gravity ) ) {
					const float cost = ( nodes[landing].origin - origin ).Length() + NAV_JUMP_PENALTY +
						NAV_GAP_PENALTY + ( ( nodes[landing].flags & NAVNODE_HAZARD ) ? NAV_HAZARD_PENALTY : 0.0f );
					LinkNodes( takeoff, landing, cost, NAVTRAVEL_JUMP );
					added++;
				}
				break;
			}
		}
	}

	if ( bot_debug.GetBool() ) {
		gameLocal.Printf( "rvNavMesh: %d gap jump links\n", added );
	}
	gapJumpLinks = added;
}

/*
================
rvNavMesh::NodeForEntityPoint

Snaps an entity's world position onto the mesh.  Entities that matter here -
jump pads, teleport exits - are placed by mappers with a generous margin, so a
wide search radius is the right trade.
================
*/
int rvNavMesh::NodeForEntityPoint( const idVec3 &point ) const {
	idVec3 floor;
	idVec3 probe = point;

	probe.z += pm_stepsize.GetFloat();

	if ( SampleFloor( probe, 512.0f, floor ) ) {
		const int node = FindNearestNode( floor, 128.0f );
		if ( node != -1 ) {
			return node;
		}
	}

	return FindNearestNode( point, 256.0f );
}

/*
================
rvNavMesh::AddOffMeshLinks

Jump pads and teleporters move a player between places the walking flood fill
can never connect.  Both are "walk into the volume and you are carried", so
they become one way links whose entry point is the volume itself.
================
*/
void rvNavMesh::AddOffMeshLinks( void ) {
	idEntity *ent;
	int		 numLinksAdded = 0;

	for ( ent = gameLocal.spawnedEntities.Next(); ent != NULL; ent = ent->spawnNode.Next() ) {
		int		travelType;
		float	cost;

		if ( ent->IsType( rvJumpPad::GetClassType() ) ) {
			travelType	= NAVTRAVEL_JUMPPAD;
			cost		= NAV_JUMPPAD_COST;
		} else if ( ent->IsType( idTrigger_Multi::GetClassType() ) ) {
			// idTrigger_Multi doubles as the teleporter trigger: a single
			// target, and that target is a player start.
			if ( ent->targets.Num() != 1 || !ent->targets[0].GetEntity() ) {
				continue;
			}
			if ( !ent->targets[0].GetEntity()->IsType( idPlayerStart::GetClassType() ) ) {
				continue;
			}
			travelType	= NAVTRAVEL_TELEPORT;
			cost		= NAV_TELEPORT_COST;
		} else {
			continue;
		}

		if ( ent->targets.Num() < 1 || !ent->targets[0].GetEntity() ) {
			continue;
		}

		idBounds entryBounds;
		if ( !NavTravelVolumeBounds( ent, entryBounds ) ) {
			continue;
		}

		const int from = NodeForEntityPoint( entryBounds.GetCenter() );
		const int to   = NodeForEntityPoint( ent->targets[0].GetEntity()->GetPhysics()->GetOrigin() );

		if ( from == -1 || to == -1 || from == to ) {
			continue;
		}

		LinkNodes( from, to, cost, travelType, ent->entityNumber );
		numLinksAdded++;

		// Only a volume that is carrying players right now seals the ground
		// under it.  A pad switched off at load is floor until something turns
		// it on, and a disabled trigger has no contents to touch.
		const bool live = ( travelType == NAVTRAVEL_JUMPPAD ) ?
			ent->IsActive( TH_THINK ) :
			( ent->GetPhysics()->GetContents() != 0 );
		if ( live ) {
			navTransport_t transport;
			transport.bounds		= entryBounds;
			transport.entityNum		= ent->entityNumber;
			transport.travelType	= travelType;
			transport.destination	= to;
			transport.cost			= cost;
			transports.Append( transport );
		}
	}

	if ( bot_debug.GetBool() ) {
		gameLocal.Printf( "rvNavMesh: %d off-mesh links\n", numLinksAdded );
	}
}

/*
================
rvNavMesh::SealTransportVolumes

A jump pad or a teleporter takes whoever touches it, so the ground under its
volume is not floor anyone walks across.  Flood fill laid ordinary links over
it all the same, and routes along them launched bots somewhere they never
meant to go: on q4dm5, a map of forty pads, most stuck reports were bots
carried off mid-route by a pad they were only walking past, or dropped onto a
landing beside one that threw them straight back up.  Every node whose
standing box touches a live volume keeps the transport as its only exit, so a
route either takes it or goes round, and WalkableLine refuses a shortcut that
would step into one.  Links into the volume stay: entering it is how a route
takes the transport.
================
*/
void rvNavMesh::SealTransportVolumes( void ) {
	transportNodes = 0;
	if ( transports.Num() == 0 ) {
		return;
	}

	const idBounds &agent = GetAgentBounds();
	idList<int> sealedBy;
	sealedBy.SetNum( nodes.Num() );
	for ( int i = 0; i < nodes.Num(); i++ ) {
		sealedBy[i] = -1;
	}

	for ( int t = 0; t < transports.Num(); t++ ) {
		// The cells whose standing box can reach the volume: the volume grown
		// by the box, horizontally.
		const idBounds &volume = transports[t].bounds;
		int minX, minY, maxX, maxY;
		CellCoords( volume[0] - idVec3( agent[1].x, agent[1].y, 0.0f ), minX, minY );
		CellCoords( volume[1] - idVec3( agent[0].x, agent[0].y, 0.0f ), maxX, maxY );

		for ( int gy = minY; gy <= maxY; gy++ ) {
			for ( int gx = minX; gx <= maxX; gx++ ) {
				const int key = nodeHash.GenerateKey( gx, gy );
				for ( int n = nodeHash.First( key ); n != -1; n = nodeHash.Next( n ) ) {
					if ( nodes[n].gx != gx || nodes[n].gy != gy || sealedBy[n] != -1 ) {
						continue;
					}
					idBounds standing = agent;
					standing.TranslateSelf( nodes[n].origin );
					if ( standing.IntersectsBounds( volume ) ) {
						sealedBy[n] = t;
					}
				}
			}
		}
	}

	for ( int n = 0; n < nodes.Num(); n++ ) {
		const int t = sealedBy[n];
		if ( t < 0 ) {
			continue;
		}

		const navTransport_t &transport = transports[t];
		nodes[n].flags |= NAVNODE_TRANSPORT;
		nodes[n].firstLink = -1;
		if ( n != transport.destination ) {
			LinkNodes( n, transport.destination, transport.cost, transport.travelType, transport.entityNum );
		}
		transportNodes++;
	}
}

/*
================
rvNavMesh::CompactLinks

Generation appends links in discovery order - a node's walks, then the reverse
walks its neighbours added, then gap jumps, then transports - so walking one
node's list jumped all over a quarter of a million entries, and every search
paid a cache miss per edge for it.  Once the graph is final, rewrite the
array so each node's links are one contiguous run in node order.  The lists
keep their form, so nothing that walks them changes; links that sealing
unhooked are dropped on the way.
================
*/
void rvNavMesh::CompactLinks( void ) {
	idList<navLink_t> compact;
	int live = 0;
	for ( int n = 0; n < nodes.Num(); n++ ) {
		for ( int l = nodes[n].firstLink; l != -1; l = links[l].next ) {
			live++;
		}
	}
	compact.SetNum( live );

	int write = 0;
	for ( int n = 0; n < nodes.Num(); n++ ) {
		const int first = write;
		for ( int l = nodes[n].firstLink; l != -1; l = links[l].next ) {
			compact[write] = links[l];
			compact[write].next = write + 1;
			write++;
		}
		if ( write > first ) {
			compact[write - 1].next = -1;
			nodes[n].firstLink = first;
		} else {
			nodes[n].firstLink = -1;
		}
	}

	links.Swap( compact );
}

/*
================
rvNavMesh::LineEntersTransport

Would a standing box carried along this line touch a transport volume it did
not start in?  The line is a string-pulled one, which may float up to
NAV_SMOOTH_MAX_DIP over the ground it crosses, so the box is taken to reach
that far below it.  A line that begins in a volume is leaving it, which is
how a bot already on a pad snaps onto the mesh.
================
*/
bool rvNavMesh::LineEntersTransport( const idVec3 &from, const idVec3 &to ) const {
	if ( transports.Num() == 0 ) {
		return false;
	}

	const idBounds &agent = GetAgentBounds();
	idBounds sweep;
	sweep.Clear();
	sweep.AddPoint( from );
	sweep.AddPoint( to );
	const idVec3 delta = to - from;

	for ( int t = 0; t < transports.Num(); t++ ) {
		// The line touches the volume grown by the box exactly when the box,
		// carried along it, touches the volume.
		idBounds grown;
		grown[0] = transports[t].bounds[0] - agent[1];
		grown[1] = transports[t].bounds[1] - agent[0];
		grown[1].z += NAV_SMOOTH_MAX_DIP;

		if ( !grown.IntersectsBounds( sweep ) || grown.ContainsPoint( from ) ) {
			continue;
		}

		float enter = 0.0f;
		float leave = 1.0f;
		bool crosses = true;
		for ( int axis = 0; axis < 3 && crosses; axis++ ) {
			if ( idMath::Fabs( delta[axis] ) < idMath::FLOAT_EPSILON ) {
				crosses = from[axis] >= grown[0][axis] && from[axis] <= grown[1][axis];
				continue;
			}
			// Not "near"/"far": windef.h defines both away.
			float lo = ( grown[0][axis] - from[axis] ) / delta[axis];
			float hi = ( grown[1][axis] - from[axis] ) / delta[axis];
			if ( lo > hi ) {
				const float swap = lo;
				lo = hi;
				hi = swap;
			}
			enter = Max( enter, lo );
			leave = Min( leave, hi );
			crosses = enter <= leave;
		}
		if ( crosses ) {
			return true;
		}
	}

	return false;
}

/*
================
rvNavMesh::TransportAt
================
*/
int rvNavMesh::TransportAt( const idBounds &absBounds ) const {
	for ( int t = 0; t < transports.Num(); t++ ) {
		if ( transports[t].bounds.IntersectsBounds( absBounds ) ) {
			return transports[t].entityNum;
		}
	}
	return -1;
}

/*
================
rvNavMesh::BuildAreas

Connected components over the links treated as undirected.  Two nodes in
different components definitely cannot reach each other, which lets a bot
reject an unreachable goal without paying for a full search.  The converse is
not guaranteed - a one way drop puts both ends in the same component - so this
is only ever used to say no.
================
*/
void rvNavMesh::BuildAreas( void ) {
	idList<int>	stack;
	int			i;

	// Reverse adjacency, so an undirected walk can follow one way links back.
	idList<int> reverseHead;
	idList<int> reverseNext;
	idList<int> reverseNode;

	reverseHead.AssureSize( nodes.Num(), -1 );
	reverseNext.SetGranularity( 4096 );
	reverseNode.SetGranularity( 4096 );

	for ( i = 0; i < nodes.Num(); i++ ) {
		for ( int l = nodes[i].firstLink; l != -1; l = links[l].next ) {
			const int dest = links[l].node;
			reverseNode.Append( i );
			reverseNext.Append( reverseHead[dest] );
			reverseHead[dest] = reverseNode.Num() - 1;
		}
	}

	numAreas = 0;

	for ( i = 0; i < nodes.Num(); i++ ) {
		if ( nodes[i].area != -1 ) {
			continue;
		}

		const int area = numAreas++;

		stack.Clear();
		stack.Append( i );
		nodes[i].area = area;

		while ( stack.Num() ) {
			const int current = stack[stack.Num() - 1];
			stack.SetNum( stack.Num() - 1, false );

			for ( int l = nodes[current].firstLink; l != -1; l = links[l].next ) {
				if ( nodes[links[l].node].area == -1 ) {
					nodes[links[l].node].area = area;
					stack.Append( links[l].node );
				}
			}

			for ( int r = reverseHead[current]; r != -1; r = reverseNext[r] ) {
				if ( nodes[reverseNode[r]].area == -1 ) {
					nodes[reverseNode[r]].area = area;
					stack.Append( reverseNode[r] );
				}
			}
		}
	}
}

/*
================
rvNavMesh::Build
================
*/
bool rvNavMesh::Build( void ) {
	idList<idVec3>	seeds;
	const int		startMsec = Sys_Milliseconds();

	// There is no collision world to probe until a map is running, and
	// GetWorldBounds would index an empty clip world list.
	if ( gameLocal.GameState() != GAMESTATE_ACTIVE ) {
		gameLocal.Warning( "rvNavMesh: no map is running, cannot build a navmesh" );
		return false;
	}

	Clear();

	cellSize = bot_navCellSize.GetFloat();
	if ( cellSize < 8.0f ) {
		cellSize = 8.0f;
	}

	gridOrigin = gameLocal.GetWorldBounds( NULL )[0];

	navAgentTrace = idTraceModel( GetAgentBounds() );
	navAgentModel = new idClipModel( navAgentTrace );

	seeds.SetGranularity( 256 );
	GatherSeeds( seeds );

	if ( seeds.Num() == 0 ) {
		gameLocal.Warning( "rvNavMesh: no spawn points or items to seed from, cannot build a navmesh" );
		Clear();
		return false;
	}

	GatherHazards();
	GatherDynamicSolids();
	FloodFill( seeds );

	if ( nodes.Num() == 0 ) {
		gameLocal.Warning( "rvNavMesh: no walkable ground found from %d seeds", seeds.Num() );
		Clear();
		return false;
	}

	AddGapJumpLinks();
	AddOffMeshLinks();
	SealTransportVolumes();
	CompactLinks();
	BuildAreas();
	BuildComponents();

	// The graph is fixed from here on, so the search scratch can be sized once
	// instead of on every query.
	gScore.SetNum( nodes.Num(), true );
	fScore.SetNum( nodes.Num(), true );
	cameFrom.SetNum( nodes.Num(), true );
	openTag.SetNum( nodes.Num(), true );
	closedTag.SetNum( nodes.Num(), true );
	fieldTargetTag.SetNum( nodes.Num(), true );
	fieldPricedTag.SetNum( nodes.Num(), true );
	fieldPricedCost.SetNum( nodes.Num(), true );
	heap.Resize( 1024, 1024 );

	for ( int i = 0; i < nodes.Num(); i++ ) {
		openTag[i]	 = 0;
		closedTag[i] = 0;
		fieldTargetTag[i] = 0;
		fieldPricedTag[i] = 0;
	}
	searchSerial = 0;
	fieldSerial = 0;

	BuildLandmarks();

	entitySnaps.SetNum( MAX_GENTITIES, true );
	for ( int i = 0; i < MAX_GENTITIES; i++ ) {
		entitySnaps[i].spawnId = -1;
		entitySnaps[i].node = -1;
		entitySnaps[i].origin.Zero();
	}
	ResetStats();

	// The probe model is only needed for generation; routing traces reuse it,
	// so it stays alive until Clear().
	buildMsec = Sys_Milliseconds() - startMsec;

	int hazardNodes = 0;
	for ( int i = 0; i < nodes.Num(); i++ ) {
		if ( nodes[i].flags & NAVNODE_HAZARD ) {
			hazardNodes++;
		}
	}

	gameLocal.Printf( "rvNavMesh: %d nodes, %d links, %d areas, %d components, %d landmarks, "
					  "%d gap jumps, %d hazard volumes (%d priced nodes), %d transports (%d sealed nodes), "
					  "%d seeds in %d ms (cell %g)\n",
					  nodes.Num(), links.Num(), numAreas, numComponents, numLandmarks, gapJumpLinks,
					  hazards.Num(), hazardNodes, transports.Num(), transportNodes, seeds.Num(), buildMsec,
					  cellSize );

	return true;
}

/*
================
rvNavMesh::BuildComponents

Strongly connected components by an iterative Tarjan pass, then the transitive
closure of their condensation.  Weak areas can only ever say "no": a one-way
drop folds a pit and the floor above it into one area although nothing in the
pit can climb back out.  The closure answers the directed question exactly, so
a goal nobody can reach - an enemy standing in that pit - is refused in
constant time instead of by an A* that floods every node reachable from the
bot before it gives up, which on a failed chase is the whole map, every 750 ms.

Tarjan emits a component only after every component reachable from it, so the
ids run from sinks up, and a component's successors always have smaller ids:
one pass in id order ORs complete rows into the row being built.
================
*/
void rvNavMesh::BuildComponents( void ) {
	const int numNodes = nodes.Num();

	idList<int> index;
	idList<int> lowLink;
	idList<int> stack;
	idList<bool> onStack;
	idList<int> frameNode;
	idList<int> frameLink;

	index.AssureSize( numNodes, -1 );
	lowLink.AssureSize( numNodes, 0 );
	onStack.AssureSize( numNodes, false );
	stack.SetGranularity( 1024 );
	frameNode.SetGranularity( 1024 );
	frameLink.SetGranularity( 1024 );

	int nextIndex = 0;
	numComponents = 0;

	for ( int root = 0; root < numNodes; root++ ) {
		if ( index[root] != -1 ) {
			continue;
		}

		index[root] = lowLink[root] = nextIndex++;
		stack.Append( root );
		onStack[root] = true;
		frameNode.Append( root );
		frameLink.Append( nodes[root].firstLink );

		while ( frameNode.Num() ) {
			const int top = frameNode.Num() - 1;
			const int v = frameNode[top];
			const int l = frameLink[top];

			if ( l != -1 ) {
				frameLink[top] = links[l].next;

				const int w = links[l].node;
				if ( index[w] == -1 ) {
					index[w] = lowLink[w] = nextIndex++;
					stack.Append( w );
					onStack[w] = true;
					frameNode.Append( w );
					frameLink.Append( nodes[w].firstLink );
				} else if ( onStack[w] ) {
					lowLink[v] = Min( lowLink[v], index[w] );
				}
				continue;
			}

			if ( lowLink[v] == index[v] ) {
				while ( true ) {
					const int member = stack[stack.Num() - 1];
					stack.SetNum( stack.Num() - 1, false );
					onStack[member] = false;
					nodes[member].component = numComponents;
					if ( member == v ) {
						break;
					}
				}
				numComponents++;
			}

			frameNode.SetNum( top, false );
			frameLink.SetNum( top, false );
			if ( top > 0 ) {
				const int parent = frameNode[top - 1];
				lowLink[parent] = Min( lowLink[parent], lowLink[v] );
			}
		}
	}

	componentReach.Clear();
	componentWords = 0;
	if ( numComponents <= 0 || numComponents > NAV_MAX_CLOSURE_COMPONENTS ) {
		if ( numComponents > NAV_MAX_CLOSURE_COMPONENTS ) {
			gameLocal.Warning( "rvNavMesh: %d strongly connected components is more than the %d the "
							   "reachability closure covers; cross-component queries will search",
							   numComponents, NAV_MAX_CLOSURE_COMPONENTS );
		}
		return;
	}

	componentWords = ( numComponents + 31 ) >> 5;
	componentReach.AssureSize( numComponents * componentWords, 0u );

	// Group nodes by component so each row is built from its own members.
	idList<int> componentStart;
	idList<int> componentNodes;
	componentStart.AssureSize( numComponents + 1, 0 );
	componentNodes.SetNum( numNodes, false );
	for ( int i = 0; i < numNodes; i++ ) {
		componentStart[nodes[i].component + 1]++;
	}
	for ( int c = 0; c < numComponents; c++ ) {
		componentStart[c + 1] += componentStart[c];
	}
	idList<int> fill;
	fill.AssureSize( numComponents, 0 );
	for ( int i = 0; i < numNodes; i++ ) {
		const int c = nodes[i].component;
		componentNodes[componentStart[c] + fill[c]++] = i;
	}

	for ( int c = 0; c < numComponents; c++ ) {
		unsigned int *row = &componentReach[c * componentWords];
		row[c >> 5] |= 1u << ( c & 31 );

		for ( int m = componentStart[c]; m < componentStart[c + 1]; m++ ) {
			const int member = componentNodes[m];
			for ( int l = nodes[member].firstLink; l != -1; l = links[l].next ) {
				const int successor = nodes[links[l].node].component;
				if ( successor == c ) {
					continue;
				}
				assert( successor < c );
				const unsigned int *successorRow = &componentReach[successor * componentWords];
				for ( int w = 0; w < componentWords; w++ ) {
					row[w] |= successorRow[w];
				}
			}
		}
	}
}

/*
================
rvNavMesh::NodeCanReach
================
*/
bool rvNavMesh::NodeCanReach( int fromNode, int toNode ) const {
	if ( fromNode < 0 || fromNode >= nodes.Num() || toNode < 0 || toNode >= nodes.Num() ) {
		return false;
	}

	const int from = nodes[fromNode].component;
	const int to = nodes[toNode].component;
	if ( from == to ) {
		return true;
	}
	if ( nodes[fromNode].area != nodes[toNode].area ) {
		return false;
	}
	if ( componentWords <= 0 || from < 0 || to < 0 ) {
		return true;		// unknown: let the search decide
	}

	return ( componentReach[from * componentWords + ( to >> 5 )] & ( 1u << ( to & 31 ) ) ) != 0;
}

/*
================
rvNavMesh::Dijkstra

Unbounded single-source costs over the forward links, or over the reverse
adjacency for "cost to reach source".  Build-time only: it borrows the A*
heap and closed tags, so it must never run while a cost field is live.
================
*/
void rvNavMesh::Dijkstra( int source, bool reverse, float *distances ) const {
	const int numNodes = nodes.Num();

	for ( int i = 0; i < numNodes; i++ ) {
		distances[i] = NAV_INFINITE_COST;
	}
	if ( source < 0 || source >= numNodes ) {
		return;
	}

	BeginSearch();
	heap.SetNum( 0, false );

	distances[source] = 0.0f;
	HeapPush( source, 0.0f );

	while ( heap.Num() ) {
		const int current = HeapPop();
		if ( closedTag[current] == searchSerial ) {
			continue;
		}
		closedTag[current] = searchSerial;

		const float base = distances[current];
		if ( reverse ) {
			for ( int r = reverseOffset[current]; r < reverseOffset[current + 1]; r++ ) {
				const int previous = reverseSource[r];
				const float tentative = base + reverseCost[r];
				if ( tentative < distances[previous] ) {
					distances[previous] = tentative;
					HeapPush( previous, tentative );
				}
			}
		} else {
			for ( int l = nodes[current].firstLink; l != -1; l = links[l].next ) {
				const int next = links[l].node;
				const float tentative = base + links[l].cost;
				if ( tentative < distances[next] ) {
					distances[next] = tentative;
					HeapPush( next, tentative );
				}
			}
		}
	}
}

/*
================
rvNavMesh::BuildLandmarks

Picks landmarks by farthest-point sampling inside the largest strongly
connected component - the playable bulk of the map, where every node is
mutually reachable and every landmark cost is finite - and records exact
travel costs from and to each one.  Round-trip separation drives the choice,
so one-way drops cannot make a landmark look farther than it is.
================
*/
void rvNavMesh::BuildLandmarks( void ) {
	const int numNodes = nodes.Num();

	numLandmarks = 0;
	landmarkNodes.Clear();
	landmarkCost.Clear();

	if ( numNodes < 64 || numComponents <= 0 ) {
		return;
	}

	// Reverse adjacency, compressed by destination.
	reverseOffset.SetNum( numNodes + 1, false );
	for ( int i = 0; i <= numNodes; i++ ) {
		reverseOffset[i] = 0;
	}
	for ( int i = 0; i < numNodes; i++ ) {
		for ( int l = nodes[i].firstLink; l != -1; l = links[l].next ) {
			reverseOffset[links[l].node + 1]++;
		}
	}
	for ( int i = 0; i < numNodes; i++ ) {
		reverseOffset[i + 1] += reverseOffset[i];
	}
	reverseSource.SetNum( links.Num(), false );
	reverseCost.SetNum( links.Num(), false );
	{
		idList<int> cursor;
		cursor.SetNum( numNodes, false );
		for ( int i = 0; i < numNodes; i++ ) {
			cursor[i] = reverseOffset[i];
		}
		for ( int i = 0; i < numNodes; i++ ) {
			for ( int l = nodes[i].firstLink; l != -1; l = links[l].next ) {
				const int slot = cursor[links[l].node]++;
				reverseSource[slot] = i;
				reverseCost[slot] = links[l].cost;
			}
		}
	}

	// The largest strongly connected component.
	idList<int> componentSize;
	componentSize.AssureSize( numComponents, 0 );
	for ( int i = 0; i < numNodes; i++ ) {
		componentSize[nodes[i].component]++;
	}
	int bulk = 0;
	for ( int c = 1; c < numComponents; c++ ) {
		if ( componentSize[c] > componentSize[bulk] ) {
			bulk = c;
		}
	}
	if ( componentSize[bulk] < 64 ) {
		reverseOffset.Clear();
		reverseSource.Clear();
		reverseCost.Clear();
		return;
	}

	int seed = -1;
	for ( int i = 0; i < numNodes && seed < 0; i++ ) {
		if ( nodes[i].component == bulk ) {
			seed = i;
		}
	}

	idList<float> separation;
	separation.SetNum( numNodes, false );
	Dijkstra( seed, false, separation.Ptr() );

	// The first landmark is the far end of the map from an arbitrary member.
	int candidate = -1;
	float farthest = -1.0f;
	for ( int i = 0; i < numNodes; i++ ) {
		if ( nodes[i].component == bulk && separation[i] < NAV_UNREACHED && separation[i] > farthest ) {
			farthest = separation[i];
			candidate = i;
		}
	}
	for ( int i = 0; i < numNodes; i++ ) {
		separation[i] = NAV_INFINITE_COST;
	}

	// Each node's costs to and from every landmark sit together in one row,
	// so a heuristic lookup reads one cache line for the node instead of one
	// per landmark and direction from tables a megabyte apart.
	landmarkCost.SetNum( NAV_LANDMARK_STRIDE * numNodes, false );
	idList<float> fromScratch;
	idList<float> toScratch;
	fromScratch.SetNum( numNodes, false );
	toScratch.SetNum( numNodes, false );

	while ( candidate >= 0 && numLandmarks < NAV_MAX_LANDMARKS ) {
		const float *from = fromScratch.Ptr();
		const float *to = toScratch.Ptr();

		Dijkstra( candidate, false, fromScratch.Ptr() );
		Dijkstra( candidate, true, toScratch.Ptr() );
		for ( int i = 0; i < numNodes; i++ ) {
			landmarkCost[i * NAV_LANDMARK_STRIDE + numLandmarks * 2]		= from[i];
			landmarkCost[i * NAV_LANDMARK_STRIDE + numLandmarks * 2 + 1]	= to[i];
		}
		landmarkNodes.Append( candidate );
		numLandmarks++;

		// Next: the member whose round trip to every chosen landmark is longest.
		candidate = -1;
		farthest = 0.0f;
		for ( int i = 0; i < numNodes; i++ ) {
			if ( nodes[i].component != bulk ) {
				continue;
			}
			separation[i] = Min( separation[i], from[i] + to[i] );
			if ( separation[i] > farthest ) {
				farthest = separation[i];
				candidate = i;
			}
		}
	}

	reverseOffset.Clear();
	reverseSource.Clear();
	reverseCost.Clear();
}

/*
================
rvNavMesh::Heuristic

Admissible and consistent lower bound on the route cost from node to goal:
the larger of scaled straight-line distance and every landmark's triangle
bound.  A landmark that can reach the node but not the goal, or that the goal
can reach but the node cannot, proves the node can never lead to the goal, and
the search drops it instead of flooding a dead-end pit.
================
*/
float rvNavMesh::Heuristic( int node, int goal ) const {
	float h = heuristicScale * ( nodes[goal].origin - nodes[node].origin ).Length();
	const float *nodeRow = landmarkCost.Ptr() + node * NAV_LANDMARK_STRIDE;
	const float *goalRow = landmarkCost.Ptr() + goal * NAV_LANDMARK_STRIDE;

	for ( int k = 0; k < numLandmarks; k++ ) {
		// cost( node -> goal ) >= cost( L -> goal ) - cost( L -> node )
		const float landmarkGoal = goalRow[k * 2];
		const float landmarkNode = nodeRow[k * 2];
		if ( landmarkGoal < NAV_UNREACHED ) {
			if ( landmarkNode < NAV_UNREACHED ) {
				h = Max( h, landmarkGoal - landmarkNode );
			}
		} else if ( landmarkNode < NAV_UNREACHED ) {
			return NAV_INFINITE_COST;
		}

		// cost( node -> goal ) >= cost( node -> L ) - cost( goal -> L )
		const float goalLandmark = goalRow[k * 2 + 1];
		const float nodeLandmark = nodeRow[k * 2 + 1];
		if ( goalLandmark < NAV_UNREACHED ) {
			if ( nodeLandmark >= NAV_UNREACHED ) {
				return NAV_INFINITE_COST;
			}
			h = Max( h, nodeLandmark - goalLandmark );
		}
	}

	return h;
}

/*
===============================================================================

	Queries

===============================================================================
*/

/*
================
rvNavMesh::RouteLowerBound
================
*/
float rvNavMesh::RouteLowerBound( int fromNode, int toNode ) const {
	if ( fromNode < 0 || fromNode >= nodes.Num() || toNode < 0 || toNode >= nodes.Num() ) {
		return NAV_INFINITE_COST;
	}
	if ( fromNode == toNode ) {
		return 0.0f;
	}
	if ( !NodeCanReach( fromNode, toNode ) ) {
		return NAV_INFINITE_COST;
	}
	return Heuristic( fromNode, toNode );
}

/*
================
rvNavMesh::FindNearestNode

Searches the grid cells around the point outwards, so the common case - the
point is standing on a node - costs one cell lookup.
================
*/
int rvNavMesh::FindNearestNode( const idVec3 &origin, float maxRadius, bool requireWalkable ) const {
	if ( nodes.Num() == 0 ) {
		return -1;
	}

	int gx, gy;
	CellCoords( origin, gx, gy );

	const int	maxRings	= (int)idMath::Ceil( maxRadius / cellSize );
	int			best		= -1;
	float		bestDistSqr = maxRadius * maxRadius;

	// Walkability probes are budgeted.  bestDistSqr only tightens on a SUCCESS,
	// so when nothing is walk-reachable the ring early-out below can never fire
	// and every node inside maxRadius gets a WalkableLine - which is two box
	// sweeps at minimum plus an interior SampleFloor per cell crossed.  That
	// failure path is not the rare one: the widest caller is the off-mesh
	// recovery search at 1536 units, and it runs precisely when the bot is
	// somewhere no node can reach, twice a second, for as long as it is stuck.
	// Rings are walked outwards, so the budget is spent nearest-first and the
	// answer in every success case is the one it would have found anyway.
	int probesLeft = requireWalkable ? NAV_MAX_WALKABLE_PROBES : nodes.Num();

	for ( int ring = 0; ring <= maxRings && probesLeft > 0; ring++ ) {
		for ( int dy = -ring; dy <= ring; dy++ ) {
			// Step across the shell of this ring rather than walking the whole
			// square and skipping the interior: the interior was covered by the
			// rings before it, and iterating it anyway turns an O(rings^2) job
			// into an O(rings^3) one.  That matters because the off-mesh
			// recovery search uses a large radius.
			const bool	edgeRow	= ( abs( dy ) == ring );
			const int	step	= ( edgeRow || ring == 0 ) ? 1 : ( ring * 2 );

			for ( int dx = -ring; dx <= ring; dx += step ) {
				const int key = nodeHash.GenerateKey( gx + dx, gy + dy );
				for ( int i = nodeHash.First( key ); i != -1; i = nodeHash.Next( i ) ) {
					if ( nodes[i].gx != gx + dx || nodes[i].gy != gy + dy ) {
						continue;
					}

					const float distSqr = ( nodes[i].origin - origin ).LengthSqr();
					if ( distSqr >= bestDistSqr ) {
						continue;
					}

					if ( requireWalkable ) {
						// A route never starts or ends on a pad or in a
						// teleporter: whoever stands there is already leaving.
						if ( nodes[i].flags & NAVNODE_TRANSPORT ) {
							continue;
						}
						if ( probesLeft <= 0 ) {
							continue;
						}
						probesLeft--;

						if ( !WalkableLine( origin, nodes[i].origin ) ) {
							continue;
						}
					}

					bestDistSqr	= distSqr;
					best		= i;
				}
			}
		}

		// A hit one ring in cannot be beaten by anything two rings out.
		if ( best != -1 && bestDistSqr <= ( ring * cellSize ) * ( ring * cellSize ) ) {
			break;
		}
	}

	return best;
}

/*
================
rvNavMesh::SnapNode
================
*/
int rvNavMesh::SnapNode( const idVec3 &point ) const {
	if ( nodes.Num() == 0 ) {
		return -1;
	}

	idVec3 routed = point;
	idVec3 projected;
	if ( GroundUnder( point, projected ) && point.z - projected.z > pm_stepsize.GetFloat() ) {
		routed = projected;
	}

	return FindNearestNode( routed, 256.0f, true );
}

/*
================
rvNavMesh::GroundUnder

Where the agent box comes down from this point: the surface under it, whether
or not a node could stand there.  Route endpoints in the air are projected
onto it before they snap.  SampleFloor demands standable ground, so a jump
over a stair edge, a ramp too steep to stand on or the lip of a ledge left
the endpoint where it was - forty units up - and no walk from there to any
node passes the slope bound, so the route failed for a bot or an enemy that
was merely mid-jump.
================
*/
bool rvNavMesh::GroundUnder( const idVec3 &point, idVec3 &ground ) const {
	trace_t trace;
	NavAgentTranslation( trace, point, point - idVec3( 0.0f, 0.0f, NAV_MAX_DROP ) );
	if ( trace.fraction >= 1.0f || trace.fraction <= 0.0f ) {
		return false;		// a bottomless drop, or a point already inside solid
	}

	ground = trace.endpos;
	return true;
}

/*
================
rvNavMesh::NodeForEntity
================
*/
int rvNavMesh::NodeForEntity( const idEntity *ent ) const {
	if ( !ent || nodes.Num() == 0 || !ent->GetPhysics() ||
		 ent->entityNumber < 0 || ent->entityNumber >= entitySnaps.Num() ) {
		return -1;
	}

	const int number = ent->entityNumber;
	const idVec3 &origin = ent->GetPhysics()->GetOrigin();
	navEntitySnap_t &snap = entitySnaps[number];

	if ( snap.spawnId == gameLocal.spawnIds[number] &&
		 ( snap.origin - origin ).LengthSqr() <= 1.0f ) {
		return snap.node;
	}

	snap.spawnId = gameLocal.spawnIds[number];
	snap.origin = origin;
	snap.node = SnapNode( origin );
	return snap.node;
}

/*
================
rvNavMesh::IsReachable
================
*/
bool rvNavMesh::IsReachable( const idVec3 &start, const idVec3 &goal ) const {
	if ( !NodeCanReach( SnapNode( start ), SnapNode( goal ) ) ) {
		return false;
	}

	rvNavPath path;
	return FindPath( start, goal, path );
}

/*
================
rvNavMesh::RandomReachablePoint

Candidates are drawn from the whole graph and filtered by the component
closure, which is exact and constant time, so a ledge or island bot still
finds the small set of nodes it can truly reach without walking every link of
the map first.  Each accepted draw is then proven by a real route, because
movers can make one branch temporarily unavailable.
================
*/
bool rvNavMesh::RandomReachablePoint( const idVec3 &origin, idVec3 &result, rvNavPath *pathOut ) const {
	if ( pathOut ) {
		pathOut->Clear();
	}

	if ( nodes.Num() < 2 ) {
		return false;
	}

	const int startNode = SnapNode( origin );
	if ( startNode == -1 ) {
		return false;
	}

	int attempts = 0;
	for ( int draw = 0; draw < 256 && attempts < NAV_MAX_ROUTE_REPAIRS; draw++ ) {
		// idRandom yields 15 bits; a large map has more nodes than that, and the
		// top of the node list would otherwise never be drawn.
		const unsigned int wide = ( (unsigned int)gameLocal.random.RandomInt() << 15 ) |
								  (unsigned int)gameLocal.random.RandomInt();
		const int candidate = (int)( wide % (unsigned int)nodes.Num() );
		if ( candidate == startNode || ( nodes[candidate].flags & NAVNODE_TRANSPORT ) ||
			 !NodeCanReach( startNode, candidate ) ) {
			continue;		// a pad or teleporter is never somewhere to stop
		}
		attempts++;

		rvNavPath candidatePath;
		if ( !FindPath( origin, nodes[candidate].origin, candidatePath ) ) {
			continue;
		}

		result = nodes[candidate].origin;
		if ( pathOut ) {
			*pathOut = candidatePath;
		}
		return true;
	}

	return false;
}

/*
================
rvNavMesh::BuildCostField
================
*/
int rvNavMesh::BuildCostField( const idVec3 &origin, float maxCost, const idList<int> *targets ) const {
	fieldSerial = 0;

	const int startNode = SnapNode( origin );
	if ( startNode == -1 ) {
		return -1;
	}

	const double startTicks = idLib::sys->GetClockTicks();

	BeginSearch();
	heap.SetNum( 0, false );

	// With a target list the search ends once every target is priced: a bot
	// whose useful pickups are all nearby does not pay for the far side of
	// the map.
	int remaining = 0;
	if ( targets ) {
		for ( int i = 0; i < targets->Num(); i++ ) {
			const int target = ( *targets )[i];
			if ( target >= 0 && target < nodes.Num() && fieldTargetTag[target] != searchSerial ) {
				fieldTargetTag[target] = searchSerial;
				remaining++;
			}
		}
	}

	gScore[startNode] = 0.0f;
	openTag[startNode] = searchSerial;
	HeapPush( startNode, 0.0f );

	while ( heap.Num() ) {
		const int current = HeapPop();
		if ( closedTag[current] == searchSerial ) {
			continue;
		}
		if ( gScore[current] > maxCost ) {
			break;
		}
		closedTag[current] = searchSerial;
		stats.fieldNodes++;

		if ( targets && ( remaining == 0 ||
			 ( fieldTargetTag[current] == searchSerial && --remaining == 0 ) ) ) {
			break;
		}

		for ( int l = nodes[current].firstLink; l != -1; l = links[l].next ) {
			const int next = links[l].node;
			const float tentative = gScore[current] + links[l].cost;
			if ( tentative > maxCost ) {
				continue;
			}
			if ( openTag[next] == searchSerial && tentative >= gScore[next] ) {
				continue;
			}

			openTag[next] = searchSerial;
			gScore[next] = tentative;
			HeapPush( next, tentative );
		}
	}

	fieldSerial = searchSerial;
	fieldPriced = false;
	stats.fieldSearches++;
	const double fieldTicks = idLib::sys->GetClockTicks() - startTicks;
	stats.searchTicks += fieldTicks;
	stats.fieldTicks += fieldTicks;

	return startNode;
}

/*
================
rvNavMesh::PathCost

Exact route cost between two nodes: A* under the landmark bound, without the
corner list, live link validation or string pulling that FindPath adds.  Goal
choice only needs to know what each route would cost; FindPath proves the
winner.  NAV_INFINITE_COST when no route exists within maxCost.
================
*/
float rvNavMesh::PathCost( int startNode, int goalNode, float maxCost ) const {
	if ( startNode < 0 || startNode >= nodes.Num() || goalNode < 0 || goalNode >= nodes.Num() ) {
		return NAV_INFINITE_COST;
	}
	if ( startNode == goalNode ) {
		return 0.0f;
	}
	if ( !NodeCanReach( startNode, goalNode ) ) {
		return NAV_INFINITE_COST;
	}

	const float startHeuristic = Heuristic( startNode, goalNode );
	if ( startHeuristic >= NAV_UNREACHED || startHeuristic > maxCost ) {
		return NAV_INFINITE_COST;
	}

	BeginSearch();
	heap.SetNum( 0, false );

	gScore[startNode]	= 0.0f;
	fScore[startNode]	= startHeuristic;
	openTag[startNode]	= searchSerial;
	HeapPush( startNode, startHeuristic );

	while ( heap.Num() ) {
		const int current = HeapPop();
		if ( closedTag[current] == searchSerial ) {
			continue;
		}
		closedTag[current] = searchSerial;
		stats.fieldNodes++;

		if ( current == goalNode ) {
			return gScore[current];
		}

		for ( int l = nodes[current].firstLink; l != -1; l = links[l].next ) {
			const int next = links[l].node;
			const float tentative = gScore[current] + links[l].cost;
			if ( openTag[next] == searchSerial && tentative >= gScore[next] ) {
				continue;
			}

			const float h = ( openTag[next] == searchSerial ) ?
				fScore[next] - gScore[next] : Heuristic( next, goalNode );
			if ( h >= NAV_UNREACHED || tentative + h > maxCost ) {
				continue;
			}

			openTag[next]	= searchSerial;
			gScore[next]	= tentative;
			fScore[next]	= tentative + h;
			HeapPush( next, fScore[next] );
		}
	}

	return NAV_INFINITE_COST;
}

/*
================
rvNavMesh::BuildCostField

Ranked form: branch and bound over the targets instead of a field.  The
landmark heuristic gives every target a lower bound on its route cost, so
weight / ( bound + startLeg + extra ) is the most it can score.  Targets are
priced exactly, best bound first, until the keepBest-th best exact score is
at least the bound of everything still unpriced - nothing left can then enter
the best few, and the caller's top few are exact.  A Dijkstra field that
proved the same thing settled a third of q4dm5 per decision and was four
fifths of all bot navigation time; a handful of landmark-guided searches
expand a few hundred nodes.
================
*/
int rvNavMesh::BuildCostField( const idVec3 &origin, float maxCost, const navFieldTarget_t *targets,
							   int numTargets, int keepBest ) const {
	static const int NAV_FIELD_MAX_KEEP = 8;
	// Search scratch reused across calls: routing is single threaded.
	static idList<int> order;
	static idList<float> bound;
	static idList<int> pricedNode;
	static idList<float> pricedCost;

	fieldSerial = 0;

	const int startNode = SnapNode( origin );
	if ( startNode == -1 ) {
		return -1;
	}

	const double startTicks = idLib::sys->GetClockTicks();

	keepBest = idMath::ClampInt( 1, NAV_FIELD_MAX_KEEP, keepBest );
	const float startLeg = ( nodes[startNode].origin - origin ).Length();

	order.SetNum( 0, false );
	bound.SetNum( Max( 0, numTargets ), false );
	pricedNode.SetNum( 0, false );
	pricedCost.SetNum( 0, false );
	bool ranked = true;
	for ( int i = 0; i < numTargets; i++ ) {
		const int node = targets[i].node;
		if ( node < 0 || node >= nodes.Num() ) {
			continue;		// nothing to price
		}
		if ( targets[i].weight <= 0.0f ) {
			ranked = false;
		}

		// A target the landmarks prove unreachable, or out of range, stays
		// unpriced without a search.
		const float lower = ( node == startNode ) ? 0.0f : Heuristic( startNode, node );
		if ( lower >= NAV_UNREACHED || lower > maxCost ) {
			continue;
		}
		bound[i] = ( targets[i].weight > 0.0f ) ?
			targets[i].weight / Max( 1.0f, lower + startLeg + Max( 0.0f, targets[i].extra ) ) : idMath::INFINITY;

		// Insertion by descending bound; target lists are at most a few hundred.
		int at = order.Num();
		order.Append( i );
		while ( at > 0 && bound[order[at - 1]] < bound[i] ) {
			order[at] = order[at - 1];
			at--;
		}
		order[at] = i;
	}

	float best[NAV_FIELD_MAX_KEEP];
	int numBest = 0;
	for ( int k = 0; k < order.Num(); k++ ) {
		const int i = order[k];
		if ( ranked && numBest >= keepBest && best[keepBest - 1] >= bound[i] ) {
			break;		// nothing still unpriced can enter the best few
		}

		const int node = targets[i].node;
		float cost = -1.0f;
		for ( int p = 0; p < pricedNode.Num(); p++ ) {
			if ( pricedNode[p] == node ) {
				cost = pricedCost[p];
				break;
			}
		}
		if ( cost < 0.0f ) {
			cost = PathCost( startNode, node, maxCost );
			pricedNode.Append( node );
			pricedCost.Append( cost );
		}
		if ( cost >= NAV_UNREACHED ) {
			continue;
		}

		// The least this target can be worth, so the stop rule never overrates
		// what is already priced.
		const float score = Min( targets[i].minWeight, targets[i].weight ) /
			Max( 1.0f, cost + startLeg + Max( 0.0f, targets[i].extra ) );
		int at = numBest;
		if ( numBest < keepBest ) {
			numBest++;
		} else if ( score <= best[keepBest - 1] ) {
			continue;
		} else {
			at = keepBest - 1;
		}
		while ( at > 0 && best[at - 1] < score ) {
			best[at] = best[at - 1];
			at--;
		}
		best[at] = score;
	}

	// Publish under a serial of its own: CostFieldDistance answers from this
	// table until the next search of any kind.
	BeginSearch();
	fieldSerial = searchSerial;
	fieldPriced = true;
	for ( int p = 0; p < pricedNode.Num(); p++ ) {
		fieldPricedTag[pricedNode[p]] = fieldSerial;
		fieldPricedCost[pricedNode[p]] = pricedCost[p];
	}

	stats.fieldSearches++;
	const double fieldTicks = idLib::sys->GetClockTicks() - startTicks;
	stats.searchTicks += fieldTicks;
	stats.fieldTicks += fieldTicks;

	return startNode;
}

/*
================
rvNavMesh::CostFieldDistance
================
*/
float rvNavMesh::CostFieldDistance( int node ) const {
	if ( fieldSerial == 0 || fieldSerial != searchSerial || node < 0 || node >= nodes.Num() ) {
		return idMath::INFINITY;
	}

	if ( fieldPriced ) {
		return ( fieldPricedTag[node] == fieldSerial && fieldPricedCost[node] < NAV_UNREACHED ) ?
			fieldPricedCost[node] : idMath::INFINITY;
	}

	return ( closedTag[node] == searchSerial ) ? gScore[node] : idMath::INFINITY;
}

/*
================
rvNavMesh::ResetStats
================
*/
void rvNavMesh::ResetStats( void ) const {
	memset( &stats, 0, sizeof( stats ) );
}

/*
================
NavSweepNearlyClear

A sweep stopped in its last couple of units arrived.  A player standing flush
against a wall is a legal endpoint, and the clip epsilon halts a box sweep a
hair short of it: a bot following a team-mate who stopped in a corner had
every route to them refused at 99%.
================
*/
static bool NavSweepNearlyClear( const trace_t &trace, const idVec3 &start, const idVec3 &end ) {
	return ( 1.0f - trace.fraction ) * ( end - start ).Length() <= NAV_SWEEP_SLACK;
}

/*
================
NavWalkFailure
================
*/
static bool NavWalkFailure( const char **failure, const char *reason ) {
	if ( failure ) {
		*failure = reason;
	}
	return false;
}

/*
================
rvNavMesh::WalkableLine

String-pull test.  A straight box sweep proves nothing is in the way, but it
would also happily cut the corner across a pit, so the ground underneath is
sampled as well.
================
*/
bool rvNavMesh::WalkableLine( const idVec3 &from, const idVec3 &to, const char **failure ) const {
	trace_t		trace;
	const float	stepSize = pm_stepsize.GetFloat();

	// A walk climbs or descends at most about a step per step: anything steeper
	// between the two points is another floor, however clear the straight sweep
	// between them happens to be.  Without this a bot under a balcony snapped
	// its route onto the balcony through the open air beside it.
	idVec3 flat = to - from;
	flat.z = 0.0f;
	if ( idMath::Fabs( to.z - from.z ) > stepSize + flat.Length() ) {
		return NavWalkFailure( failure, "steeper than a walk" );
	}

	if ( LineEntersTransport( from, to ) ) {
		return NavWalkFailure( failure, "steps onto a transport" );
	}

	idVec3 start = from + idVec3( 0.0f, 0.0f, NAV_GROUND_OFFSET );
	idVec3 end   = to   + idVec3( 0.0f, 0.0f, NAV_GROUND_OFFSET );
	float probeLift = NAV_GROUND_OFFSET;

	// Most shortcuts are ordinary floor movement.  Test at standing height
	// first so a legal low corridor is not rejected merely because a
	// step-height-raised player hull would overlap its ceiling.
	if ( !NavAgentTranslation( trace, start, end ) && !NavSweepNearlyClear( trace, start, end ) ) {
		const trace_t flat = trace;

		// A flat sweep can hit a stair that player physics will step over.  The
		// raised alternative is only needed in that case.
		start = from + idVec3( 0.0f, 0.0f, stepSize );
		end   = to   + idVec3( 0.0f, 0.0f, stepSize );
		probeLift = stepSize;

		if ( !NavAgentTranslation( trace, start, end ) && !NavSweepNearlyClear( trace, start, end ) ) {
			if ( failure ) {
				// Diagnostics only: what each sweep met, so a refused snap in a
				// bot_debug log can be traced to the brush or entity at fault.
				static char blocked[256];
				const idEntity *flatHit = ( flat.c.entityNum >= 0 && flat.c.entityNum < MAX_GENTITIES ) ?
					gameLocal.entities[flat.c.entityNum] : NULL;
				const idEntity *raisedHit = ( trace.c.entityNum >= 0 && trace.c.entityNum < MAX_GENTITIES ) ?
					gameLocal.entities[trace.c.entityNum] : NULL;
				idStr::snPrintf( blocked, sizeof( blocked ),
								 "blocked: flat %.2f by %s normal %s, raised %.2f by %s normal %s",
								 flat.fraction, flatHit ? flatHit->GetName() : "?", flat.c.normal.ToString( 2 ),
								 trace.fraction, raisedHit ? raisedHit->GetName() : "?", trace.c.normal.ToString( 2 ) );
				*failure = blocked;
			}
			return false;
		}
	}

	const idVec3	delta	= to - from;
	const float		length	= delta.Length();
	const int		samples	= ( length > idMath::FLOAT_EPSILON )
								? Max( 2, (int)idMath::Ceil( length / cellSize ) )
								: 1;

	for ( int i = 1; i < samples; i++ ) {
		const float	frac  = (float)i / (float)samples;
		const idVec3	expected = from + delta * frac;
		idVec3		probe = expected + idVec3( 0.0f, 0.0f, probeLift );
		idVec3		floor;

		if ( !SampleFloor( probe, probeLift + NAV_SMOOTH_MAX_DIP, floor ) ) {
			return NavWalkFailure( failure, "no floor under the line" );
		}

		// The straight line must stay close to the ground it crosses, or the
		// shortcut is really a fall.
		if ( expected.z - floor.z > NAV_SMOOTH_MAX_DIP ) {
			return NavWalkFailure( failure, "floats over a drop" );
		}

		// A dip the agent tolerates can still be a shallow kill pit or a lava
		// channel the graph itself refused; a shortcut must not reintroduce it.
		if ( IsLethalFloor( floor ) ) {
			return NavWalkFailure( failure, "crosses lethal floor" );
		}
	}

	return true;
}

/*
================
rvNavMesh::IsLethalFloor

The cheap form of IsHazardous for the inner loops: hazard volumes are a short
list of bounds, and the liquid point query only runs on a map where flood fill
actually met lava or slime.
================
*/
bool rvNavMesh::IsLethalFloor( const idVec3 &floor ) const {
	const int hazard = HazardAtFloor( floor );
	if ( hazard >= 0 && hazards[hazard].lethal ) {
		return true;
	}

	return lethalLiquidSeen && NavPointInLethalLiquid( floor );
}

/*
================
rvNavMesh::FindLink
================
*/
int rvNavMesh::FindLink( int fromNode, int toNode ) const {
	if ( fromNode < 0 || fromNode >= nodes.Num() ) {
		return -1;
	}

	for ( int l = nodes[fromNode].firstLink; l != -1; l = links[l].next ) {
		if ( links[l].node == toNode ) {
			return l;
		}
	}

	return -1;
}

/*
================
rvNavMesh::GatherDynamicSolids
================
*/
void rvNavMesh::GatherDynamicSolids( void ) {
	dynamicSolids.Clear();
	dynamicSolidSpawnIds.Clear();

	for ( idEntity *ent = gameLocal.spawnedEntities.Next(); ent != NULL; ent = ent->spawnNode.Next() ) {
		if ( !NavIsDynamicSolid( ent ) || !ent->GetPhysics() ||
			 !( ent->GetPhysics()->GetContents() & NAV_CLIP_MASK ) ) {
			continue;
		}
		dynamicSolids.Append( ent->entityNumber );
		dynamicSolidSpawnIds.Append( gameLocal.spawnIds[ent->entityNumber] );
	}
}

/*
================
rvNavMesh::DynamicSolidNear

Whether any recorded dynamic solid currently overlaps these bounds.  Most maps
have none at all, and most links are nowhere near the ones that exist, so this
spares nearly every route the collision sweeps of LinkTraversable.
================
*/
bool rvNavMesh::DynamicSolidNear( const idBounds &bounds ) const {
	for ( int i = 0; i < dynamicSolids.Num(); i++ ) {
		const int number = dynamicSolids[i];
		idEntity *ent = gameLocal.entities[number];
		if ( !ent || gameLocal.spawnIds[number] != dynamicSolidSpawnIds[i] || !ent->GetPhysics() ) {
			continue;
		}
		if ( ent->GetPhysics()->GetAbsBounds().IntersectsBounds( bounds ) ) {
			return true;
		}
	}
	return false;
}

/*
================
rvNavMesh::DynamicTranslation

The link-shape sweep against entities only, where only a dynamic solid that
is not a walk-activated door may block.  A static entity in the way was there
when generation proved the link through it, so it is passed and the sweep
repeated; a proof that would need more passes than the trace interface allows
is accepted, because everything left in the way was there at build time too.
================
*/
bool rvNavMesh::DynamicTranslation( trace_t &trace, const idVec3 &start, const idVec3 &end ) const {
	const idEntity *passEntity = NULL;
	const idEntity *passEntity2 = NULL;

	for ( int attempt = 0; attempt < 3; attempt++ ) {
		gameLocal.TranslationEntities( NULL, trace, start, end, navAgentModel, mat3_identity,
									  NAV_CLIP_MASK, passEntity, passEntity2 );
		if ( trace.fraction >= 1.0f ||
			 trace.c.entityNum < 0 || trace.c.entityNum >= MAX_GENTITIES ) {
			return true;
		}

		idEntity *blocker = gameLocal.entities[trace.c.entityNum];
		if ( !blocker ) {
			return true;
		}
		if ( NavIsDynamicSolid( blocker ) && !NavDoorIsWalkActivated( blocker ) ) {
			return false;
		}

		if ( !passEntity ) {
			passEntity = blocker;
		} else if ( !passEntity2 ) {
			passEntity2 = blocker;
		} else {
			return true;
		}
	}

	return true;
}

/*
================
rvNavMesh::LinkTraversable

Generation records the map's topology, but solid movers can change after the
mesh is built.  Validate only the links on a selected route (rather than every
edge A* examines) and let the caller exclude any stale link before searching
again.  Ordinary doors remain usable because approaching them activates them.
================
*/
bool rvNavMesh::LinkTraversable( int fromNode, int linkIndex ) const {
	if ( fromNode < 0 || fromNode >= nodes.Num() ||
		 linkIndex < 0 || linkIndex >= links.Num() ) {
		return false;
	}

	const navLink_t &link = links[linkIndex];
	if ( link.node < 0 || link.node >= nodes.Num() ) {
		return false;
	}

	switch ( link.travelType ) {
		case NAVTRAVEL_WALK:
		case NAVTRAVEL_DROP:
		case NAVTRAVEL_JUMP: {
			// The static world and traversal type were proven when this graph edge
			// was generated.  Only dynamic solid entities can make that proof stale.
			//
			// Reproduce the SHAPE the generator proved - lift, cross at that
			// height, then drop - instead of sweeping the straight chord between
			// the two node origins.  That chord is not a movement any player
			// makes, and for a drop or a jump it runs diagonally through the very
			// surface being left or landed on.  Harmless while the trace also
			// sees the world, because the world was proven; fatal here, because
			// this trace sees ONLY entities, so a ledge, crate or lift platform
			// that happens to be a brush model blocks its own link on every
			// single route and A* pays a full repair flood to route around ground
			// the bot can plainly stand on.
			const idVec3	fromFloor	= nodes[fromNode].origin;
			const idVec3	toFloor		= nodes[link.node].origin;
			const float		rise		= Max( 0.0f, toFloor.z - fromFloor.z );
			trace_t			trace;

			// Only something that moved since the build can invalidate the
			// proof, and nothing that moves is anywhere near most links.
			if ( dynamicSolids.Num() == 0 ) {
				return true;
			}
			idBounds swept;
			swept.Clear();
			swept.AddPoint( fromFloor );
			swept.AddPoint( toFloor );
			const idBounds &agent = GetAgentBounds();
			swept[0] += agent[0];
			swept[1] += agent[1];
			swept[1].z += pm_jumpheight.GetFloat() + NAV_GROUND_OFFSET;
			if ( !DynamicSolidNear( swept ) ) {
				return true;
			}

			idVec3 start = fromFloor;
			start.z += NAV_GROUND_OFFSET;

			// A jump commits to the full height whatever the rise; a step only
			// lifts as far as it has to, so a flat walk is not failed by a low
			// ceiling it passes under perfectly well.
			const float lift = ( link.travelType == NAVTRAVEL_JUMP ) ?
				pm_jumpheight.GetFloat() : Min( rise, pm_stepsize.GetFloat() );

			if ( lift > 0.0f ) {
				idVec3 lifted = start;
				lifted.z = fromFloor.z + NAV_GROUND_OFFSET + lift;

				if ( !DynamicTranslation( trace, start, lifted ) ) {
					return false;
				}
				start = lifted;
			}

			idVec3 across = toFloor;
			across.z = start.z;

			if ( !DynamicTranslation( trace, start, across ) ) {
				return false;
			}

			// Then down onto the destination floor.  This is the whole fall for a
			// drop, and nothing at all when the crossing already ended level.
			idVec3 down = across;
			down.z = toFloor.z + NAV_GROUND_OFFSET;

			if ( down.z >= across.z ) {
				return true;
			}

			return DynamicTranslation( trace, across, down );
		}

		case NAVTRAVEL_JUMPPAD:
		case NAVTRAVEL_TELEPORT: {
			if ( link.actionEntityNum < 0 || link.actionEntityNum >= MAX_GENTITIES ) {
				return false;
			}

			idEntity *action = gameLocal.entities[link.actionEntityNum];
			if ( !action || action->targets.Num() < 1 || !action->targets[0].GetEntity() ) {
				return false;
			}

			if ( link.travelType == NAVTRAVEL_JUMPPAD ) {
				if ( !action->IsType( rvJumpPad::GetClassType() ) ||
					 !action->IsActive( TH_THINK ) ) {
					return false;
				}
			} else if ( !action->IsType( idTrigger_Multi::GetClassType() ) ||
						!action->targets[0].GetEntity()->IsType( idPlayerStart::GetClassType() ) ) {
				return false;
			}

			idBounds activationBounds;
			return NavTravelVolumeBounds( action, activationBounds );
		}

		default:
			return false;
	}
}

/*
================
rvNavMesh::LinkTravelType

How a bot has to get from one node to the next.  Nodes that are not directly
linked - the result of a string pull - are walked.
================
*/
int rvNavMesh::LinkTravelType( int fromNode, int toNode ) const {
	const int link = FindLink( fromNode, toNode );
	return ( link != -1 ) ? links[link].travelType : NAVTRAVEL_WALK;
}

/*
================
rvNavMesh::LinkActionEntity

Volume traversal has to retain the pad or trigger that owns the graph edge.
The bot uses its live bounds to enter the volume before steering toward the
remote exit.
================
*/
int rvNavMesh::LinkActionEntity( int fromNode, int toNode ) const {
	const int link = FindLink( fromNode, toNode );
	return ( link != -1 ) ? links[link].actionEntityNum : -1;
}

/*
================
rvNavMesh::StringPull

Turns the cell-by-cell node chain into the small number of corners a bot
actually has to steer through.  Only walk links are ever skipped: a jump pad or
a teleporter has to be entered deliberately, so those corners always survive.
================
*/
void rvNavMesh::StringPull( const idList<int> &nodePath, const idVec3 &goal, rvNavPath &path ) const {
	int i = 0;

	path.Clear();

	while ( i < nodePath.Num() - 1 ) {
		// How far ahead smoothing is even allowed to look: never past a link
		// that has to be travelled deliberately.
		int limit = Min( i + NAV_SMOOTH_LOOKAHEAD, nodePath.Num() - 1 );
		int immediateTravel = NAVTRAVEL_WALK;

		for ( int j = i + 1; j <= limit; j++ ) {
			const int travelType = LinkTravelType( nodePath[j - 1], nodePath[j] );
			if ( travelType != NAVTRAVEL_WALK ) {
				limit = j - 1;
				if ( j == i + 1 ) {
					immediateTravel = travelType;
				}
				break;
			}
		}

		// A mature reachability carries both its entry and exit.  If the route
		// begins with an action (or resumes immediately after one), preserve the
		// source explicitly before its destination.  The bot can then align
		// with a jump takeoff or enter a pad/teleporter volume instead of aiming
		// directly at a remote endpoint.
		if ( immediateTravel != NAVTRAVEL_WALK ) {
			const idVec3 &source = nodes[nodePath[i]].origin;

			// A running jump across a gap needs the bot moving at speed along
			// the jump line when it leaves the ground, not arriving at the edge
			// from the side.  Put a run-up corner behind the takeoff when both
			// legs to it are plain floor.
			if ( immediateTravel == NAVTRAVEL_JUMP ) {
				idVec3 across = nodes[nodePath[i + 1]].origin - source;
				across.z = 0.0f;
				if ( across.Normalize() > cellSize * 1.6f ) {
					const idVec3 runup = source - across * NAV_GAP_RUNUP;
					const idVec3 &before = ( path.Num() > 0 ) ? path[path.Num() - 1].origin : nodes[nodePath[0]].origin;
					if ( ( before - runup ).LengthSqr() > 1.0f &&
						 WalkableLine( before, runup ) && WalkableLine( runup, source ) ) {
						path.Append( runup, NAVTRAVEL_WALK );
					}
				}
			}

			if ( path.Num() == 0 ||
				 ( path[path.Num() - 1].origin - source ).LengthSqr() > 1.0f ) {
				path.Append( source, NAVTRAVEL_WALK );
			}

			path.Append( nodes[nodePath[i + 1]].origin, immediateTravel,
						 LinkActionEntity( nodePath[i], nodePath[i + 1] ) );
			i++;
			continue;
		}

		// Walk forward while the shortcut still holds.  Stopping at the first
		// failure keeps this to a handful of traces per corner.
		int furthest = i + 1;
		for ( int j = i + 2; j <= limit; j++ ) {
			if ( !WalkableLine( nodes[nodePath[i]].origin, nodes[nodePath[j]].origin ) ) {
				break;
			}
			furthest = j;
		}

		path.Append( nodes[nodePath[furthest]].origin, LinkTravelType( nodePath[furthest - 1], nodePath[furthest] ) );
		i = furthest;
	}

	// Finish at the caller's actual goal rather than the node under it, but
	// only when walking straight there is safe.  The old empty-path shortcut
	// skipped this proof when start and goal snapped to the same node, which
	// could invent a walk through a wall or between stacked floors.
	const idVec3 &lastOrigin = ( path.Num() > 0 )
		? path[path.Num() - 1].origin
		: nodes[nodePath[0]].origin;

	if ( WalkableLine( lastOrigin, goal ) ) {
		path.Append( goal, NAVTRAVEL_WALK );
	}
}

/*
================
rvNavMesh::BeginSearch

The serial-tagged scratch arrays avoid clearing tens of thousands of entries
for every query.  Reset them explicitly before the signed serial can wrap;
otherwise a very long-running server could inherit stale open/closed tags.
================
*/
void rvNavMesh::BeginSearch( void ) const {
	if ( searchSerial >= 0x7ffffffe ) {
		for ( int i = 0; i < openTag.Num(); i++ ) {
			openTag[i] = 0;
			closedTag[i] = 0;
		}
		for ( int i = 0; i < fieldTargetTag.Num(); i++ ) {
			fieldTargetTag[i] = 0;
			fieldPricedTag[i] = 0;
		}
		searchSerial = 1;
	} else {
		searchSerial++;
	}
}

/*
================
rvNavMesh::HeapPush
================
*/
void rvNavMesh::HeapPush( int node, float key ) const {
	navHeapEntry_t entry;

	entry.node	= node;
	entry.key	= key;

	int child = heap.Append( entry );

	while ( child > 0 ) {
		const int parent = ( child - 1 ) >> 1;

		if ( heap[parent].key <= heap[child].key ) {
			break;
		}

		const navHeapEntry_t swap = heap[parent];
		heap[parent] = heap[child];
		heap[child]  = swap;
		child = parent;
	}
}

/*
================
rvNavMesh::HeapPop
================
*/
int rvNavMesh::HeapPop( void ) const {
	const int top = heap[0].node;

	heap[0] = heap[heap.Num() - 1];
	heap.SetNum( heap.Num() - 1, false );

	int parent = 0;
	while ( true ) {
		const int left  = parent * 2 + 1;
		const int right = left + 1;
		int		  best  = parent;

		if ( left < heap.Num() && heap[left].key < heap[best].key ) {
			best = left;
		}
		if ( right < heap.Num() && heap[right].key < heap[best].key ) {
			best = right;
		}
		if ( best == parent ) {
			break;
		}

		const navHeapEntry_t swap = heap[parent];
		heap[parent] = heap[best];
		heap[best]   = swap;
		parent = best;
	}

	return top;
}

/*
================
rvNavMesh::FindPath

A* uses Euclidean distance scaled by the cheapest cost-to-distance ratio in the
graph.  This remains admissible and consistent for jump pads and teleporters,
without making every search on those maps a wide Dijkstra pass.

The generated graph describes topology, not a promise that every mover remains
where it was at build time.  After A* selects a route, only that small set of
edges is checked against live collision.  Blocked edges are excluded and A* is
retried, allowing a bot to choose another corridor without tracing every link
the search merely considered.
================
*/
bool rvNavMesh::FindPath( const idVec3 &start, const idVec3 &goal, rvNavPath &path ) const {
	path.Clear();

	if ( nodes.Num() == 0 ) {
		return false;
	}

	const double startTicks = idLib::sys->GetClockTicks();
	stats.pathSearches++;
	const bool found = FindPathInternal( start, goal, path );
	if ( !found ) {
		stats.pathFailures++;
	}
	stats.searchTicks += idLib::sys->GetClockTicks() - startTicks;

	return found;
}

/*
================
rvNavMesh::FindPathInternal
================
*/
bool rvNavMesh::FindPathInternal( const idVec3 &start, const idVec3 &goal, rvNavPath &path ) const {
	idVec3 routeStart = start;
	idVec3 routeGoal = goal;

	// A player can legitimately request a route while airborne.  Preserve the
	// strict endpoint proof for grounded items and objectives, but route an
	// endpoint that is clearly more than one step above standable ground from or
	// to that landing point.  A grounded endpoint starts its hull sweep touching
	// the floor and therefore cannot project through it onto a stacked level.
	idVec3 projected;
	if ( GroundUnder( start, projected ) && start.z - projected.z > pm_stepsize.GetFloat() ) {
		routeStart = projected;
	}
	if ( GroundUnder( goal, projected ) && goal.z - projected.z > pm_stepsize.GetFloat() ) {
		routeGoal = projected;
	}

	const int startNode = FindNearestNode( routeStart, 256.0f, true );
	const int goalNode  = FindNearestNode( routeGoal, 256.0f, true );

	if ( startNode == -1 || goalNode == -1 ) {
		stats.snapFailures++;
		if ( bot_debug.GetInteger() >= 1 && gameLocal.time >= nextSnapReportTime ) {
			// Say why: the nearest node by distance alone, and the first test
			// the walk onto it failed.
			nextSnapReportTime = gameLocal.time + 1000;
			const idVec3 &point = ( startNode == -1 ) ? routeStart : routeGoal;
			const int nearest = FindNearestNode( point, 256.0f, false );
			const char *failure = "no node within 256";
			if ( nearest != -1 ) {
				failure = ( nodes[nearest].flags & NAVNODE_TRANSPORT ) ? "nearest is a transport" : "walkable";
				WalkableLine( point, nodes[nearest].origin, &failure );
			}
			gameLocal.Printf( "rvNavMesh: no node near route %s %s: %s%s%s\n",
							  ( startNode == -1 ) ? "start" : "goal", point.ToString( 0 ), failure,
							  ( nearest != -1 ) ? " to " : "",
							  ( nearest != -1 ) ? nodes[nearest].origin.ToString( 0 ) : "" );
		}
		return false;
	}
	// Exact directed reachability, in constant time.  An unreachable goal used
	// to cost a search that settled everything the start could reach.
	if ( !NodeCanReach( startNode, goalNode ) ) {
		stats.quickRejects++;
		return false;
	}

	if ( startNode == goalNode ) {
		// Same cell, but "same cell" is not the same as "nothing in the way" -
		// a pillar can sit between the two exact points.  Both endpoints snapped
		// to this node through individually walkable legs, so use the node as a
		// real corner when the direct shortcut is blocked.  Recheck the outbound
		// direction because WalkableLine's floor sampling is directional.
		if ( !WalkableLine( routeStart, routeGoal ) ) {
			const idVec3 &via = nodes[startNode].origin;
			if ( !WalkableLine( routeStart, via ) || !WalkableLine( via, routeGoal ) ) {
				return false;
			}

			if ( ( via - routeStart ).LengthSqr() > 1.0f &&
				 ( via - routeGoal ).LengthSqr() > 1.0f ) {
				path.Append( via, NAVTRAVEL_WALK );
			}
		}

		if ( path.Num() == 0 ||
			 ( path[path.Num() - 1].origin - routeGoal ).LengthSqr() > 1.0f ) {
			path.Append( routeGoal, NAVTRAVEL_WALK );
		}

		const float exactCost = path.LengthFrom( start );
		path.SetTravelCost( start, exactCost );
		path.routeStart = routeStart;
		return path.Num() > 0;
	}

	idList<int> rejectedLinks;
	rejectedLinks.SetGranularity( 16 );

	const float startHeuristic = Heuristic( startNode, goalNode );
	if ( startHeuristic >= NAV_UNREACHED ) {
		stats.quickRejects++;
		return false;
	}

	for ( int repair = 0; repair <= NAV_MAX_ROUTE_REPAIRS; repair++ ) {
		if ( repair > 0 ) {
			stats.repairSearches++;
		}

		BeginSearch();
		heap.SetNum( 0, false );

		gScore[startNode]	= 0.0f;
		fScore[startNode]	= startHeuristic;
		cameFrom[startNode]	= -1;
		openTag[startNode]	= searchSerial;

		HeapPush( startNode, fScore[startNode] );

		bool found = false;
		while ( heap.Num() ) {
			const int current = HeapPop();

			// A stale copy of a node that has already been expanded.
			if ( closedTag[current] == searchSerial ) {
				continue;
			}
			closedTag[current] = searchSerial;
			stats.nodesExpanded++;

			if ( current == goalNode ) {
				found = true;
				break;
			}

			for ( int l = nodes[current].firstLink; l != -1; l = links[l].next ) {
				bool rejected = false;
				for ( int r = 0; r < rejectedLinks.Num(); r++ ) {
					if ( rejectedLinks[r] == l ) {
						rejected = true;
						break;
					}
				}
				if ( rejected ) {
					continue;
				}

				const int next = links[l].node;
				const float tentative = gScore[current] + links[l].cost;

				if ( openTag[next] == searchSerial && tentative >= gScore[next] ) {
					continue;
				}

				// A node first seen this search gets its bound computed once;
				// a cheaper path to an open node reuses it.
				const float h = ( openTag[next] == searchSerial ) ?
					fScore[next] - gScore[next] : Heuristic( next, goalNode );
				if ( h >= NAV_UNREACHED ) {
					continue;	// proven unable to lead to the goal
				}

				openTag[next]	= searchSerial;
				gScore[next]	= tentative;
				fScore[next]	= tentative + h;
				cameFrom[next]	= current;

				HeapPush( next, fScore[next] );
			}
		}

		if ( !found ) {
			stats.searchFailures++;
			path.Clear();
			return false;
		}

		// Unwind, then reverse.
		idList<int> nodePath;
		nodePath.SetGranularity( 64 );

		for ( int node = goalNode; node != -1; node = cameFrom[node] ) {
			nodePath.Append( node );
		}

		for ( int i = 0, j = nodePath.Num() - 1; i < j; i++, j-- ) {
			const int swap = nodePath[i];
			nodePath[i] = nodePath[j];
			nodePath[j] = swap;
		}

		// Recheck only the selected route against the current collision world.
		// Add every stale edge found in this pass, so wide blockers usually need
		// one repair search rather than one search per blocked cell.
		bool routeValid = true;
		for ( int i = 0; i + 1 < nodePath.Num(); i++ ) {
			const int link = FindLink( nodePath[i], nodePath[i + 1] );
			if ( link == -1 ) {
				path.Clear();
				return false;
			}
			if ( LinkTraversable( nodePath[i], link ) ) {
				continue;
			}

			rejectedLinks.Append( link );
			routeValid = false;
		}

		if ( !routeValid ) {
			continue;
		}

		const float exactTravelCost =
			( routeStart - start ).Length() +
			( nodes[startNode].origin - routeStart ).Length() +
			gScore[goalNode] +
			( routeGoal - nodes[goalNode].origin ).Length();

		StringPull( nodePath, routeGoal, path );

		// The routed start (the exact origin or an airborne landing projection) was
		// proven against startNode when it was selected, but a later string pull can
		// replace that first node with a farther corner.  Two individually clear legs
		// around a wall do not prove their diagonal.  Put the start node back when
		// that final shortcut is not itself clear.
		if ( path.Num() > 0 && !WalkableLine( routeStart, path[0].origin ) ) {
			rvNavPath safeStartPath;
			safeStartPath.Append( nodes[startNode].origin, NAVTRAVEL_WALK );

			for ( int i = 0; i < path.Num(); i++ ) {
				safeStartPath.Append( path[i].origin, path[i].travelType,
									  path[i].actionEntityNum );
			}

			path = safeStartPath;
		}

		// StringPull deliberately refuses an unsafe final shortcut from the last
		// sampled node to the exact goal or projected landing point.  That refusal is
		// a failed route, not a useful partial path: handing it to goal selection
		// makes the bot arrive at the wall or wrong floor and immediately choose the
		// same target again.
		// The one exception is a goal standing in the reach of a pad or a
		// teleporter - an item beside one, an enemy at its edge.  Nobody can
		// stand exactly there without being carried off, so the node nearest it
		// is as close as any route goes, and arriving there is arriving.
		if ( path.Num() > 0 &&
			 ( path[path.Num() - 1].origin - routeGoal ).LengthSqr() > 1.0f &&
			 ( path[path.Num() - 1].origin - routeGoal ).LengthSqr() <= Square( NAV_TRANSPORT_GOAL_REACH ) ) {
			idBounds standing = GetAgentBounds();
			standing.TranslateSelf( routeGoal );
			if ( TransportAt( standing ) >= 0 ) {
				path.SetTravelCost( start, exactTravelCost );
				path.routeStart = routeStart;
				return true;
			}
		}

		if ( path.Num() == 0 ||
			 ( path[path.Num() - 1].origin - routeGoal ).LengthSqr() > 1.0f ) {
			stats.legFailures++;
			if ( bot_debug.GetInteger() >= 1 && gameLocal.time >= nextSnapReportTime ) {
				nextSnapReportTime = gameLocal.time + 1000;
				const idVec3 &last = ( path.Num() > 0 ) ? path[path.Num() - 1].origin : nodes[goalNode].origin;
				const char *failure = "walkable";
				WalkableLine( last, routeGoal, &failure );
				gameLocal.Printf( "rvNavMesh: final leg %s to %s refused: %s\n",
								  last.ToString( 0 ), routeGoal.ToString( 0 ), failure );
			}
			path.Clear();
			return false;
		}

		path.SetTravelCost( start, exactTravelCost );
		path.routeStart = routeStart;
		return true;
	}

	stats.searchFailures++;
	path.Clear();
	return false;
}

/*
================
rvNavMesh::DebugDraw
================
*/
void rvNavMesh::DebugDraw( const idVec3 &viewOrigin, float radius ) const {
	static const idVec4 *travelColors[NAVTRAVEL_NUM] = {
		&colorGreen,		// walk
		&colorYellow,		// drop
		&colorOrange,		// jump
		&colorMagenta,		// jump pad
		&colorCyan			// teleport
	};

	// The renderer keeps a fixed debug line pool (MAX_DEBUG_LINES, 16384) and
	// silently drops anything past it, so a dense mesh drawn without a budget
	// fills the pool with whichever nodes happened to come first and shows
	// nothing useful.  Spend the budget near the viewer instead.
	static const int NAV_DEBUG_MAX_LINES = 8192;

	const float	radiusSqr	= radius * radius;
	int			budget		= NAV_DEBUG_MAX_LINES;

	for ( int i = 0; i < nodes.Num() && budget > 0; i++ ) {
		if ( ( nodes[i].origin - viewOrigin ).LengthSqr() > radiusSqr ) {
			continue;
		}

		gameRenderWorld->DebugLine( colorBlue, nodes[i].origin, nodes[i].origin + idVec3( 0.0f, 0.0f, 8.0f ), 0 );
		budget--;

		for ( int l = nodes[i].firstLink; l != -1 && budget > 0; l = links[l].next ) {
			const int type = idMath::ClampInt( 0, NAVTRAVEL_NUM - 1, links[l].travelType );

			// One direction only; the reverse link would draw the same segment.
			if ( links[l].node < i && links[l].travelType == NAVTRAVEL_WALK ) {
				continue;
			}

			gameRenderWorld->DebugLine( *travelColors[type],
										nodes[i].origin + idVec3( 0.0f, 0.0f, 4.0f ),
										nodes[links[l].node].origin + idVec3( 0.0f, 0.0f, 4.0f ),
										0 );
			budget--;
		}
	}
}
