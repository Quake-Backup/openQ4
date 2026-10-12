// Actual navigation functions are inserted by mp_bot_jump_trajectories.py.
// This small collision world sweeps the configured player hull against
// axis-aligned solids. Cylinder vertices and route proofs are production code;
// collision is an independent continuous separating-axis test.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>
#undef INFINITY
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
template<class T> T Min(T a, T b) { return std::min(a,b); }
template<class T> T Max(T a, T b) { return std::max(a,b); }
float Square(float x) { return x*x; }
struct idMath {
    static constexpr float INFINITY=std::numeric_limits<float>::infinity();
    static constexpr float PI=3.14159265358979323846f,TWO_PI=2*PI;
    static int Abs(int x) { return std::abs(x); }
    static float Sin(float x) { return std::sin(x); }
    static float Cos(float x) { return std::cos(x); }
    static float Fabs(float x) { return std::fabs(x); }
    static float Sqrt(float x) { return std::sqrt(x); }
    static float Ceil(float x) { return std::ceil(x); }
};
struct idVec3 {
    float x=0,y=0,z=0;
    idVec3() = default;
    idVec3(float a,float b,float c):x(a),y(b),z(c) {}
    void Set(float a,float b,float c) { x=a; y=b; z=c; }
    idVec3 operator+(const idVec3& b) const { return {x+b.x,y+b.y,z+b.z}; }
    idVec3 operator-(const idVec3& b) const { return {x-b.x,y-b.y,z-b.z}; }
    idVec3 operator*(float f) const { return {x*f,y*f,z*f}; }
    idVec3 operator/(float f) const { return *this*(1/f); }
    idVec3& operator+=(const idVec3& b) { *this=*this+b; return *this; }
    idVec3& operator-=(const idVec3& b) { *this=*this-b; return *this; }
    idVec3& operator*=(float f) { *this=*this*f; return *this; }
    float operator*(const idVec3& b) const { return x*b.x+y*b.y+z*b.z; }
    idVec3 Cross(const idVec3& b) const { return {y*b.z-z*b.y,z*b.x-x*b.z,x*b.y-y*b.x}; }
    float LengthSqr() const { return *this * *this; }
    float Length() const { return std::sqrt(LengthSqr()); }
    float Normalize() { float n=Length(); if(n>0) *this=*this*(1/n); return n; }
    float& operator[](int i) { return i==0?x:i==1?y:z; }
    float operator[](int i) const { return i==0?x:i==1?y:z; }
};
idVec3 operator*(float f,const idVec3& v) { return v*f; }
struct idBounds {
    idVec3 points[2];
    idBounds()=default;
    idBounds(idVec3 a,idVec3 b) { points[0]=a; points[1]=b; }
    idBounds(std::initializer_list<idVec3> p) { std::copy(p.begin(),p.end(),points); }
    idVec3& operator[](int i) { return points[i]; }
    const idVec3& operator[](int i) const { return points[i]; }
    void Clear() { points[0]={1e30f,1e30f,1e30f}; points[1]={-1e30f,-1e30f,-1e30f}; }
    void AddPoint(const idVec3& p) {
        for(int i=0;i<3;i++) { points[0][i]=Min(points[0][i],p[i]); points[1][i]=Max(points[1][i],p[i]); }
    }
    void TranslateSelf(const idVec3& p) { points[0]=points[0]+p; points[1]=points[1]+p; }
};
struct CVar { float value; float GetFloat() const { return value; } int GetInteger() const { return int(value); } };
CVar pm_stepsize{16},pm_jumpheight{48},pm_speed{320},pm_usecylinder{1};
constexpr int MAX_GENTITIES=64, SURF_NODAMAGE=1, MASK_WATER=2;
struct idEntity {
    struct Physics { idVec3 origin; const idVec3& GetOrigin() const { return origin; } } physics;
    struct Target { idEntity* entity=nullptr; idEntity* GetEntity() const { return entity; } };
    struct Targets : std::vector<Target> { int Num() const { return int(size()); } } targets;
    Physics* GetPhysics() { return &physics; }
};
struct GameLocal {
    idEntity* entities[MAX_GENTITIES]={};
    float waterTop=-1e30f;
    idVec3 GetGravity() const { return {0,0,-800}; }
    idVec3 GetCurrentGravity(idEntity*) const { return GetGravity(); }
    int LiquidContentsAtPoint(const idVec3& p,void*) const { return p.z<=waterTop ? MASK_WATER : 0; }
} gameLocal;
struct Material { int flags=0; int GetSurfaceFlags() const { return flags; } } floorMaterial;
struct trace_t { float fraction=1; idVec3 endpos; struct { idVec3 normal; const Material* material=nullptr; } c; };
enum { NAVTRAVEL_WALK, NAVTRAVEL_DROP, NAVTRAVEL_JUMP, NAVTRAVEL_JUMPPAD, NAVTRAVEL_TELEPORT };
struct navCorner_t { idVec3 origin; int travelType=NAVTRAVEL_WALK,actionEntityNum=-1; };
// PRODUCTION_CONSTANTS

