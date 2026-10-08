// Copyright (C) 2026 DarkMatter Productions. GPL-3.0-or-later.
#include "RetainedHud.h"
#include <chrono>
#include "DeviceContext.h"
#include "Window.h"
#include "UserInterfaceLocal.h"
#include "RetainedUI.h"
extern idUserInterfaceManagerLocal uiManagerLocal;
#ifndef ID_DEDICATED
#include "../framework/KeyInput.h"
#include "retained/Runtime.h"
#include <json/json.h>
#include <cmath>
#include <cstdlib>
#include <array>
#include <map>
#include <set>

namespace {
constexpr const char* Artwork="guis/hud/artwork.q4ui";
constexpr const char* Registry="guis/hud/coverage.json";
bool Read(const char* path,std::string& result) {
	void* data=nullptr; const int size=fileSystem->ReadFile(path,&data);
	if (size<=0 || !data) return false;
	result.assign(static_cast<const char*>(data),size); fileSystem->FreeFile(data); return true;
}
std::string Key(std::string key) {
	for (auto& c:key) { if (c=='\\') c='/'; else if (c>='A' && c<='Z') c+=32; }
	return key;
}
}
struct idRetainedHud::Impl {
	retainedUIView_t* view=nullptr;
	std::map<std::string,std::string> materials;
	std::map<std::string,std::string> imageAliases;
	std::set<std::string> images,reported;
	bool attempted=false,announced=false,drawn=false;
	~Impl() { RetainedUI_DestroyView(view); }
	void Failure(const char* source,const std::string& error) {
		if (reported.insert(error).second) common->Warning("HUD coverage: %s: %s; using stock presentation",source,error.c_str());
	}
};
idRetainedHud::idRetainedHud() : impl(std::make_unique<Impl>()) {}
idRetainedHud::~idRetainedHud() = default;
void idRetainedHud::Reset() { impl=std::make_unique<Impl>(); }
bool idRetainedHud::DrawSucceeded() const { return impl->drawn; }
void idRetainedHud::Precache() {
	if (!cvarSystem->GetCVarBool("ui_retained")) return;
	// Game-owned damage numbers use the console atlas outside the GUI font
	// loader. Prepare its retained alias while image loading is deferred too.
	RetainedUI_PrecacheImage("fonts/english/bigchars",false);
	std::string source,error; if (!Read(Registry,source)) return;
	Json::Value data; Json::CharReaderBuilder builder; std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
	if (!reader->parse(source.data(),source.data()+source.size(),&data,&error) || !data.isObject() || !data["images"].isArray() ||
		(!data["imageAliases"].isNull() && !data["imageAliases"].isObject())) return;
	for (const auto& image:data["images"]) {
		if (!image.isString()) continue;
		std::string name=image.asString();
		if (data["imageAliases"].isMember(name)) {
			if (!data["imageAliases"][name].isString()) continue;
			name=data["imageAliases"][name].asString();
		}
		const bool additive=name.starts_with("q4-add/");
		RetainedUI_PrecacheImage(additive?name.substr(7):name,additive);
	}
	// Objective photos are chosen by map scripts, not by the HUD document.
	// Register their retained aliases during the engine's deferred image load.
	idFileList* photos=fileSystem->ListFiles("gfx/objectives",".tga",false,true);
	if (photos) {
		for (int i=0;i<photos->GetNumFiles();++i) {
			idStr name=photos->GetFile(i); name.StripFileExtension();
			RetainedUI_PrecacheImage(name.c_str(),false);
		}
		fileSystem->FreeFileList(photos);
	}
}
bool idRetainedHud::Eligible(const char* source) {
	if (!source || !cvarSystem->GetCVarBool("ui_retained")) return false;
	static const std::set<std::string> roots={
		"guis/hud.gui","guis/hud_strogg.gui","guis/cursor.gui","guis/mphud.gui",
		"guis/wristcomm.gui","guis/wristcomm_strogg.gui","guis/scoreboard.gui","guis/summary.gui","guis/mpmsgmode.gui",
		"guis/cinematic.gui","guis/e3cinematic.gui","guis/vehicles/hud.gui","guis/vehicles/airdef_cannon.gui",
		"guis/vehicles/medchange.gui","guis/vehicles/mdisp.gui","guis/vehicles/strogg_noise.gui","guis/vehicles/blank.gui",
		"guis/q4xhud.gui","guis/q4xhud_strogg.gui","guis/q4x_cannon_hud.gui","guis/dropshiphealth.gui","guis/mcchealth.gui","guis/introcinematic.gui",
		"guis/vehicles/speederplayerhud.gui","guis/vehicles/gev_hud.gui","guis/vehicles/mcc/cannonplayerhud.gui","guis/vehicles/airassault/turretplayerhud.gui"};
	if (!roots.contains(Key(source))) return false;
	return true;
}
bool idRetainedHud::Draw(const char* source,const std::vector<openq4::ui::HudPrimitive>& captured) {
	using namespace openq4::ui;
	impl->drawn=false;
	if (!impl->attempted) {
		impl->attempted=true;
		// A mod's replacement owns its artwork and scripts, even at a stock name.
		for (const char* cvar:{"fs_game","fs_game_base"}) {
			const char* mod=cvarSystem->GetCVarString(cvar);
			if (!mod[0] || !idStr::Icmp(mod,"baseoq4") || !idStr::Icmp(mod,"q4base") || !idStr::Icmp(mod,"q4xbase")) continue;
			idFile* file=fileSystem->OpenFileRead(source,false,mod);
			if (file) { fileSystem->CloseFile(file); return false; }
		}
		std::string registry,artwork,error;
		if (!Read(Registry,registry) || !Read(Artwork,artwork)) { impl->Failure(source,"missing retained artwork catalog"); return false; }
		Json::Value data; Json::CharReaderBuilder builder; std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
		if (!reader->parse(registry.data(),registry.data()+registry.size(),&data,&error) || !data.isObject() ||
			!data["version"].isInt() || data["version"].asInt()!=1 || !data["materials"].isObject() || !data["images"].isArray() ||
			(!data["imageAliases"].isNull() && !data["imageAliases"].isObject())) {
			impl->Failure(source,"invalid retained HUD coverage registry"); return false;
		}
		for (const auto& name:data["materials"].getMemberNames()) if (!data["materials"][name].isString()) {
			impl->Failure(source,"invalid HUD material mapping"); return false;
		}
		for (const auto& name:data["images"]) if (!name.isString()) { impl->Failure(source,"invalid HUD image mapping"); return false; }
		for (const auto& name:data["imageAliases"].getMemberNames()) if (!data["imageAliases"][name].isString()) {
			impl->Failure(source,"invalid HUD image alias"); return false;
		}
		for (const auto& name:data["materials"].getMemberNames()) impl->materials.emplace(Key(name),data["materials"][name].asString());
		for (const auto& name:data["images"]) impl->images.insert(Key(name.asString()));
		for (const auto& name:data["imageAliases"].getMemberNames()) impl->imageAliases.emplace(Key(name),data["imageAliases"][name].asString());
		impl->view=RetainedUI_CreateView(); std::vector<Diagnostic> diagnostics;
		if (!impl->view || !RetainedUI_LoadView(impl->view,artwork,Artwork,diagnostics)) {
			impl->Failure(source,diagnostics.empty()?"retained HUD view unavailable":diagnostics.front().message); return false;
		}
	}
	if (!impl->view || !RetainedUI_PrepareView(impl->view)) return false;
	Viewport viewport; if (!RetainedUI_DefaultViewport(viewport)) return false;
	auto items=captured;
	int vectors=0,glyphs=0,images=0;
	for (auto& item:items) {
		const auto name=Key(item.material);
		if (item.glyph) { ++glyphs; continue; }
		const auto found=impl->materials.find(name);
		if (found!=impl->materials.end()) { item.artwork=found->second; ++vectors; }
		else if (impl->images.contains(name) || name.starts_with("gfx/objectives/") || name.starts_with("levelshots/")) {
			++images; const auto alias=impl->imageAliases.find(name);
			if (alias!=impl->imageAliases.end()) item.material=alias->second;
		}
		else { impl->Failure(source,"unmapped material "+name); return false; }
	}
	std::string error;
	if (!RetainedUI_ViewRuntime(impl->view)->SetHudPresentation(items,viewport,error)) { impl->Failure(source,error); return false; }
	const bool drawn=RetainedUI_DrawViewRoot(impl->view,viewport);
	impl->drawn=drawn;
	if (drawn && !impl->announced && !items.empty()) {
		common->Printf("HUD_COVERAGE source=%s renderer=retained vectors=%d glyphs=%d images=%d legacy=0\n",source,vectors,glyphs,images);
		impl->announced=true;
	}
	return drawn;
}
idCVar ui_hudLayout("ui_hudLayout","1",CVAR_GUI|CVAR_INTEGER|CVAR_ARCHIVE,
	"HUD layout: 0 Classic (the stock rectangles), 1 Remastered (anchored to the safe area), "
	"2 Minimal (health, armor, ammunition and the crosshair), 3 Competitive (multiplayer: the match bar, "
	"team status and network quality; elsewhere Remastered)",0,3);