// The exact cylinder builder is inserted below. Edge normals are unused by
// this fixture's separating-axis collision test; face normals and vertices
// are built by the engine function itself.
enum { TRM_BOX,TRM_CYLINDER };
constexpr int MAX_TRACEMODEL_VERTS=32,MAX_TRACEMODEL_EDGES=32,MAX_TRACEMODEL_POLYS=16;
struct Common { void Printf(const char*) {} } common;
namespace idLib { Common* common=&::common; }
struct idTraceModel {
    int type=TRM_BOX,numVerts=0,numEdges=0,numPolys=0;
    idVec3 verts[32],offset;
    struct { int v[2]; } edges[33];
    struct Poly { int numEdges=0,edges[16]; idVec3 normal; float dist=0; idBounds bounds; } polys[16];
    idBounds bounds;
    bool isConvex=true;
    idTraceModel()=default;
    explicit idTraceModel(const idBounds& b):numVerts(8),bounds(b) {
        for(int i=0;i<8;i++) verts[i]={b[(i>>0)&1].x,b[(i>>1)&1].y,b[(i>>2)&1].z};
    }
    idTraceModel(const idBounds& b,int sides,bool alternate) { SetupCylinder(b,sides,alternate); }
    void SetupCylinder(const idBounds&,int,bool);
    void GenerateEdgeNormals() {}
};
using traceModelPoly_t=idTraceModel::Poly;
// PRODUCTION_HULL
idBounds agentBounds{{{-16,-16,0},{16,16,77}}};
idTraceModel agentHull;
idTraceModel& navAgentTrace=agentHull;
idBounds navSupportBounds{{{-NAV_LANDING_SUPPORT,-NAV_LANDING_SUPPORT,0},
    {NAV_LANDING_SUPPORT,NAV_LANDING_SUPPORT,NAV_LANDING_SUPPORT}}};
idTraceModel navSupportTrace(navSupportBounds);

struct Box { idVec3 lo,hi; };
std::vector<Box> solids;
bool lethal=false;
bool slopedFloor=false;
bool transported=false;

bool Sweep(trace_t& trace,const idVec3& start,const idVec3& end,const idTraceModel& model) {
    trace={}; trace.endpos=end; trace.c.material=&floorMaterial;
    const idVec3 delta=end-start;
    std::vector<idVec3> axes{{1,0,0},{0,1,0},{0,0,1}};
    if (model.type==TRM_CYLINDER) {
        for(int i=0;i<model.numPolys-2;i++) axes.push_back(model.polys[i].normal);
    }
    for (const Box& b:solids) {
        float enter=0,leave=1; idVec3 normal; bool missed=false;
        for (const idVec3& axis:axes) {
            float shapeLo=1e30f,shapeHi=-1e30f,boxLo=0,boxHi=0;
            for(int i=0;i<model.numVerts;i++) {
                shapeLo=Min(shapeLo,model.verts[i]*axis); shapeHi=Max(shapeHi,model.verts[i]*axis);
            }
            for(int i=0;i<3;i++) {
                boxLo+=Min(b.lo[i]*axis[i],b.hi[i]*axis[i]);
                boxHi+=Max(b.lo[i]*axis[i],b.hi[i]*axis[i]);
            }
            const float lo=boxLo-shapeHi,hi=boxHi-shapeLo,origin=start*axis,speed=delta*axis;
            if (std::fabs(speed)<1e-6f) {
                if (origin<=lo || origin>=hi) { missed=true; break; }
                continue;
            }
            float a=(lo-origin)/speed,z=(hi-origin)/speed;
            float sign=-1;
            if (a>z) { std::swap(a,z); sign=1; }
            if (a>enter) { enter=a; normal=axis*sign; }
            leave=std::min(leave,z);
            if (enter>leave) { missed=true; break; }
        }
        if (!missed && leave>0 && enter<trace.fraction) {
            trace.fraction=enter; trace.endpos=start+delta*enter; trace.c.normal=normal;
        }
    }
    if (slopedFloor) {
        // Infinite 45-degree walkable plane z=x, with true hull support.
        idVec3 normal(-1,0,1); normal.Normalize();
        float support=1e30f;
        for(int i=0;i<model.numVerts;i++) support=Min(support,model.verts[i]*normal);
        const float from=start*normal+support,to=end*normal+support;
        if (from<0 || to<0) {
            const float fraction=from<=0 ? 0 : from/(from-to);
            if(fraction<trace.fraction) {
                trace.fraction=fraction; trace.endpos=start+delta*fraction; trace.c.normal=normal;
            }
        }
    }
    return trace.fraction>=1;
}
bool NavAgentTranslation(trace_t& trace,const idVec3& start,const idVec3& end) {
    return Sweep(trace,start,end,agentHull);
}
bool NavTraceTranslation(trace_t& trace,const idVec3& start,const idVec3& end,
    const idTraceModel& model,const idBounds&) {
    return Sweep(trace,start,end,model);
}
class rvNavMesh {
public:
    bool SampleFloor(const idVec3&,float,idVec3&) const;
    bool TryStep(const idVec3&,const idVec3&,idVec3&,int&) const;
    bool RunningDropClear(const idVec3&,const idVec3&) const;
    bool LandingContinuationClear(const idVec3&,const idVec3&) const;
    bool RecoveryStepClear(const idVec3&,const idVec3&) const;
    bool GapJumpClear(const idVec3&,const idVec3&,float,float,float,bool=false,const idVec3* = nullptr,float=0,const idVec3* = nullptr,float=0) const;
    bool JumpTakeoffClear(const idVec3&,const idVec3&,const idVec3&,float,float,const idVec3* = nullptr,float=0) const;
    bool DodgeJumpClear(const idVec3&,const idVec3&,const idVec3&,float,float=0) const;
    bool PadLanding(idEntity*,const idVec3&,const idVec3*,idVec3&,float* = nullptr,float* = nullptr) const;
    bool LandingSurvivable(const idVec3&,const navCorner_t&,float) const;
    bool IsSteeredPadLanding(int,const idVec3&) const { return false; }
    static const idBounds& GetAgentBounds() { return agentBounds; }
    int TransportAt(const idBounds&) const { return transported ? 1 : -1; }
    bool LineEntersTransport(const idVec3&,const idVec3&) const { return transported; }
    bool IsLethalFloor(const idVec3&) const { return lethal; }
    bool WalkableLine(const idVec3& a,const idVec3& b) const {
        // A landing must stay on the same platform all the way back to its
        // sampled endpoint, not merely share its height across another gap.
        for (int i=0;i<=32;i++) {
            idVec3 floor;
            const idVec3 point=a+(b-a)*(float(i)/32);
            if (!SampleFloor(point+idVec3(0,0,1),17,floor) || std::fabs(floor.z-point.z)>16) return false;
        }
        return true;
    }
};
// PRODUCTION_FUNCTIONS