idCVar ui_hudWidth("ui_hudWidth","1",CVAR_GUI|CVAR_INTEGER|CVAR_ARCHIVE,
	"how far edge HUD groups spread on wide views: 0 16:9, 1 21:9, 2 the full view",0,2);

namespace {
struct RemasteredRoute { const char* source; const char* document; };
// Awakening's on-foot HUDs carry the stock groups plus their own expansion
// panels, which their documents cover behind the Marine/Strogg family.
constexpr RemasteredRoute RemasteredRoutes[]={
	{"guis/hud.gui","guis/hud/remastered_marine.q4ui"},
	{"guis/q4xhud.gui","guis/hud/remastered_marine_awakening.q4ui"},
	{"guis/hud_strogg.gui","guis/hud/remastered_strogg.q4ui"},
	{"guis/q4xhud_strogg.gui","guis/hud/remastered_strogg_awakening.q4ui"},
	{"guis/mphud.gui","guis/hud/remastered_mp.q4ui"},
	// The walker's and the hover tank's shared vehicle HUD (14.12).
	{"guis/vehicles/hud.gui","guis/hud/remastered_vehicle.q4ui"},
	// The air-defence cannon's optic, transmission display and HUD prompt (14.12).
	{"guis/vehicles/airdef_cannon.gui","guis/hud/remastered_airdef.q4ui"},
	// The Awakening's space cannon with its ship-status panel (14.12).
	{"guis/vehicles/airassault/turretplayerhud.gui","guis/hud/remastered_turret.q4ui"},
	// The Awakening's MCC cannon with the MCC's hull readout (14.12).
	{"guis/vehicles/mcc/cannonplayerhud.gui","guis/hud/remastered_mcc.q4ui"},
	// The Awakening's speeder bike with the m07 race display (14.12).
	{"guis/vehicles/speederplayerhud.gui","guis/hud/remastered_speeder.q4ui"},
	// The objectives display and its notices (14.11).
	{"guis/wristcomm.gui","guis/hud/remastered_objectives.q4ui"},
	{"guis/wristcomm_strogg.gui","guis/hud/remastered_objectives_strogg.q4ui"},
	// The scoreboard (14.14), Tourney's bracket tree included.
	{"guis/scoreboard.gui","guis/hud/remastered_scoreboard.q4ui"},
};
constexpr int AudioColumns=40;   // the playback graph's 650 ms history (section 14.8)
// ui_hudProfile N: where a Remastered HUD's frame goes, averaged over N frames
// per document: forwarded writes and events, the state batch, the document's
// draw and the stock windows' unseen evaluation.
idCVar ui_hudProfile("ui_hudProfile","0",CVAR_GUI|CVAR_INTEGER,"print Remastered HUD timings averaged over this many frames",0,1000);
double ProfileNow() { return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
}

struct idRemasteredHud::Impl {
	idStr document;
	idUserInterface* gui=nullptr;
	// The document's declared application state: 'n'umber, 'b'oolean, 's'tring.
	// Only these keys cross over, normalized, because one loose legacy value
	// would otherwise reject the document's whole state batch.
	std::map<std::string,char> types;
	bool failed=false,reported=false;
	int failures=0;
	// Named events sent before the document loads (a vehicle's boarding
	// events precede its HUD's first frame), replayed in order after the join.
	std::vector<std::string> pending;
	// Events whose effect outlasts them (extensions.openq4.joinEvents): when a
	// stock window variable holds at the join ("!" inverts the test), the event
	// replays, so a peer rebuilt mid-game - the layout switched to Classic and
	// back while a vehicle is crewed or a transmission plays - shows what the
	// stock windows show. A variable the stock GUI lacks is skipped.
	std::vector<std::pair<std::string,std::string>> joinEvents;
	// Layout readbacks (extensions.openq4.readbacks), applied the next frame;
	// none feeds back into the layout it measures:
	// [state, node, view] sets a boolean state when the node lays out wider
	// than the view (the chat entry's mark of text before its view);
	// [state, node, "@height"] a number state to the node's laid-out height in
	// canvas dp while it has one (the chat prompts, wrapped or not);
	// [state, view, "@firstInside", prefix, count] a number state to the first
	// of nodes prefix0..prefix<count-1> laid out with its top inside the
	// view, or count (the open chat's history fades the lines its room cuts).
	struct Readback { std::string state,node,view,prefix; int count=0; };
	std::vector<Readback> readbacks;
	std::map<std::string,float> readbackValues;
	// ui_hudProfile accumulators
	double forwardMs=0,eventMs=0,stateMs=0,drawMs=0,unseenMs=0;
	int forwards=0,events=0,frames=0;
	std::map<std::string,std::pair<int,double>> eventCost;
	void Report() {
		const int wanted=ui_hudProfile.GetInteger();
		if (wanted<=0) { frames=0; return; }
		if (++frames<wanted) return;
		common->Printf("HUD_PROFILE document=%s frames=%d forward_ms=%.3f forwards=%.1f event_ms=%.3f events=%.1f state_ms=%.3f draw_ms=%.3f unseen_ms=%.3f\n",
			document.c_str(),frames,forwardMs/frames,double(forwards)/frames,eventMs/frames,double(events)/frames,stateMs/frames,drawMs/frames,unseenMs/frames);
		for (const auto& [name,cost]:eventCost) common->Printf("HUD_PROFILE_EVENT document=%s name=%s count=%d ms=%.3f\n",document.c_str(),name.c_str(),cost.first,cost.second);
		forwardMs=eventMs=stateMs=drawMs=unseenMs=0; forwards=events=frames=0; eventCost.clear();
	}
	~Impl() { if (gui) uiManager->DeAlloc(gui); }
	bool ReadTypes(const std::string& source) {
		Json::Value data; std::string error;
		Json::CharReaderBuilder builder; builder["allowComments"]=true;
		std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
		if (!reader->parse(source.data(),source.data()+source.size(),&data,&error) || !data.isObject()) return false;
		const Json::Value& joins=data["extensions"]["openq4"]["joinEvents"];
		if (joins.isArray()) for (const auto& pair:joins) {
			if (pair.isArray() && pair.size()==2 && pair[0].isString() && pair[1].isString())
				joinEvents.emplace_back(pair[0].asString(),pair[1].asString());
		}
		const Json::Value& measured=data["extensions"]["openq4"]["readbacks"];
		if (measured.isArray()) for (const auto& entry:measured) {
			if (!entry.isArray() || (entry.size()!=3 && entry.size()!=5)) continue;
			bool strings=true;
			for (const auto& part:entry) strings=strings && part.isString();
			if (!strings) continue;
			Readback readback{entry[0].asString(),entry[1].asString(),entry[2].asString()};
			if (entry.size()==5) { readback.prefix=entry[3].asString(); readback.count=atoi(entry[4].asString().c_str()); }
			readbacks.push_back(std::move(readback));
		}
		const Json::Value& state=data["state"];
		if (!state.isObject()) return true;
		for (const auto& name:state.getMemberNames()) {
			const Json::Value& entry=state[name];
			if (!entry.isObject() || entry.isMember("cvar")) continue;
			const std::string type=entry["type"].asString();
			types[name]=type=="number" ? 'n' : type=="boolean" ? 'b' : 's';
		}
		return true;
	}
	// An icon material reaches the document as its base name ("item_machinegun"
	// for gfx/guis/hud/icons/item_machinegun_colored.tga), so one comparison
	// names a glyph whatever spelling the item definition used.
	static std::string IconName(const char* value) {
		std::string name=Key(value);
		const auto slash=name.find_last_of('/');
		if (slash!=std::string::npos) name.erase(0,slash+1);
		const auto dot=name.find_last_of('.');
		if (dot!=std::string::npos) name.erase(dot);
		if (name.size()>8 && name.ends_with("_colored")) name.resize(name.size()-8);
		return name;
	}
	// HUD text reaches the stock GUI with its color and icon escapes. A key
	// escape (^ikHH or ^iKHH, HH the bound key's number in hex) becomes the
	// key's name for the document's prompts; every other escape goes.
	static idStr PlainText(const char* value) {
		const auto isHex=[](char c) { return (c>='0' && c<='9') || ((c|0x20)>='a' && (c|0x20)<='f'); };
		const auto hex=[](char c) { return c<='9' ? c-'0' : (c|0x20)-'a'+10; };
		idStr text;
		for (const char* p=value; *p; ) {
			if (p[0]=='^' && p[1]=='i' && (p[2]=='k' || p[2]=='K') && isHex(p[3]) && isHex(p[4])) {
				text+=idKeyInput::KeyNumToString(hex(p[3])*16+hex(p[4]),true);
				p+=5;
			} else {
				text+=*p++;
			}
		}
		text.RemoveEscapes();
		return text;
	}
	void Forward(const char* name,const char* value) {
		const auto found=types.find(name);
		if (found==types.end() || !value) return;
		if (found->second=='s') {
			const std::string key=Key(name);
			if (key.size()>=4 && key.ends_with("icon")) gui->SetStateString(name,IconName(value).c_str());
			else gui->SetStateString(name,PlainText(value).c_str());
			return;
		}
		char* end=nullptr;
		const double number=std::strtod(value,&end);
		const bool parsed=end!=value && std::isfinite(number);
		if (found->second=='n') gui->SetStateString(name,parsed ? va("%.9g",number) : "0");
		else gui->SetStateBool(name,parsed ? number!=0 : !idStr::Icmp(value,"true"));
	}
	void Fail(const char* reason) {
		failed=true;
		if (!reported) { common->Warning("Remastered HUD %s: %s; using the stock layout",document.c_str(),reason); reported=true; }
	}
	// Decoded peaks of a playing channel, oldest first, from its emitter's
	// own history. Missing, stopped and finished audio stays flat.
	void PublishAudio(const idDict& state,const char* source,const char* target) {
		float peaks[AudioColumns]={};
		const int emitterIndex=state.GetInt(va("%s_emitter",source));
		const int channel=state.GetInt(va("%s_channel",source));
		idSoundWorld* world=soundSystem->GetSoundWorldFromId(SOUNDWORLD_GAME);
		idSoundEmitter* emitter=world && emitterIndex>0 ? world->EmitterForIndex(emitterIndex) : nullptr;
		if (emitter && channel>SCHANNEL_ANY) emitter->AmplitudeHistory(channel,peaks,AudioColumns);
		// Reduced motion keeps the instrument's level without horizontal travel.
		if (cvarSystem->GetCVarBool("ui_retainedReducedMotion")) for (int i=0;i<AudioColumns-1;++i) peaks[i]=peaks[AudioColumns-1];
		for (int i=0;i<AudioColumns;++i) gui->SetStateFloat(va("%s.%d",target,i),idMath::ClampFloat(0,1,peaks[i]));
	}
};

idRemasteredHud::idRemasteredHud(const char* document) : impl(std::make_unique<Impl>()) { impl->document=document; }
idRemasteredHud::~idRemasteredHud() = default;
bool idRemasteredHud::Failed() const { return impl->failed; }
const char* idRemasteredHud::Document() const { return impl->document.c_str(); }
const char* idRemasteredHud::DocumentFor(const char* source) {
	if (!source) return nullptr;
	const auto key=Key(source);
	for (const auto& route:RemasteredRoutes) if (key==route.source) return route.document;
	return nullptr;
}
bool idRemasteredHud::Enabled() {
	return cvarSystem->GetCVarBool("ui_retained") && ui_hudLayout.GetInteger()!=0;
}
void idRemasteredHud::ForwardState(const char* name,const char* value) {
	if (!impl->gui) return;
	if (!ui_hudProfile.GetInteger()) { impl->Forward(name,value); return; }
	const double started=ProfileNow();
	impl->Forward(name,value);
	impl->forwardMs+=ProfileNow()-started; ++impl->forwards;
}
void idRemasteredHud::ForwardDelete(const char* name) { if (impl->gui && impl->types.count(name)) impl->gui->DeleteStateVar(name); }
void idRemasteredHud::ForwardEvent(const char* name) {
	if (!impl->gui) {
		if (!impl->failed && impl->pending.size()<32) impl->pending.emplace_back(name);
		return;
	}
	if (!ui_hudProfile.GetInteger()) { impl->gui->HandleNamedEvent(name); return; }
	const double started=ProfileNow();
	impl->gui->HandleNamedEvent(name);
	const double cost=ProfileNow()-started;
	impl->eventMs+=cost; ++impl->events;
	auto& entry=impl->eventCost[name]; ++entry.first; entry.second+=cost;
}
void idRemasteredHud::ProfileUnseen(double milliseconds) { impl->unseenMs+=milliseconds; impl->Report(); }

bool idRemasteredHud::Draw(const idUserInterfaceLocal& legacy,int time) {
	if (impl->failed) return false;
	if (!impl->gui) {
		// A restore must never create GUIs outside its serialized stream.
		if (session && session->IsLoadingSaveGame()) return false;
		// A mod's own stock-named HUD owns its layout and scripts.
		for (const char* cvar:{"fs_game","fs_game_base"}) {
			const char* mod=cvarSystem->GetCVarString(cvar);
			if (!mod[0] || !idStr::Icmp(mod,"baseoq4") || !idStr::Icmp(mod,"q4base") || !idStr::Icmp(mod,"q4xbase")) continue;
			idFile* file=fileSystem->OpenFileRead(legacy.GetSourceFile(),false,mod);
			if (file) { fileSystem->CloseFile(file); impl->failed=true; return false; }
		}
		std::string source;
		if (!Read(impl->document.c_str(),source)) { impl->Fail("the document is not installed"); return false; }
		if (!impl->ReadTypes(source)) { impl->Fail("the document's state declarations do not parse"); return false; }
		impl->gui=uiManager->FindGui(impl->document.c_str(),true,true);
		if (!impl->gui) { impl->Fail("the document did not load"); return false; }
		// Join the stock GUI's dictionary as the game left it; later writes and
		// named events reach both, so nothing older than this frame is lost.
		const idDict& state=legacy.State();
		for (int i=0;i<state.GetNumKeyVals();++i) {
			const idKeyValue* entry=state.GetKeyVal(i);
			impl->Forward(entry->GetKey().c_str(),entry->GetValue().c_str());
		}
		impl->gui->HandleNamedEvent("hudJoin");
		for (const auto& [variable,event]:impl->joinEvents) {
			const bool invert=variable.starts_with('!');
			idStr value;
			if (legacy.GetPresentationValue(variable.c_str()+(invert ? 1 : 0),value) && (atof(value.c_str())!=0)!=invert)
				impl->gui->HandleNamedEvent(event.c_str());
		}
		for (const auto& name:impl->pending) impl->gui->HandleNamedEvent(name.c_str());
		impl->pending.clear();
	}
	// The layout of section 14.4 in canvas dp: the 720 dp high (960 dp wide
	// below 4:3) canvas, its side screens capped by the HUD width setting, the
	// safe inset on every edge and the independent HUD scale.
	openq4::ui::Viewport viewport;
	if (!RetainedUI_DefaultViewport(viewport)) return false;
	const float ratio=Min(viewport.height/720.f,viewport.width/960.f);
	if (!(ratio>0)) return false;
	const float width=viewport.width/ratio,height=viewport.height/ratio;
	const float cap=ui_hudWidth.GetInteger()==0 ? 1280.f : ui_hudWidth.GetInteger()==1 ? 720.f*64.f/27.f : width;
	const float side=Max(0.f,(width-Min(width,cap))*.5f);
	const float inset=idMath::ClampFloat(0,10,cvarSystem->GetCVarFloat("ui_hudSafeInset"))*.01f;
	const float scale=idMath::ClampFloat(.5f,2.f,cvarSystem->GetCVarFloat("ui_hudScale"));
	const float span=width-2*(side+inset*width);
	impl->gui->SetStateFloat("hud.width",width);
	impl->gui->SetStateFloat("hud.height",height);
	impl->gui->SetStateFloat("hud.left",side+inset*width);
	impl->gui->SetStateFloat("hud.right",side+inset*width);
	impl->gui->SetStateFloat("hud.top",inset*height);
	impl->gui->SetStateFloat("hud.bottom",inset*height);
	impl->gui->SetStateFloat("hud.scale",scale);
	// Narrow views fit the center column (the 438 u status bar and its strip)
	// without changing the saved preference.
	impl->gui->SetStateFloat("hud.centerScale",Min(scale,Max(.25f,span/690.f)));
	const idDict& state=legacy.State();
	if (state.FindKey("radio_audio_emitter")) impl->PublishAudio(state,"radio_audio","radio.peak");
	if (state.FindKey("voice_self_audio_emitter")) impl->PublishAudio(state,"voice_self_audio","voice.self.peak");
	if (state.FindKey("voice_audio_emitter")) impl->PublishAudio(state,"voice_audio","voice.other.peak");
	const double stateStarted=ProfileNow();
	impl->gui->StateChanged(time,false);
	const double drawStarted=ProfileNow();
	impl->gui->Redraw(time);
	impl->stateMs+=drawStarted-stateStarted;
	impl->drawMs+=ProfileNow()-drawStarted;
	idStr drawn;
	if (impl->gui->GetPresentationValue("__retained::drawSucceeded",drawn) && drawn=="1") {
		impl->failures=0;
		const auto measure=[&](const std::string& node,const char* field,float& value) {
			idStr text;
			if (!impl->gui->GetPresentationValue(va("%s::@%s",node.c_str(),field),text)) return false;
			value=atof(text); return true;
		};
		for (const auto& [name,node,view,prefix,count]:impl->readbacks) {
			if (view=="@firstInside") {
				float top=0,y=0,height=0;
				if (!measure(node,"y",top)) continue;
				int first=count;
				for (int i=0;i<count;++i) {
					// A node out of the layout keeps its last box: skip it.
					const std::string child=prefix+std::to_string(i);
					idStr display;
					if (impl->gui->GetPresentationValue(va("%s::display",child.c_str()),display) && display=="none") continue;
					if (measure(child,"height",height) && height>0 && measure(child,"y",y) && y>=top-.5f) { first=i; break; }
				}
				const auto last=impl->readbackValues.find(name);
				if (last==impl->readbackValues.end() || last->second!=first) {
					impl->readbackValues[name]=float(first); impl->gui->SetStateFloat(name.c_str(),float(first));
				}
				continue;
			}
			if (view=="@height") {
				idStr height;
				if (!impl->gui->GetPresentationValue(va("%s::@height",node.c_str()),height)) continue;
				const float value=atof(height);
				const auto last=impl->readbackValues.find(name);
				if (value>0 && (last==impl->readbackValues.end() || idMath::Fabs(last->second-value)>.25f)) {
					impl->readbackValues[name]=value; impl->gui->SetStateFloat(name.c_str(),value);
				}
				continue;
			}
			idStr nodeWidth,viewWidth;
			const bool wider=impl->gui->GetPresentationValue(va("%s::@width",node.c_str()),nodeWidth) &&
				impl->gui->GetPresentationValue(va("%s::@width",view.c_str()),viewWidth) && atof(nodeWidth)>atof(viewWidth)+.5;
			auto& last=impl->readbackValues[name];
			if ((last!=0)!=wider) { last=wider ? 1.f : 0.f; impl->gui->SetStateBool(name.c_str(),wider); }
		}
		return true;
	}
	// A frame that cannot present falls back at once; a persistent failure
	// stops retrying until the gate, layout or document changes.
	if (++impl->failures>=30) impl->Fail("the document failed to draw");
	return false;
}
#else
struct idRetainedHud::Impl {};
idRetainedHud::idRetainedHud() = default;
idRetainedHud::~idRetainedHud() = default;
void idRetainedHud::Reset() {}
bool idRetainedHud::DrawSucceeded() const { return false; }
void idRetainedHud::Precache() {}
bool idRetainedHud::Eligible(const char*) { return false; }
bool idRetainedHud::Draw(const char*,const std::vector<openq4::ui::HudPrimitive>&) { return false; }
struct idRemasteredHud::Impl {};
idRemasteredHud::idRemasteredHud(const char*) {}
idRemasteredHud::~idRemasteredHud() = default;
const char* idRemasteredHud::DocumentFor(const char*) { return nullptr; }
bool idRemasteredHud::Enabled() { return false; }
bool idRemasteredHud::Draw(const idUserInterfaceLocal&,int) { return false; }
void idRemasteredHud::ForwardState(const char*,const char*) {}
void idRemasteredHud::ForwardDelete(const char*) {}
void idRemasteredHud::ForwardEvent(const char*) {}
void idRemasteredHud::ProfileUnseen(double) {}
bool idRemasteredHud::Failed() const { return true; }
const char* idRemasteredHud::Document() const { return ""; }
#endif

bool idUserInterfaceLocal::DrawRemasteredHud(int drawTime) {
	const char* document=idRemasteredHud::DocumentFor(source.c_str());
	if (!document || !idRemasteredHud::Enabled()) { delete remastered; remastered=nullptr; return false; }
	if (!remastered) remastered=new idRemasteredHud(document);
	if (remastered->Failed() || !remastered->Draw(*this,drawTime)) return false;
	// The stock windows keep evaluating unseen, so their scripts, timers and
	// presentation aliases stay exactly where Classic would find them.
	const auto started=std::chrono::steady_clock::now();
	std::vector<openq4::ui::HudPrimitive> unseen;
	uiManagerLocal.dc.BeginHudCapture(unseen);
	uiManagerLocal.dc.PushClipRect(uiManagerLocal.screenRect);
	desktop->Redraw(0,0);
	uiManagerLocal.dc.PopClipRect();
	uiManagerLocal.dc.EndHudCapture();
	remastered->ProfileUnseen(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count());
	return true;
}

bool idUserInterfaceLocal::RemasteredActive() const {
	// Answers before the first draw too, so a caller never pairs the stock
	// gauges with the Remastered layout for a frame.
	return idRemasteredHud::Enabled() && idRemasteredHud::DocumentFor(source.c_str()) && !(remastered && remastered->Failed());
}

bool idUserInterfaceLocal::DrawRetainedHud() {
	if (!idRetainedHud::Eligible(source.c_str())) {
		delete retainedHud; retainedHud=nullptr; return false;
	}
	if (!retainedHud) retainedHud=new idRetainedHud;
	std::vector<openq4::ui::HudPrimitive> items;
	uiManagerLocal.dc.BeginHudCapture(items);
	uiManagerLocal.dc.PushClipRect(uiManagerLocal.screenRect);
	desktop->Redraw(0,0);
	uiManagerLocal.dc.PopClipRect();
	uiManagerLocal.dc.EndHudCapture();
	// RunTimeEvents is time-guarded: fallback redraw cannot replay script actions.
	return retainedHud->Draw(source.c_str(),items);
}

void idUserInterfaceManagerLocal::ClearHudPrimitives() {
	generatedHuds.DeleteContents(true); generatedHudNames.Clear();
}

bool idUserInterfaceManagerLocal::DrawHudPrimitives(const char* source,const idList<uiHudPrimitive_t>& packet) {
	if (!cvarSystem->GetCVarBool("ui_retained")) { ClearHudPrimitives(); return false; }
	if (!source || packet.Num()>32768) return false;
	int index=generatedHudNames.FindIndex(source);
	if (index<0) {
		if (generatedHuds.Num()>=16) return false;
		index=generatedHudNames.Append(source); generatedHuds.Append(new idRetainedHud);
	}
	std::vector<openq4::ui::HudPrimitive> items;
	items.reserve(packet.Num());
	for (int i=0;i<packet.Num();++i) {
		const auto& p=packet[i]; if (!p.material) return false;
		openq4::ui::HudPrimitive item;item.material=p.material->GetName();item.glyph=p.glyph;
		for (int n=0;n<4;++n) {
			item.quad[2*n]=p.points[n].x;item.quad[2*n+1]=p.points[n].y;
			item.colour[n]=p.color[n];item.uv[n]=p.uv[n];
		}
		items.push_back(std::move(item));
	}
	return generatedHuds[index]->Draw(source,items);
}

void idUserInterfaceManagerLocal::HudTest(const idCmdArgs& args) {
	if (args.Argc()<3) { common->Printf("ui_hudTest <source.gui> report | event <name> | state <key> <value> | get <key> | presentation <alias> <value>\n"); return; }
	int count=0;
	for (int i=0;i<allocations.Num();++i) {
		auto* gui=allocations[i]; if (idStr::Icmp(gui->Name(),args.Argv(1))) continue;
		if (!idStr::Icmp(args.Argv(2),"report")) {
			idStr value; gui->GetPresentationValue("__retained::hudDrawSucceeded",value);
			common->Printf("HUD_REPORT source=%s drawn=%s\n",gui->Name(),value.c_str());
		} else if (args.Argc()==4 && !idStr::Icmp(args.Argv(2),"event")) gui->HandleNamedEvent(args.Argv(3));
		else if (args.Argc()==5 && !idStr::Icmp(args.Argv(2),"state")) gui->SetStateString(args.Argv(3),args.Argv(4));
		else if (args.Argc()==4 && !idStr::Icmp(args.Argv(2),"get")) {
			common->Printf("HUD_STATE source=%s key=%s value=%s\n",gui->Name(),args.Argv(3),gui->State().GetString(args.Argv(3)));
			++count; continue;
		}
		else if (args.Argc()==5 && !idStr::Icmp(args.Argv(2),"presentation")) {
			if (!gui->SetPresentationValue(args.Argv(3),args.Argv(4))) common->Warning("HUD probe: unknown presentation alias %s",args.Argv(3));
		} else continue;
		gui->StateChanged(common->GetPresentationTime()); ++count;
	}
	if (!count) common->Warning("HUD probe: no matching GUI or operation: %s",args.Argv(1));
}