void Platform(float xmin,float xmax,float z) {
    solids.push_back({{xmin,-256,z-64},{xmax,256,z}});
}
int main() {
    rvNavMesh nav;
    agentHull=NavCreateAgentTrace(agentBounds);
    // A box touches the far corner of this floor; the real default player's
    // diagonal footprint does not. A node here drops straight into the pit.
    solids.push_back({{13,13,-64},{128,128,0}});
    trace_t corner;
    CHECK(!Sweep(corner,{0,0,1},{0,0,-20},idTraceModel(agentBounds)));
    CHECK(NavAgentTranslation(corner,{0,0,1},{0,0,-20}));
    CHECK(agentHull.type==TRM_CYLINDER && agentHull.numVerts==16);
    // Box servers and signed/custom cylinder settings remain supported.
    pm_usecylinder.value=0; agentHull=NavCreateAgentTrace(agentBounds);
    CHECK(!NavAgentTranslation(corner,{0,0,1},{0,0,-20}));
    pm_usecylinder.value=-1; agentHull=NavCreateAgentTrace(agentBounds);
    CHECK(std::fabs(agentHull.verts[0].x-16)<0.001f && std::fabs(agentHull.verts[0].y)<0.001f);
    pm_usecylinder.value=4; agentHull=NavCreateAgentTrace(agentBounds);
    CHECK(agentHull.numVerts==8 && std::fabs(agentHull.verts[0].x-std::sqrt(128.0f))<0.001f);
    pm_usecylinder.value=1; agentHull=NavCreateAgentTrace(agentBounds); solids.clear();
    // A walk along q4dz3's rim was supported only by the outer two units of
    // the hull; the controller's small lateral error then lost the floor.
    Platform(12,128,0);
    CHECK(!NavAgentTranslation(corner,{0,0,1},{0,0,-20}));
    idVec3 rimFloor;
    CHECK(!nav.SampleFloor({0,0,1},20,rimFloor));
    CHECK(nav.SampleFloor({24,0,1},20,rimFloor));
    solids.clear();
    const float launch=std::sqrt(2.0f*800.0f*48.0f);
    // A lift/cross/down probe reaches the small landing, but a running
    // drop passes its edge. A broad destination still accepts the drop.
    Platform(-256,0,224); Platform(0,48,0);
    CHECK(!nav.RunningDropClear({-12,0,224},{12,0,0}));
    solids.clear(); Platform(-256,0,224); Platform(0,512,0);
    CHECK(nav.RunningDropClear({-12,0,224},{12,0,0}));
    // A grounded eight-unit lip is a legal step before a broad drop, even
    // when a ceiling rules out jumping over it.
    solids.push_back({{-4,-256,224},{0,256,232}});
    solids.push_back({{-256,-256,324},{16,256,384}});
    CHECK(nav.RunningDropClear({-24,0,224},{24,0,0}));
    // Completing on the same Z does not connect two islands across a gap.
    solids.clear(); Platform(-256,0,0); Platform(64,256,0);
    CHECK(!nav.LandingContinuationClear({-24,0,0},{80,0,0}));
    CHECK(nav.LandingContinuationClear({-128,0,0},{-24,0,0}));
    CHECK(nav.LandingContinuationClear({-128,0,0},{-24,0,16})); // an item origin one step above floor
    // The old eight-unit probe could touch a rim without central support.
    CHECK(!nav.LandingContinuationClear({6,0,0},{6,0,0}));
    // Strauss's rounded CTF3 position was sixteen units past the floor edge,
    // aiming diagonally inland. Model genuine sub-unit box contact, not mere
    // tangency, then also exercise the actual default cylinder footprint.
    solids.clear(); Platform(-256,0,0);
    pm_usecylinder.value=0; agentHull=NavCreateAgentTrace(agentBounds);
    CHECK(!nav.LandingContinuationClear({15.75f,0,0},{15.75f,0,0}));
    CHECK(nav.RecoveryStepClear({15.75f,0,0},{-28,63,0}));
    CHECK(!nav.RecoveryStepClear({16,0,0},{-28,63,0})); // zero overlap is not support
    CHECK(!nav.RecoveryStepClear({15.75f,0,0},{80,0,0}));
    Platform(64,256,0);
    CHECK(!nav.RecoveryStepClear({15.75f,0,0},{80,0,0})); // safe endpoint beyond a real gap
    solids.back().lo.x=35;
    CHECK(!nav.RecoveryStepClear({15.75f,0,0},{80,0,0})); // three-unit loss of support between 4u samples
    CHECK(!nav.RecoveryStepClear({15.75f,0,0},{80,12,0}));
    solids.push_back({{-4,8,0},{4,80,100}});
    CHECK(!nav.RecoveryStepClear({15.75f,0,0},{-28,63,0})); // body collides before reaching support
    solids.clear(); Platform(-256,0,0);
    lethal=true; CHECK(!nav.RecoveryStepClear({15.75f,0,0},{-28,63,0})); lethal=false;
    transported=true; CHECK(!nav.RecoveryStepClear({15.75f,0,0},{-28,63,0})); transported=false;
    CHECK(!nav.RecoveryStepClear({-32,0,0},{80,0,0})); // supported starts cannot gain a gap exception
    pm_usecylinder.value=1; agentHull=NavCreateAgentTrace(agentBounds);
    float cylinderRim=0;
    for(int i=0;i<agentHull.numVerts;i++) cylinderRim=Max(cylinderRim,-agentHull.verts[i].x);
    cylinderRim-=0.25f;
    CHECK(nav.RecoveryStepClear({cylinderRim,0,0},{-28,63,0}));
    solids.clear();
    // Only three units of headroom is still a valid standing corridor.
    // Raising the full hull by a step before the floor probe used to fail it.
    Platform(-256,256,0); solids.push_back({{-256,-256,80},{256,256,128}});
    CHECK(nav.LandingContinuationClear({0,0,0},{0,0,0}));
    CHECK(nav.LandingContinuationClear({0,0,0},{48,0,0}));
    CHECK(nav.LandingContinuationClear({0,0,0},{48,0,16}));
    solids.back().lo.z=70;
    CHECK(!nav.LandingContinuationClear({0,0,0},{0,0,0}));
    CHECK(!nav.LandingContinuationClear({0,0,0},{48,0,0}));
    solids.clear();
    auto clear=[&](idVec3 a,idVec3 b) { return nav.GapJumpClear(a,b,320*0.85f,launch,800); };
    // Walking 72 units is safe on this platform, but a full dodge hop flies
    // beyond it. This was never covered by the ordinary strafe-floor probe.
    Platform(-256,96,0);
    CHECK(nav.SampleFloor({72,0,1},20,rimFloor));
    CHECK(!nav.DodgeJumpClear({0,0,0},{320,0,0},{1,0,0},800));
    solids.clear(); Platform(-256,250,0);
    CHECK(nav.DodgeJumpClear({0,0,0},{0,0,0},{1,0,0},800));
    CHECK(!nav.DodgeJumpClear({0,0,0},{0,0,0},{1,0,0},800,416));
    CHECK(!nav.DodgeJumpClear({0,0,0},{0,0,0},{1,0,0},800,480));
    solids.clear(); Platform(-256,512,0);
    CHECK(nav.DodgeJumpClear({0,0,0},{416,0,0},{1,0,0},800,416));
    CHECK(nav.DodgeJumpClear({0,0,0},{480,0,0},{1,0,0},800,480));
    solids.clear(); Platform(-512,512,0);
    CHECK(nav.DodgeJumpClear({0,0,0},{320,0,0},{1,0,0},800));
    // Existing momentum matters even when the requested input points inland.
    solids.clear(); Platform(-48,320,0);
    CHECK(!nav.DodgeJumpClear({0,0,0},{-320,0,0},{1,0,0},800));
    solids.clear(); Platform(-512,512,0); lethal=true;
    CHECK(!nav.DodgeJumpClear({0,0,0},{320,0,0},{1,0,0},800)); lethal=false;
    transported=true;
    CHECK(!nav.DodgeJumpClear({0,0,0},{320,0,0},{1,0,0},800)); transported=false;
    solids.clear();
    // A standing hull and a vertical projection reach this ledge; a real
    // running jump, even braking after passing its corner, overshoots it.
    Platform(-256,16,0); Platform(32,80,-280);
    CHECK(!clear({0,0,0},{48,0,-280}));
    // The same jump over a broad lower floor is still useful and safe.
    solids.clear(); Platform(-256,16,0); Platform(32,512,-280);
    CHECK(clear({0,0,0},{48,0,-280}));
    // Normal gap jumps survive; a narrow island crossed while still rising
    // cannot substitute for a floor under the actual descending trajectory.
    solids.clear(); Platform(-256,16,0); Platform(64,512,0);
    CHECK(clear({0,0,0},{80,0,0}));
    CHECK(nav.JumpTakeoffClear({0,0,0},{320,0,0},{80,0,0},800,320));
    CHECK(!clear({0,0,0},{300,0,0}));
    solids.clear(); Platform(-256,16,0); Platform(64,96,0);
    CHECK(!clear({0,0,0},{80,0,0}));
    // Grounded support does not make a lethal landing valid.
    solids.clear(); Platform(-256,16,0); Platform(64,512,0); lethal=true;
    CHECK(!clear({0,0,0},{80,0,0})); lethal=false;
    // TryStep's jump-height lift must use the trajectory proof when a
    // railing hides a lower ledge, instead of accepting its vertical drop.
    solids.clear(); Platform(-256,16,0); Platform(32,80,-280);
    solids.push_back({{20,-256,0},{24,256,20}});
    idVec3 floor; int travel=-1;
    CHECK(!nav.TryStep({0,0,0},{48,0,0},floor,travel));
    // q4ctf3 also offered a well-supported 28-unit raised decorative lip.
    // A vertical lift reaches its top, but forward momentum carries the
    // player off the far side before touchdown. The broad version is a
    // legitimate stair: touching its vertical side on ascent must not make
    // every raised jump fail.
    solids.clear(); Platform(-256,16,0); Platform(16,40,28);
    CHECK(NavLandingSupported({24,0,28}));
    CHECK(!nav.TryStep({0,0,0},{24,0,0},floor,travel));
    // Multiplayer permits an air step within one step of the floor. That
    // preserves forward speed instead of stalling against the raised face.
    // A medium platform still cannot hold the resulting running touchdown.
    solids.clear(); Platform(-256,16,0); Platform(16,96,28);
    CHECK(!nav.TryStep({0,0,0},{24,0,0},floor,travel));
    solids.clear(); Platform(-256,16,0); Platform(16,512,28);
    CHECK(nav.TryStep({0,0,0},{24,0,0},floor,travel));
    CHECK(travel==NAVTRAVEL_JUMP && std::fabs(floor.z-28)<0.001f);
    CHECK(nav.JumpTakeoffClear({0,0,0},{0,0,0},{24,0,28},800,320));
    // Cortez's recorded CTF3 launch had an11u/5u corner offset and278u/s
    // transverse momentum. The central graph proof was valid but did not
    // cover that state; the live proof must reject it while retaining a
    // settled launch from the same actual position onto a broad strip.
    solids.clear();
    solids.push_back({{-512,360,-96},{-128,640,-32}});
    solids.push_back({{-384,-256,-320},{-256,360,-256}});
    CHECK(nav.GapJumpClear({-300,372,-32},{-300,348,-256},272,launch,800,true));
    CHECK(!nav.JumpTakeoffClear({-289,377,-32},{-278,-160,0},{-300,348,-256},800,320));
    CHECK(nav.JumpTakeoffClear({-289,377,-32},{0,-160,0},{-300,348,-256},800,320));
    CHECK(nav.JumpTakeoffClear({-300,372,-32},{0,-320,0},{-300,348,-256},800,320));
    // The relevant rectangular CTF3 floor extents also reject the recorded
    // launch. This reduced geometry omits trim/decoration, so it does not
    // substitute for the live map test or assert the graph build outcome.
    solids.clear();
    solids.push_back({{-360,368,-48},{-144,528,-32}});
    solids.push_back({{-512,272,-272},{-288,512,-256}});
    solids.push_back({{-288,48,-272},{-160,464,-256}});
    idVec3 firstWish(-11,-29,0); firstWish.Normalize();
    CHECK(!nav.JumpTakeoffClear({-289,377,-32},{-278,-160,0},{-300,348,-256},800,320,&firstWish,232));
    CHECK(nav.JumpTakeoffClear({-289,377,-32},{0,0,0},{-300,348,-256},800,320,&firstWish,232));
    // Rotate the approach diagonally, as on the recorded q4ctf3 route:
    // its takeoff overhangs the old floor by four units and the destination
    // lies on a light housing just beyond that floor's edge.
    solids.clear(); solids.push_back({{-256,-256,-64},{256,-4,0}});
    solids.push_back({{19,-2,-64},{47,33,28}});
    CHECK(NavLandingSupported({24,24,28}));
    CHECK(!nav.TryStep({0,0,0},{24,24,0},floor,travel));
    solids.back().hi={512,512,28};
    CHECK(nav.TryStep({0,0,0},{24,24,0},floor,travel));
    CHECK(travel==NAVTRAVEL_JUMP);
    // Ordinary player-height steps and a low barrier over broad ground
    // remain traversable by their appropriate action.
    solids.clear(); Platform(-256,16,0); Platform(16,512,16);
    CHECK(nav.TryStep({0,0,0},{24,0,0},floor,travel));
    CHECK(travel==NAVTRAVEL_WALK);
    solids.clear(); Platform(-256,512,0);
    solids.push_back({{16,-256,0},{24,256,20}});
    CHECK(nav.TryStep({0,0,0},{48,0,0},floor,travel));
    CHECK(travel==NAVTRAVEL_JUMP);
    // Even the correct hull may graze a lower rim. Drops need enough floor
    // near the destination centre to tolerate normal controller error.
    solids.clear(); Platform(-256,16,324); Platform(62,128,0);
    CHECK(!nav.TryStep({0,0,324},{48,0,324},floor,travel));
    CHECK(nav.SampleFloor({72,0,325},400,floor)); // support exists, but not running touchdown
    CHECK(!nav.TryStep({0,0,324},{72,0,324},floor,travel));
    solids.back().hi.x=512;
    CHECK(nav.TryStep({0,0,324},{72,0,324},floor,travel));
    CHECK(travel==NAVTRAVEL_DROP);
    // q4ctf3's adjacent hop was aimed beside a raised decorative rim. Use a
    // seven-unit overhang so even a zero-epsilon centre box clips the rim,
    // while support around the destination centre still fails.
    solids.clear(); Platform(36,128,0);
    solids.push_back({{4,7,0},{32,32,28}});
    CHECK(NavFloorSupported({0,0,28}));
    CHECK(!NavLandingSupported({0,0,28}));
    CHECK(!nav.TryStep({24,-24,0},{0,0,0},floor,travel));
    // A normal 24-unit-wide landing remains valid, including a four-unit
    // seam through its middle between separately authored floor brushes.
    solids.clear(); solids.push_back({{-12,-64,-64},{12,64,0}});
    CHECK(NavLandingSupported({0,0,0}));
    CHECK(nav.LandingContinuationClear({0,0,0},{0,0,0}));
    solids.clear(); Platform(-128,-2,0); Platform(2,128,0);
    CHECK(NavLandingSupported({0,0,0}));
    CHECK(nav.LandingContinuationClear({0,0,0},{0,0,0}));
    // Side probes must start high enough for a legitimate rising slope.
    // Their hit height is still constrained to the expected landing floor.
    solids.clear(); slopedFloor=true;
    CHECK(nav.SampleFloor({0,0,64},128,floor));
    CHECK(NavLandingSupported(floor));
    CHECK(nav.LandingContinuationClear(floor,floor)); slopedFloor=false;
    // The independent swept landing feeds CrashLand's squared speed and
    // surface/water discounts, not a hardcoded ban on pads or low health.
    trace_t impact; impact.endpos={0,0,0}; impact.c.material=&floorMaterial;
    CHECK(std::fabs(NavLandingImpactDelta(impact,{0,0,-745},{0,0,-800})-55.5025f)<0.001f);
    floorMaterial.flags=SURF_NODAMAGE;
    CHECK(NavLandingImpactDelta(impact,{0,0,-745},{0,0,-800})==0); floorMaterial.flags=0;
    gameLocal.waterTop=2;
    CHECK(std::fabs(NavLandingImpactDelta(impact,{0,0,-745},{0,0,-800})-27.75125f)<0.001f);
    gameLocal.waterTop=50;
    CHECK(std::fabs(NavLandingImpactDelta(impact,{0,0,-745},{0,0,-800})-13.875625f)<0.001f);
    gameLocal.waterTop=80;
    CHECK(NavLandingImpactDelta(impact,{0,0,-745},{0,0,-800})==0); gameLocal.waterTop=-1e30f;
    solids.clear(); Platform(-512,512,0);
    navCorner_t lower{{0,0,0},NAVTRAVEL_DROP};
    CHECK(!nav.LandingSurvivable({0,0,320},lower,40));
    CHECK(nav.LandingSurvivable({0,0,320},lower,60));
    CHECK(nav.LandingSurvivable({0,0,250},lower,40));
    lower.travelType=NAVTRAVEL_JUMP;
    CHECK(!nav.LandingSurvivable({0,0,250},lower,40));
    idEntity pad,target; pad.targets.push_back({&target}); target.physics.origin={0,0,400};
    gameLocal.entities[1]=&pad;
    navCorner_t padEnd{{0,0,0},NAVTRAVEL_JUMPPAD,1};
    CHECK(!nav.LandingSurvivable({0,0,0},padEnd,60));
    CHECK(nav.LandingSurvivable({0,0,0},padEnd,idMath::INFINITY));
    floorMaterial.flags=SURF_NODAMAGE;
    CHECK(nav.LandingSurvivable({0,0,0},padEnd,40)); floorMaterial.flags=0;
    gameLocal.waterTop=50;
    CHECK(nav.LandingSurvivable({0,0,0},padEnd,40)); gameLocal.waterTop=-1e30f;
    target.physics.origin.z=200;
    CHECK(nav.LandingSurvivable({0,0,0},padEnd,40));
    std::puts("Player hulls and jump/drop trajectories: unsupported corners/rims rejected, broad landings/gaps retained: PASS");
    return 0;
}
