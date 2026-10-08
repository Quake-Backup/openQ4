#include <chrono>
// Remastered HUD documents (ui-visual-design.md 14.4-14.12): each loads
// through the real compiler and runtime, lands its groups on the section 14.4
// anchors at several aspects, and answers the stock HUD's state and events.

static Viewport RemasteredViewport(int width, int height) {
	Viewport viewport; viewport.width = width; viewport.height = height;
	viewport.canvasHeight = 720; viewport.canvasMinWidth = 960; viewport.fitScale = 1;
	return viewport;
}

// The engine peer's layout values for a viewport (RetainedHud.cpp).
static StateValues RemasteredLayout(int width, int height, float scale = 1, float insetPercent = 0, int hudWidth = 1) {
	const float ratio = std::min(height/720.f,width/960.f);
	const float w = width/ratio, h = height/ratio;
	const float cap = hudWidth == 0 ? 1280.f : hudWidth == 1 ? 720.f*64.f/27.f : w;
	const float side = std::max(0.f,(w-std::min(w,cap))*.5f);
	const float inset = insetPercent*.01f;
	const float span = w-2*(side+inset*w);
	return {{"hud.width",double(w)},{"hud.height",double(h)},{"hud.left",double(side+inset*w)},{"hud.right",double(side+inset*w)},
		{"hud.top",double(inset*h)},{"hud.bottom",double(inset*h)},{"hud.scale",double(scale)},
		{"hud.centerScale",double(std::min(scale,std::max(.25f,span/690.f)))}};
}

// The HUD layout preference the documents read (ui_hudLayout: 1 Remastered, 2 Minimal).
static void RemasteredLayoutPreference(ScreenHost& host, double layout) { host.cvars["ui_hudLayout"] = layout; }

static bool Shown(const Runtime& runtime, const std::string& id) {
	const auto value = runtime.PresentedValue(id,"display");
	return value && value->text != "none";
}

static void CheckRemasteredHud(ScreenHost& host, const char* path, const std::string& id, bool strogg) {
	const auto source = Read(path);
	Document document; std::vector<Diagnostic> diagnostics;
	if (!document.Load(source,diagnostics)) for (const auto& d : diagnostics) std::fprintf(stderr,"%s %u:%u %s\n",path,unsigned(d.line),unsigned(d.column),d.message.c_str());
	Check(diagnostics.empty() && document.Model().id == id,"Remastered HUD validates");
	Check(document.Model().canvasHeight == 720 && document.Model().canvasMinWidth == 960,"Remastered HUD uses the 4:3 center-screen canvas");
	RemasteredLayoutPreference(host,1);
	Runtime runtime(host);
	Check(runtime.Initialize(),"retained runtime");
	Check(runtime.LoadDocument(source,"guis/hud/remastered.q4ui",diagnostics),"Remastered HUD loads into a runtime");
	std::string error;
	StateValues values = RemasteredLayout(1280,720);
	values.insert({{"player_health",100.0},{"player_healthpct",1.0},{"player_armor",57.0},{"player_armorpct",.57},
		{"player_ammo",36.0},{"player_totalammo",50.0},{"player_clip_size",50.0},{"player_ammopct",.72},{"player_weapon",5.0},
		{"ammoIcon",std::string("item_nailgun")},{"weapicon",std::string("gun_nailgun")},{"mp",false}});
	Check(runtime.SetState(values,error,0),error.c_str());
	Runtime::EventEffects effects;
	Check(runtime.RunEvent("hudJoin",0,effects,error),"the join program runs");
	host.draws.clear();
	runtime.Frame(RemasteredViewport(1280,720),.1);
	Check(host.errors == 0,"Remastered HUD frames without runtime errors");
	Check(!host.draws.empty(),"Remastered HUD draws native geometry");
	Bounds ammo, health, armor;
	// The status bar: one 438 u group centred on the bottom edge (Strogg 449 u).
	Check(runtime.GetBounds("ammo-gauge",ammo) && runtime.GetBounds("health-gauge",health) && runtime.GetBounds("armor-gauge",armor),
		"the three gauges are laid out");
	const float first = strogg ? 95.5f : 101.f, second = strogg ? 275.5f : 278.f, third = strogg ? 415.5f : 414.f;
	Check(Near(ammo.x,160+first*1.5f) && Near(health.x,160+second*1.5f) && Near(armor.x,160+third*1.5f),
		"the status bar sits centred with the stock gaps at 16:9");
	Check(Near(ammo.y,(strogg ? 419 : 420)*1.5f),"gauges rest on the bottom edge");
	// Armor at zero hides its gauge without moving the others.
	values["player_armor"] = 0.0; values["player_armorpct"] = 0.0;
	Check(runtime.SetState(values,error,.2),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),.25);
	Bounds moved;
	Check(!Shown(runtime,"armor-gauge"),"armor hides at zero");
	Check(runtime.GetBounds("health-gauge",moved) && Near(moved.x,health.x),"health keeps its place when armor goes");
	values["player_armor"] = 57.0; values["player_armorpct"] = .2;
	Check(runtime.SetState(values,error,.3),error.c_str());
	// The armor fill drains toward the numerals: at 20% only the trailing
	// part from 120 - 115 x 0.2 = 97 u shows, cut parallel to the sheared
	// sides. Fill paint is #B2CF6D at 0.35; its vertices start no further
	// left than the cut (plus the 0.22 shear over the gauge height).
	host.draws.clear();
	runtime.Frame(RemasteredViewport(1280,720),.32);
	{
		// The Strogg fill (s_valbar, #FF9000 at 0.35) runs 119 u to 124 u: 100.2 u at 20%.
		const float lead = strogg ? 124 - 119*.2f : 97, fillR = strogg ? 1.f : 0xB2/255.f, fillG = strogg ? 0x90/255.f : 0xCF/255.f;
		Bounds gauge; runtime.GetBounds("armor-gauge",gauge);
		const auto window = runtime.PresentedValue("armor-fill-window","left");
		float minX = 1e9f; size_t count = 0;
		for (const auto& draw : host.draws) for (const auto& v : draw.vertices)
			if (std::abs(v.r-fillR*.35f) < .02f && std::abs(v.g-fillG*.35f) < .02f && std::abs(v.a-.35f) < .02f &&
				v.x > gauge.x-20 && v.x < gauge.x+gauge.width+20) { minX = std::min(minX,v.x); ++count; }
		const float cut = gauge.x + lead*1.5f - .22f*29.5f*1.5f - 2;
		std::fprintf(stderr,"armor window left %.2f, fill vertices %zu min x %.1f (cut at about %.1f, gauge %.1f..%.1f), clip masks %llu fallbacks %llu\n",
			window ? window->data[0] : -1.f,count,minX,cut,gauge.x,gauge.x+gauge.width,
			(unsigned long long)runtime.Statistics().clipMasks,(unsigned long long)runtime.Statistics().clipMaskFallbacks);
		Check(window && Near(float(window->data[0]),lead*1.5f,.01f),"the fill window's leading edge follows the armor");
		Check(count > 0 && minX >= cut,"the fill is cut at its leading edge");
	}
	values["player_armorpct"] = .57;
	Check(runtime.SetState(values,error,.35),error.c_str());

	// 21:9 (2520x1080): side screens capped at 21:9; the transmission display
	// takes the trailing edge, the status bar stays centred.
	Check(runtime.SetState(RemasteredLayout(2520,1080),error,.4),error.c_str());
	Check(runtime.RunEvent("radioChatterUp",.4,effects,error),"radio up");
	runtime.Frame(RemasteredViewport(2520,1080),.5);
	// The top-trailing anchor box (the stock canvas) ends at the view's edge.
	Bounds corner;
	const float px = 1080/720.f;   // pixels per dp at 1080p
	Check(runtime.GetBounds("anchor-tr",corner) && Near(corner.x+corner.width,2520,1.5f) && Near(corner.width,960*px,1.5f) && Near(corner.y,0),
		"the transmission display's anchor takes the trailing edge at 21:9");
	Check(Shown(runtime,"radio"),"the transmission display shows on radioChatterUp");
	Check(runtime.GetBounds("ammo-gauge",ammo) && Near(ammo.x,(2520-960*px)/2+first*1.5f*px,1.5f),"the status bar stays centred at 21:9");

	// 32:9 (3840x1080): edge groups stop at the 21:9 edges.
	Check(runtime.SetState(RemasteredLayout(3840,1080),error,.6),error.c_str());
	runtime.Frame(RemasteredViewport(3840,1080),.7);
	const float cap = 720*64/27.f*px;
	Check(runtime.GetBounds("anchor-tr",corner) && Near(corner.x+corner.width,(3840+cap)/2,2.f),
		"the trailing groups stop at the 21:9 edge on 32:9");

	// 9:16 portrait (720x1280): the center screen spans the width.
	Check(runtime.SetState(RemasteredLayout(720,1280),error,.8),error.c_str());
	runtime.Frame(RemasteredViewport(720,1280),.9);
	Check(runtime.GetBounds("ammo-gauge",ammo) && ammo.x > 0 && ammo.x+ammo.width < 720,"the status bar fits the portrait width");
	// The Strogg gauge rect (419 + 67 u) overhangs the canvas below its art.
	Check(runtime.GetBounds("armor-gauge",armor) && armor.y+armor.height <= 1280+(strogg ? 6*1.5f*.75f+1 : 1) && armor.y > 1000,
		"gauges ride the portrait bottom edge");

	// HUD scale grows each group about its anchor: the bottom edge holds.
	Check(runtime.SetState(RemasteredLayout(1280,720,1.5f),error,1.0),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),1.05);
	Bounds scaled;
	// Scaled about the bottom-centre anchor point (480, 720 dp of its box):
	// with the box centre at (480, 360) the compensating translation is
	// (0, (720 - 360)(1 - 1.5)).
	const auto bottom = runtime.PresentedValue("anchor-bc","transform");
	Check(bottom && Near(float(bottom->data[0]),0,.01f) && Near(float(bottom->data[1]),-180,.01f) &&
		Near(float(bottom->data[2]),1.5f,.001f) && Near(float(bottom->data[3]),1.5f,.001f),
		"HUD scale grows the status bar about the bottom edge");
	const auto corner2 = runtime.PresentedValue("anchor-tr","transform");
	Check(corner2 && Near(float(corner2->data[0]),-240,.01f) && Near(float(corner2->data[1]),180,.01f),
		"HUD scale grows trailing groups about their corner");
	(void)scaled;
	Check(runtime.SetState(RemasteredLayout(1280,720),error,1.1),error.c_str());

	// The weapon strip shows on a change, after the first weapon.
	Check(runtime.SetState({{"weapon0",true},{"weapon0_index",0.0},{"weapon5",true},{"weapon5_index",1.0},{"weapon5_ammo",86.0},
		{"weapon0_ammo",-1.0},{"weaponcount",2.0},{"player_idealweapon",5.0},{"player_lastweapon",0.0},{"weaponname",std::string("NAILGUN")}},error,1.2),error.c_str());
	Check(runtime.RunEvent("weaponChange",1.2,effects,error),"weapon change runs");
	runtime.Frame(RemasteredViewport(1280,720),1.3);
	Bounds slot0, slot5;
	Check(runtime.GetBounds("slot0",slot0) && runtime.GetBounds("slot5",slot5) && slot0.width > 0,"strip slots appear on a change");
	Check(Near(slot5.x-slot0.x,30*1.5f),"slots sit on a 30 u pitch");
	Check(Near((slot0.x+slot5.x+26*1.5f)/2,640,1.5f),"the strip is centred");
	runtime.Frame(RemasteredViewport(1280,720),2.4);
	Check(!Shown(runtime,"strip"),"the strip hides 1000 ms after the change");

	// Pickups: a repeat merges with a count; a different item adds a line.
	Check(runtime.SetState({{"itemtext",std::string("Armor Shard")},{"itemicon",std::string("item_armorshard")}},error,2.5),error.c_str());
	Check(runtime.RunEvent("itemPickup",2.5,effects,error) && runtime.RunEvent("itemPickup",2.55,effects,error),"pickups run");
	runtime.Frame(RemasteredViewport(1280,720),2.6);
	auto count = runtime.PresentedValue("pickup0-count","text");
	auto count1 = runtime.PresentedValue("pickup1-count","text");
	Check((count && count->text == "2") || (count1 && count1->text == "2"),"a repeated pickup merges into one line with a count");
	Check(runtime.SetState({{"itemtext",std::string("Large Health")},{"itemicon",std::string("item_health")}},error,2.7),error.c_str());
	Check(runtime.RunEvent("itemPickup",2.7,effects,error),"second pickup");
	runtime.Frame(RemasteredViewport(1280,720),2.8);
	Bounds p0, p1;
	Check(runtime.GetBounds("pickup0",p0) && runtime.GetBounds("pickup1",p1) && std::abs(p0.y-p1.y) > 30,"two pickup lines stack");
	runtime.Frame(RemasteredViewport(1280,720),6.5);
	Check(!Shown(runtime,"pickup0") && !Shown(runtime,"pickup1"),"pickup lines leave after 3250 ms");

	// Low health switches the health colours and pulses the rim.
	Check(runtime.SetState({{"player_health",20.0},{"player_healthpct",.2},{"player_healthDelta",80.0}},error,7),error.c_str());
	Check(runtime.RunEvent("updateHealth",7,effects,error),"update health");
	runtime.Frame(RemasteredViewport(1280,720),7.1);
	Check(Shown(runtime,"health-rim-on") && Shown(runtime,"health-fill-low") && !Shown(runtime,"health-fill-sp"),"low health shows the rim and red fill");

	// The hit indicator: 0.70 to nothing over 500 ms, rotated toward the source.
	Check(runtime.SetState({{"hitdir",90.0}},error,7.2),error.c_str());
	Check(runtime.RunEvent("playerHit",7.2,effects,error),"player hit");
	host.draws.clear();
	runtime.Frame(RemasteredViewport(1280,720),7.24);
	const auto fade = runtime.PresentedValue("hit-fade","opacity");
	const auto turn = runtime.PresentedValue("hit","transform");
	if (fade && turn) std::fprintf(stderr,"hit-fade opacity %.3f, hit rotation %.1f, draws %zu, composites %zu\n",fade->data[0],turn->data[4],host.draws.size(),host.compositeOpacities.size());
	Check(fade && fade->data[0] > .55 && fade->data[0] < .7,"the hit indicator flashes from 0.70");
	Check(turn && Near(float(turn->data[4]),-90,.01f),"the hit indicator turns toward the source");
	runtime.Frame(RemasteredViewport(1280,720),7.8);
	Check(runtime.PresentedValue("hit-fade","opacity")->data[0] < .01,"the hit indicator is gone after 500 ms");

	// A ride (the flatbed, the tram) replaces the three gauges with its silhouette gauge.
	const char* healthKey = strogg ? "tram_healthpct" : "vehicle_health";
	Check(runtime.SetState({{healthKey,0.5}},error,7.9),error.c_str());
	Check(runtime.RunEvent(strogg ? "enterTram" : "enterFlatbed",7.9,effects,error),"boarding a ride runs");
	runtime.Frame(RemasteredViewport(1280,720),7.95);
	Check(Shown(runtime,"ride") && !Shown(runtime,"health-gauge") && !Shown(runtime,"ammo-gauge"),"a ride's gauge replaces the gauges");
	Bounds ride;
	Check(runtime.GetBounds("ride",ride) && Near(ride.x+ride.width/2,640,1.5f),"the ride gauge centres on the bottom edge");
	const auto rideShift = runtime.PresentedValue("ride-win","transform");
	Check(rideShift && Near(float(rideShift->data[0]),(strogg ? 119-105*.5f : 120-115*.5f)*1.5f,.1f),"the silhouette fills to half from the trailing end");
	Check(runtime.RunEvent(strogg ? "leaveTram" : "leaveFlatbed",8,effects,error),"leaving the ride runs");
	runtime.Frame(RemasteredViewport(1280,720),8.05);
	Check(!Shown(runtime,"ride") && Shown(runtime,"health-gauge"),"leaving restores the gauges");

	// Minimal (section 13.9): the gauges stay where they are; the strip,
	// pickups, radio, boss bar, notices and damage direction go.
	RemasteredLayoutPreference(host,2);
	runtime.Frame(RemasteredViewport(1280,720),8.1);
	Bounds minimalHealth;
	Check(Shown(runtime,"health-gauge") && Shown(runtime,"ammo-gauge") && runtime.GetBounds("health-gauge",minimalHealth) &&
		Near(minimalHealth.x,health.x) && Near(minimalHealth.y,health.y),"Minimal keeps the gauges in place");
	Check(!Shown(runtime,"bc-full") && !Shown(runtime,"tr-full") && !Shown(runtime,"tc-full") && !Shown(runtime,"cc-full"),
		"Minimal leaves out the strip, pickups, radio, boss bar, notices and damage direction");
	RemasteredLayoutPreference(host,1);
	runtime.Frame(RemasteredViewport(1280,720),8.15);
	Check(Shown(runtime,"bc-full") && Shown(runtime,"tr-full") && Shown(runtime,"tc-full") && Shown(runtime,"cc-full"),
		"Remastered brings them back");
	// Competitive (13.9) is a multiplayer preset: there the status bar grows by
	// 15% about the bottom edge; single player keeps the Remastered size.
	RemasteredLayoutPreference(host,3);
	runtime.Frame(RemasteredViewport(1280,720),8.16);
	const auto single = runtime.PresentedValue("anchor-bc","transform");
	Check(single && Near(float(single->data[2]),1,.001f) && Shown(runtime,"bc-full"),"single player keeps the Remastered status bar");
	Check(runtime.SetState({{"mp",true}},error,8.17),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),8.18);
	const auto enlarged = runtime.PresentedValue("anchor-bc","transform");
	Check(enlarged && Near(float(enlarged->data[2]),1.15f,.001f) && Near(float(enlarged->data[1]),360*(1-1.15f),.05f),
		"Competitive grows the multiplayer status bar by 15% about the bottom edge");
	Check(runtime.SetState({{"mp",false}},error,8.19),error.c_str());
	RemasteredLayoutPreference(host,1);
	runtime.Frame(RemasteredViewport(1280,720),8.2);

	// The Awakening's m03 script also sends the ship panel to the player's HUD.
	if (id.find(".awakening") != std::string::npos) {
		Check(!Shown(runtime,"ship"),"the ship panel waits for the script");
		Check(runtime.RunEvent("ship_target_01_on",8.2,effects,error),"a marker reaches the player's HUD");
		runtime.Frame(RemasteredViewport(1280,720),8.25);
		Bounds cone;
		Check(Shown(runtime,"ship") && Shown(runtime,"ship-target1") && !runtime.GetBounds("ship-cone",cone),
			"on foot the panel appears without the gun's view cone");
	}
	Check(host.errors == 0,"no runtime diagnostics across the HUD exercise");
	runtime.Shutdown();
}

// The multiplayer peer (ui-visual-design.md 14.9): the stock mphud keys and
// events plus the game's Remastered fields drive the kill feed, standing,
// team panels, timer, center column, chat, vote and follow card.
static void CheckRemasteredMpHud(ScreenHost& host, const char* path) {
	const auto source = Read(path);
	Document document; std::vector<Diagnostic> diagnostics;
	if (!document.Load(source,diagnostics)) for (const auto& d : diagnostics) std::fprintf(stderr,"%s %u:%u %s\n",path,unsigned(d.line),unsigned(d.column),d.message.c_str());
	Check(diagnostics.empty() && document.Model().id == "openq4.hud.remastered.mp","Remastered MP HUD validates");
	RemasteredLayoutPreference(host,1);
	Runtime runtime(host);
	Check(runtime.Initialize(),"retained runtime");
	Check(runtime.LoadDocument(source,"guis/hud/remastered_mp.q4ui",diagnostics),"Remastered MP HUD loads into a runtime");
	std::string error;
	Runtime::EventEffects effects;
	StateValues values = RemasteredLayout(1280,720);
	values.insert({{"gametype",1.0},{"oq4_state",3.0},{"oq4_timed",true},{"oq4_clock_warn",0.0},{"oq4_clock",std::string("7:05")}});
	Check(runtime.SetState(values,error,0),error.c_str());
	Check(runtime.RunEvent("hudJoin",0,effects,error),"the join program runs");
	runtime.Frame(RemasteredViewport(1280,720),.1);
	Check(host.errors == 0,"the MP HUD frames without runtime errors");

	// Kill feed: trailing at 637 u in the top-trailing anchor (box left 320 dp at 16:9).
	Check(runtime.SetState({{"oq4_feed0_live",true},{"oq4_feed0_alpha",1.0},{"oq4_feed0_attacker",std::string("Vex")},
		{"oq4_feed0_victim",std::string("Kaz")},{"oq4_feed0_weapon",7.0},{"oq4_feed0_kind",0.0},{"oq4_feed0_hasAttacker",true}},error,.2),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),.25);
	Bounds line;
	Check(Shown(runtime,"feed0") && runtime.GetBounds("feed0-c",line) && Near(line.x+line.width,320+637*1.5f,1.5f),
		"a kill feed line trails at 633 u on its plate");
	Check(Shown(runtime,"feed0-w7") && !Shown(runtime,"feed0-w6") && Shown(runtime,"feed0-a") && Shown(runtime,"feed0-plate") &&
		!Shown(runtime,"feed0-band"),"the line shows the attacker and the weapon on the dark plate");
	Check(Near(line.y,(24-11.5f)*1.5f,1.f),"the first line sits at the stock baseline");
	Check(runtime.SetState({{"oq4_feed0_kind",1.0},{"oq4_feed0_hasAttacker",false},{"oq4_feed0_you",true},{"oq4_feed0_youVictim",true}},error,.3),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),.35);
	Check(Shown(runtime,"feed0-skull") && !Shown(runtime,"feed0-w7") && !Shown(runtime,"feed0-a"),"a suicide shows the skull and no attacker");
	Check(Shown(runtime,"feed0-band") && Shown(runtime,"feed0-marker") && !Shown(runtime,"feed0-plate"),"your line uses the band plate and the marker");
	const auto victimColour = runtime.PresentedValue("feed0-v","color");
	Check(victimColour && Near(float(victimColour->data[0]),1) && Near(float(victimColour->data[2]),1),"your name is white");
	Check(runtime.SetState({{"gametype",2.0}},error,.4),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),.45);
	Check(runtime.GetBounds("feed0-c",line) && Near(line.y,(70-11.5f)*1.5f,1.f),"the Tourney feed sits under the strip");
	Check(runtime.SetState({{"gametype",1.0},{"oq4_feed0_live",false}},error,.5),error.c_str());

	// Standing: the leader and you, your gap, the limit.
	Check(runtime.SetState({{"player1_name",std::string("Sarge")},{"player1_score",std::string("24")},{"oq4_rank1",std::string("1ST")},
		{"player2_name",std::string("You")},{"player2_score",std::string("19")},{"oq4_rank2",std::string("3RD")},{"oq4_row2_you",true},
		{"oq4_limit",30.0},{"oq4_gap",-5.0},{"oq4_gap_show",true}},error,.6),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),.65);
	Bounds row1, row2;
	Check(Shown(runtime,"standing") && runtime.GetBounds("standing1",row1) && runtime.GetBounds("standing2",row2) &&
		Near(row1.x,6*1.5f) && Near(row1.y,36*1.5f) && Near(row2.y-row1.y,21*1.5f),"the standing plates sit from y 36, 3 u apart");
	Check(Shown(runtime,"standing2-band") && Shown(runtime,"standing2-marker") && !Shown(runtime,"standing1-band"),"your row carries the band and marker");
	Check(Shown(runtime,"standing2-gap") && !Shown(runtime,"standing1-gap") && Shown(runtime,"standing1-limit"),"your gap and the limit show");
	const auto sign = runtime.PresentedValue("standing2-sign","text");
	Check(sign && sign->text == "#str_231224","a deficit reads with the minus sign key");
	const auto lead = runtime.PresentedValue("standing1-rank","color");
	Check(lead && Near(float(lead->data[0]),1) && Near(float(lead->data[2]),0x23/255.f,.01f),"the leader ordinal is #FFBE23");
	Bounds clock;
	Check(runtime.GetBounds("timer",clock) && Near(clock.y,82*1.5f),"the timer sits under the standing");

	// Team panels: your team first.
	Check(runtime.SetState({{"gametype",4.0},{"oq4_team_game",true},{"oq4_my_team",1.0},{"marine_score",3.0},{"strogg_score",2.0},
		{"oq4_flag0_state",1.0},{"oq4_flag0_carrier",std::string("Vex")},{"oq4_flag0_carrier_team",1.0}},error,.7),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),.75);
	Bounds marine, strogg;
	Check(Shown(runtime,"teams") && !Shown(runtime,"standing") && runtime.GetBounds("team0",marine) && runtime.GetBounds("team1",strogg) &&
		Near(strogg.y,36*1.5f) && Near(marine.y,61*1.5f),"your team panel leads");
	Check(Shown(runtime,"team0-flag-pulse") && Shown(runtime,"team0-flag-bang") && Shown(runtime,"team0-carrier") &&
		Shown(runtime,"team1-flag-base"),"a taken flag pulses under ! with its carrier");
	Check(runtime.GetBounds("timer",clock) && Near(clock.y,90*1.5f),"the timer sits under the team panels");

	// Timer: the last minute in #FFBE23, the last ten seconds pulsing #E46D56.
	Check(runtime.SetState({{"oq4_clock_warn",1.0},{"oq4_clock",std::string("0:42")}},error,.8),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),.85);
	const auto minute = runtime.PresentedValue("timer-clock","color");
	Check(Shown(runtime,"timer-clock") && minute && Near(float(minute->data[2]),0x23/255.f,.01f),"the last minute reads #FFBE23");
	Check(runtime.SetState({{"oq4_clock_warn",2.0},{"oq4_clock",std::string("0:08")}},error,.9),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),.95);
	Check(Shown(runtime,"timer-urgent") && !Shown(runtime,"timer-clock"),"the last ten seconds pulse");
	Check(runtime.SetState({{"oq4_state",1.0},{"showready",true},{"readytext",std::string("Ready 3/8")}},error,1.0),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),1.05);
	Check(Shown(runtime,"timer-warm") && Shown(runtime,"timer-ready") && !Shown(runtime,"timer-clock"),"warm-up shows the ready count");
	Check(runtime.SetState({{"oq4_state",3.0}},error,1.1),error.c_str());

	// Center column: notices stack and leave after 2500 ms.
	Check(runtime.SetState({{"frag_notice_text",std::string("You fragged Vex")},{"main_notice_text",std::string("3rd place with 19")},
		{"oq4_rank_notice_text",std::string("3rd place with 19")},{"oq4_rank_notice_lead",std::string("3rd")},
		{"oq4_rank_notice_post",std::string(" place with 19")}},error,1.2),error.c_str());
	Check(runtime.RunEvent("frag_notice",1.2,effects,error) && runtime.RunEvent("main_notice",1.2,effects,error),"notices run");
	runtime.Frame(RemasteredViewport(1280,720),1.3);
	Bounds frag, main;
	Check(Shown(runtime,"frag") && Shown(runtime,"main") && runtime.GetBounds("frag",frag) && runtime.GetBounds("main",main) &&
		main.y >= frag.y+frag.height,"the main notice stacks under the frag notice");
	Check(Shown(runtime,"main-lead") && !Shown(runtime,"main-text"),"a rank notice sets its ordinal apart");
	Check(Near(frag.x+frag.width/2,640,1.5f),"notices are centred");
	runtime.Frame(RemasteredViewport(1280,720),3.8);
	Check(!Shown(runtime,"frag") && !Shown(runtime,"main"),"notices leave after 2500 ms");

	// Chat: a line shows on its plate and steps aside while typing.
	Check(runtime.SetState({{"oq4_chat0_live",true},{"oq4_chat0_alpha",1.0},{"oq4_chat0_name",std::string("Kaz")},
		{"oq4_chat0_text",std::string("rail on the bridge")},{"oq4_chat0_kind",1.0},{"oq4_chat0_team",0.0}},error,3.9),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),3.95);
	Bounds chat;
	Check(Shown(runtime,"chat0") && Shown(runtime,"chat0-tag") && runtime.GetBounds("chat0",chat) && Near(chat.x,8*1.5f,1.f) &&
		Near(chat.y+chat.height,720-59*1.5f,2.f),"a team chat line sits on the leading edge clear of the status bar");
	Check(runtime.SetState({{"oq4_chatting",true}},error,4.0),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),4.05);
	Check(!Shown(runtime,"chat0"),"the lines step aside while the input is open");

	// Vote and follow card.
	Check(runtime.SetState({{"voteNotice",1.0},{"voteInfo_1",std::string("Map: q4dm7")},{"oq4_vote_yes",3.0},{"oq4_spectating",true},
		{"oq4_following",true},{"oq4_follow_name",std::string("Vex")}},error,4.1),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),4.15);
	Check(Shown(runtime,"vote") && Shown(runtime,"vote-l1") && !Shown(runtime,"vote-l2"),"the vote card lists its fields");
	Check(Shown(runtime,"follow") && Shown(runtime,"follow-name") && !Shown(runtime,"spec-free"),"the follow card names the followed player");
	Check(runtime.SetState({{"oq4_key_prev",std::string("MWHEELDOWN")}},error,4.16),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),4.17);
	const auto prev = runtime.PresentedValue("pr-prev-key","text");
	Check(Shown(runtime,"pr-next") && Shown(runtime,"pr-prev") && Shown(runtime,"pr-free") && prev && prev->text == "MWHEELDOWN",
		"following, the prompts read Next, Previous and Free camera");

	// Free camera: name tags over the players in view, centred on their points.
	Check(runtime.SetState({{"oq4_following",false},{"oq4_tags",2.0},{"oq4_tag0_x",0.5},{"oq4_tag0_y",0.4},
		{"oq4_tag0_name",std::string("Nyx")},{"oq4_tag0_team",1.0},{"oq4_tag1_x",0.25},{"oq4_tag1_y",0.5},
		{"oq4_tag1_name",std::string("Orin")},{"oq4_tag1_team",-1.0}},error,4.18),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),4.19);
	Bounds tag, pointer;
	const auto tagName = runtime.PresentedValue("tag0-name","text");
	Check(Shown(runtime,"tag0") && Shown(runtime,"tag1") && !Shown(runtime,"tag2") && tagName && tagName->text == "Nyx",
		"the free camera tags the players in view");
	// The holder moves by transform (bounds are its layout box): the plate's
	// centre and the pointer's tip land on the projected point.
	const auto place = runtime.PresentedValue("tag0","transform");
	Check(place && runtime.GetBounds("tag0-plate",tag) && runtime.GetBounds("tag0-point",pointer) && Near(tag.height,13*1.5f,1.f) &&
		Near(float(place->data[0])+tag.x+tag.width/2,640,1.5f) && Near(float(place->data[1])+pointer.y+pointer.height,.4f*720,1.5f),
		"a tag centres over its player with its pointer's tip on the point");
	Check(Shown(runtime,"tag0-team1") && !Shown(runtime,"tag0-team0") && !Shown(runtime,"tag1-team0") && !Shown(runtime,"tag1-team1"),
		"a team tag carries its team's block; deathmatch carries none");
	Check(Shown(runtime,"spec-free") && !Shown(runtime,"follow-name"),"the card reads Free camera");
	Check(runtime.SetState({{"oq4_following",true}},error,4.195),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),4.197);
	Check(!Shown(runtime,"tag0"),"following, the tags go");

	// Respawn: the attack control's Respawn in the lower third, fading in as it
	// appears; while overtime holds the respawn, the seconds left instead.
	Check(runtime.SetState({{"oq4_spectating",false},{"oq4_following",false},{"oq4_respawn",1.0},
		{"oq4_key_attack",std::string("MOUSE1")}},error,4.2),error.c_str());
	Check(runtime.RunEvent("oq4_respawn",4.2,effects,error),"the respawn prompt appears");
	runtime.Frame(RemasteredViewport(1280,720),4.25);
	Bounds respawn;
	const auto rising = runtime.PresentedValue("respawn-c","opacity");
	Check(rising && float(rising->data[0]) < .5f,"the respawn prompt fades in");
	runtime.Frame(RemasteredViewport(1280,720),4.5);
	const auto risen = runtime.PresentedValue("respawn-c","opacity");
	Check(risen && Near(float(risen->data[0]),1),"the respawn prompt settles opaque");
	Check(Shown(runtime,"respawn") && Shown(runtime,"respawn-ready") && !Shown(runtime,"respawn-held") && !Shown(runtime,"follow"),
		"dead, the attack control's Respawn shows instead of the follow card");
	const auto key = runtime.PresentedValue("respawn-key","text");
	Check(key && key->text == "MOUSE1","the prompt names the attack control");
	Check(runtime.GetBounds("respawn-c",respawn) && Near(respawn.x+respawn.width/2,640,1.5f) && respawn.y > 720*2/3.f &&
		respawn.y+respawn.height < 720-51*1.5f,"the prompt is centred in the lower third, clear of the status bar");
	Check(runtime.SetState({{"oq4_respawn",2.0},{"oq4_respawn_in",4.0}},error,4.6),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),4.65);
	const auto seconds = runtime.PresentedValue("respawn-in","text");
	Check(Shown(runtime,"respawn-held") && !Shown(runtime,"respawn-ready") && seconds && seconds->text == "4",
		"a held respawn counts down its seconds");
	Check(runtime.SetState({{"oq4_respawn",0.0}},error,4.7),error.c_str());
	Check(runtime.RunEvent("oq4_respawn",4.7,effects,error),"the respawn prompt leaves");
	runtime.Frame(RemasteredViewport(1280,720),4.75);
	Check(!Shown(runtime,"respawn"),"alive again, the prompt goes");

	// Minimal: the standings, timer, notices, kill feed and aim text go;
	// the vote, chat and the respawn prompt stay.
	RemasteredLayoutPreference(host,2);
	Check(runtime.SetState({{"oq4_respawn",1.0}},error,4.8),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),4.85);
	Check(!Shown(runtime,"tl-full") && !Shown(runtime,"tc-full") && !Shown(runtime,"tr-full") && !Shown(runtime,"cc-full"),
		"Minimal leaves out the standings, timer, notices, kill feed and aim text");
	Check(Shown(runtime,"vote") && Shown(runtime,"respawn") && Shown(runtime,"chatbox"),"Minimal keeps the vote, chat and the respawn prompt");
	RemasteredLayoutPreference(host,1);
	Check(runtime.SetState({{"oq4_respawn",0.0}},error,4.9),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),4.95);
	Check(Shown(runtime,"tl-full") && Shown(runtime,"tr-full"),"Remastered brings them back");
	Check(host.errors == 0,"no runtime diagnostics across the MP HUD exercise");
	runtime.Shutdown();
}

// Competitive (ui_hudLayout 3; 13.9, 14.9): the match bar at the top center,
// network quality beside it, the team status at the leading edge, the vote in
// the top-leading corner and the chat clear of the enlarged status bar.
static void CheckRemasteredCompetitiveHud(ScreenHost& host, const char* path) {
	const auto source = Read(path);
	std::vector<Diagnostic> diagnostics;
	RemasteredLayoutPreference(host,3);
	Runtime runtime(host);
	Check(runtime.Initialize(),"retained runtime");
	Check(runtime.LoadDocument(source,"guis/hud/remastered_mp.q4ui",diagnostics),"Remastered MP HUD loads for Competitive");
	std::string error;
	Runtime::EventEffects effects;
	StateValues values = RemasteredLayout(1280,720);
	values.insert({{"gametype",1.0},{"oq4_state",3.0},{"oq4_timed",true},{"oq4_clock_warn",0.0},{"oq4_clock",std::string("7:05")},
		{"oq4_mb_sub",std::string("TO 30")},{"oq4_mb0_name",std::string("You")},{"oq4_mb0_rank",std::string("2ND")},{"oq4_mb0_score",19.0},
		{"oq4_mb0_you",true},{"oq4_mb1_name",std::string("Sarge")},{"oq4_mb1_rank",std::string("1ST")},{"oq4_mb1_score",24.0},
		{"oq4_mb1_lead",true},{"oq4_ping",40.0},{"oq4_loss",0.0},{"player1_name",std::string("Sarge")},{"oq4_rank1",std::string("1ST")}});
	Check(runtime.SetState(values,error,0),error.c_str());
	Check(runtime.RunEvent("hudJoin",0,effects,error),"the join program runs");
	runtime.Frame(RemasteredViewport(1280,720),.1);
	Check(host.errors == 0,"Competitive frames without runtime errors");

	// The bar: three plates from y 6, 26 u tall, 2 u apart, a 60 u timer
	// between 122 u sides (the top-center box starts at 160 dp at 16:9).
	Bounds side0, timer, side1;
	Check(Shown(runtime,"matchbar") && !Shown(runtime,"tl-rem") && !Shown(runtime,"tc-strip"),
		"Competitive trades the standing, timer and strip for the match bar");
	Check(runtime.GetBounds("mb0-plate",side0) && runtime.GetBounds("mbt-plate",timer) && runtime.GetBounds("mb1-plate",side1),
		"the bar's plates are laid out");
	Check(Near(side0.x,160+166*1.5f) && Near(side0.width,122*1.5f) && Near(side0.y,6*1.5f) && Near(side0.height,26*1.5f) &&
		Near(timer.x,160+290*1.5f) && Near(timer.width,60*1.5f) && Near(side1.x,160+352*1.5f),"the sides flank the 60 u timer 2 u apart");
	// Deathmatch: the rank ordinal on the band, the leader's in #FFBE23, the
	// name and your marker; the scores at the inner ends in Chain.
	Check(Shown(runtime,"mb0-band") && Shown(runtime,"mb0-rank") && Shown(runtime,"mb0-name") && !Shown(runtime,"mb0-block0") &&
		!Shown(runtime,"mb0-block1") && !Shown(runtime,"mb0-team"),"deathmatch sides hold the rank and the name");
	Check(Shown(runtime,"mb0-marker") && !Shown(runtime,"mb1-marker"),"you lead with the marker");
	const auto leader = runtime.PresentedValue("mb1-rank","color"), you = runtime.PresentedValue("mb0-rank","color");
	Check(leader && Near(float(leader->data[2]),0x23/255.f,.01f) && you && Near(float(you->data[2]),1),"the leader's ordinal is #FFBE23");
	const auto score0 = runtime.PresentedValue("mb0-score","text"), score1 = runtime.PresentedValue("mb1-score","text");
	Check(score0 && score0->text == "19" && score1 && score1->text == "24","each side shows its score");
	Bounds score;
	Check(runtime.GetBounds("mb0-score",score) && Near(score.x+score.width,160+(166+122-7)*1.5f,1.5f),"the score sits at the inner end");
	// The timer: the time over the limit.
	const auto clock = runtime.PresentedValue("mbt-clock","text"), sub = runtime.PresentedValue("mbt-sub","text");
	Check(Shown(runtime,"mbt-clock") && clock && clock->text == "7:05" && Shown(runtime,"mbt-sub") && sub && sub->text == "TO 30",
		"the timer reads the time over the limit");
	Check(runtime.SetState({{"oq4_clock_warn",2.0},{"oq4_clock",std::string("0:08")}},error,.2),error.c_str());
	Check(runtime.RunEvent("oq4_clockWarn",.2,effects,error),"the clock warning runs");
	runtime.Frame(RemasteredViewport(1280,720),.25);
	Check(Shown(runtime,"mbt-urgent") && !Shown(runtime,"mbt-clock"),"the last ten seconds pulse in the bar");
	Check(runtime.SetState({{"oq4_clock_warn",0.0},{"oq4_state",1.0},{"showready",true},{"oq4_mb_ready",std::string("READY 3/8")}},error,.3),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),.35);
	const auto ready = runtime.PresentedValue("mbt-sub","text");
	Check(Shown(runtime,"mbt-warm") && ready && ready->text == "READY 3/8","warm-up reads WARM-UP over the ready count");
	Check(runtime.SetState({{"oq4_state",3.0},{"showready",false}},error,.4),error.c_str());

	// The kill feed keeps its place beside the bar at 16:9 and steps 38 u
	// under it on narrower views, where its longest line would meet the
	// network readout.
	Check(runtime.SetState({{"oq4_feed0_live",true},{"oq4_feed0_alpha",1.0},{"oq4_feed0_attacker",std::string("Vex")},
		{"oq4_feed0_victim",std::string("Kaz")},{"oq4_feed0_weapon",7.0},{"oq4_feed0_kind",0.0},{"oq4_feed0_hasAttacker",true}},error,.41),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),.415);
	Bounds feedLine;
	Check(Shown(runtime,"feed0") && runtime.GetBounds("feed0-c",feedLine) && Near(feedLine.y,(24-11.5f)*1.5f,1.f),
		"the kill feed keeps its place beside the bar at 16:9");
	Check(runtime.SetState(RemasteredLayout(960,720),error,.42),error.c_str());
	runtime.Frame(RemasteredViewport(960,720),.425);
	Check(runtime.GetBounds("feed0-c",feedLine) && Near(feedLine.y,(24-11.5f+38)*1.5f,1.f),"at 4:3 the kill feed steps under the bar");
	Check(runtime.SetState(RemasteredLayout(1280,720),error,.43),error.c_str());
	Check(runtime.SetState({{"oq4_feed0_live",false}},error,.435),error.c_str());

	// Network quality follows the bar's trailing end at all times: four bars
	// by the ping, each 2% of loss removing one.
	runtime.Frame(RemasteredViewport(1280,720),.45);
	Bounds net;
	Check(Shown(runtime,"mbn") && runtime.GetBounds("mbn",net) && Near(net.x,160+480*1.5f,1.5f),"network quality follows the bar");
	const auto lit3 = runtime.PresentedValue("mbn-b3","background-color");
	Check(lit3 && Near(float(lit3->data[0]),0xB5/255.f,.01f) && Near(float(lit3->data[3]),.9f,.01f),"a 40 ms ping lights all four bars");
	Check(runtime.SetState({{"oq4_loss",5.0}},error,.5),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),.55);
	const auto lossy1 = runtime.PresentedValue("mbn-b1","background-color"), lossy2 = runtime.PresentedValue("mbn-b2","background-color");
	Check(lossy1 && Near(float(lossy1->data[0]),0xE3/255.f,.01f) && lossy2 && Near(float(lossy2->data[3]),.2f,.01f),
		"5% loss leaves two amber bars");
	Check(runtime.SetState({{"oq4_loss",0.0}},error,.6),error.c_str());

	// Team modes: your team leads with its color and mark; flags hang under
	// the bar from the inner ends; your team's list sits at the leading edge.
	Check(runtime.SetState({{"gametype",4.0},{"oq4_team_game",true},{"oq4_my_team",1.0},{"marine_score",3.0},{"strogg_score",2.0},
		{"oq4_flag1_state",1.0},{"oq4_flag1_carrier",std::string("Vex")},{"oq4_flag0_state",2.0},{"oq4_ts1_count",2.0},
		{"oq4_ts1_0_name",std::string("You")},{"oq4_ts1_0_weapon",7.0},{"oq4_ts1_0_health",125.0},{"oq4_ts1_0_armor",50.0},
		{"oq4_ts1_0_you",true},{"oq4_ts1_1_name",std::string("Kaz")},{"oq4_ts1_1_dead",true},{"oq4_ts1_1_health",0.0}},error,.7),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),.75);
	Check(Shown(runtime,"mb0-block1") && Shown(runtime,"mb0-logo1") && Shown(runtime,"mb1-block0") && Shown(runtime,"mb0-team") &&
		!Shown(runtime,"mb0-band") && !Shown(runtime,"mb0-name"),"team modes lead with your team's block, mark and name");
	const auto teamName = runtime.PresentedValue("mb0-team","text");
	const auto teamScore = runtime.PresentedValue("mb0-score","text");
	Check(teamName && teamName->text == "#str_200199" && teamScore && teamScore->text == "2","the Strogg side leads with the Strogg score");
	Bounds flag0, flag1;
	Check(Shown(runtime,"mbf0") && Shown(runtime,"mbf0-taken") && Shown(runtime,"mbf0-carrier") && Shown(runtime,"mbf0-1taken") &&
		Shown(runtime,"mbf1-dropped") && Shown(runtime,"mbf1-0dropped") && !Shown(runtime,"mbf1-taken"),
		"each side's flag hangs under the bar with its state and carrier");
	Check(runtime.GetBounds("mbf0",flag0) && runtime.GetBounds("mbf1",flag1) && Near(flag0.x+flag0.width,160+282*1.5f,1.5f) &&
		Near(flag1.x,160+358*1.5f,1.5f) && Near(flag0.y,34*1.5f,1.f),"the flags hang 6 u in from the inner ends");
	Bounds row0, row1, head;
	Check(Shown(runtime,"tsl") && !Shown(runtime,"tst") && Shown(runtime,"tsl0") && Shown(runtime,"tsl1") && !Shown(runtime,"tsl2"),
		"your team's list shows its players");
	Check(runtime.GetBounds("tsl0",row0) && runtime.GetBounds("tsl1",row1) && Near(row0.x,8*1.5f) && Near(row0.y,150*1.5f) &&
		Near(row0.width,138*1.5f) && Near(row1.y-row0.y,16.5f*1.5f),"the rows sit from y 150 on a 16.5 u pitch");
	Check(Shown(runtime,"tsl0-band") && Shown(runtime,"tsl0-marker") && Shown(runtime,"tsl0-w7") && !Shown(runtime,"tsl0-plate"),
		"your row uses the band plate, the marker and your weapon");
	const auto overcharged = runtime.PresentedValue("tsl0-hp","color");
	Check(overcharged && Near(float(overcharged->data[2]),0x23/255.f,.01f),"health above 100 reads #FFBE23");
	const auto deadName = runtime.PresentedValue("tsl1-name","opacity");
	Check(Shown(runtime,"tsl1-dead") && !Shown(runtime,"tsl1-hp") && deadName && Near(float(deadName->data[0]),.4f),
		"the dead keep their row at 0.40 under a cross");
	Check(runtime.GetBounds("tsl-head",head) && head.y+head.height <= row0.y+1,"the team name sits above the list");

	// The vote takes the top-leading corner, clear of the team status.
	Check(runtime.SetState({{"voteNotice",1.0},{"voteInfo_1",std::string("Map: q4dm7")},{"oq4_vote_yes",3.0}},error,.8),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),.85);
	Bounds ballot;
	Check(Shown(runtime,"vote") && runtime.GetBounds("vote",ballot) && Near(ballot.y,36*1.5f,1.f) && ballot.y+ballot.height < head.y,
		"the vote sits in the top-leading corner clear of the team status");
	Check(runtime.SetState({{"voteNotice",0.0}},error,.9),error.c_str());

	// Chat clears the 15% larger status bar by 8 u (21:9, where the box spans
	// the room beside it).
	Check(runtime.SetState(RemasteredLayout(2520,1080),error,1.0),error.c_str());
	Check(runtime.SetState({{"oq4_chat0_live",true},{"oq4_chat0_alpha",1.0},{"oq4_chat0_name",std::string("Kaz")},
		{"oq4_chat0_text",std::string("rail on the bridge")},{"oq4_chat0_kind",0.0}},error,1.05),error.c_str());
	runtime.Frame(RemasteredViewport(2520,1080),1.1);
	Bounds chat;
	const float px = 1080/720.f;
	Check(Shown(runtime,"chat0") && runtime.GetBounds("chat0",chat) && Near(chat.y+chat.height,1080-(51*1.15f+8)*1.5f*px,3.f),
		"chat clears the enlarged status bar by 8 u");
	// Narrower views keep the box low, under eight team rows (which end at
	// 280 u): 200 u wide at 16:9, and at 4:3 as wide as the space beside the
	// widest enlarged strip allows, down to 120 u.
	Check(runtime.SetState(RemasteredLayout(1280,720),error,1.12),error.c_str());
	StateValues lines;
	for (int i = 0; i < 4; ++i) {
		const std::string key = "oq4_chat"+std::to_string(i);
		lines.insert({{key+"_live",true},{key+"_alpha",1.0},{key+"_name",std::string("Kaz")},{key+"_text",std::string("rail on the bridge")},
			{key+"_kind",0.0}});
	}
	Check(runtime.SetState(lines,error,1.13),error.c_str());
	for (const int width : {1280,960}) {
		Check(runtime.SetState(RemasteredLayout(width,720),error,width == 1280 ? 1.14 : 1.16),error.c_str());
		runtime.Frame(RemasteredViewport(width,720),width == 1280 ? 1.15 : 1.17);
		Bounds box; float top = 720;
		for (int i = 0; i < 4; ++i) {
			Bounds line;
			if (runtime.GetBounds("chat"+std::to_string(i),line)) top = std::min(top,line.y);
		}
		const float expected = width == 1280 ? 200*1.5f : 120*1.5f;
		Check(runtime.GetBounds("chatbox",box) && Near(box.width,expected,1.f) && Near(box.y+box.height,720-(51*1.15f+8)*1.5f,2.f),
			width == 1280 ? "at 16:9 the box stays 8 u above the enlarged status bar, 200 u wide" :
				"at 4:3 the box narrows beside the enlarged strip instead of rising");
		Check(top > 280*1.5f,"four chat lines stay under eight team rows");
	}
	Check(runtime.SetState(RemasteredLayout(1280,720),error,1.2),error.c_str());

	// Spectating: both lists flank the view, the followed row lit; held
	// statistics take the trailing list's place. Tags add a health line.
	Check(runtime.SetState({{"oq4_spectating",true},{"oq4_following",true},{"oq4_follow_name",std::string("Nyx")},{"oq4_my_team",1.0},
		{"oq4_ts1_0_you",false},{"oq4_ts1_0_followed",true},{"oq4_ts0_count",1.0},{"oq4_ts0_0_name",std::string("Orin")},
		{"oq4_ts0_0_health",80.0},{"oq4_ts0_0_weapon",1.0}},error,1.3),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),1.35);
	Bounds trailing;
	const auto leadingName = runtime.PresentedValue("tsl0-name","text");
	Check(Shown(runtime,"tsl") && Shown(runtime,"tst") && leadingName && leadingName->text == "Orin","spectators see both teams, the Marines leading");
	// The trailing anchor's box ends at the view's edge (box left 320 dp at 16:9).
	Check(runtime.GetBounds("tst0",trailing) && Near(trailing.x+trailing.width,320+(494+138)*1.5f,1.5f),"the Strogg list trails 8 u from the edge");
	Check(Shown(runtime,"tst0-lit1") && Shown(runtime,"tst0-marker") && !Shown(runtime,"tst0-band"),"the followed row is lit with the marker");
	Check(runtime.SetState({{"stat_visible",1.0}},error,1.4),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),1.45);
	Check(!Shown(runtime,"tst") && Shown(runtime,"tsl"),"held statistics take the trailing list's place");
	Check(runtime.SetState({{"stat_visible",0.0},{"oq4_following",false},{"oq4_tags",1.0},{"oq4_tag0_x",0.5},{"oq4_tag0_y",0.4},
		{"oq4_tag0_name",std::string("Nyx")},{"oq4_tag0_team",1.0},{"oq4_tag0_health",60.0}},error,1.5),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),1.55);
	Bounds track, fill;
	Check(Shown(runtime,"tag0") && Shown(runtime,"tag0-hp") && runtime.GetBounds("tag0-hp",track) && runtime.GetBounds("tag0-hp-fill",fill) &&
		track.width > 0 && Near(fill.width,track.width*.6f,1.f),"a Competitive tag's health line fills to the health");

	// Remastered drops the bar, the lists and the tags' health line.
	RemasteredLayoutPreference(host,1);
	runtime.Frame(RemasteredViewport(1280,720),1.6);
	Check(!Shown(runtime,"matchbar") && !Shown(runtime,"tsl") && !Shown(runtime,"tst") && !Shown(runtime,"tag0-hp") && Shown(runtime,"tl-rem"),
		"Remastered returns the standing and drops the Competitive groups");
	Check(host.errors == 0,"no runtime diagnostics across the Competitive exercise");
	runtime.Shutdown();
}

// The vehicle HUD (guis/vehicles/hud.gui, 14.12): the centred status bar,
// its warnings, the entry lines, the hit flash and the HUD prompt.
static void CheckRemasteredVehicleHud(ScreenHost& host, const char* path) {
	const auto source = Read(path);
	Document document; std::vector<Diagnostic> diagnostics;
	if (!document.Load(source,diagnostics)) for (const auto& d : diagnostics) std::fprintf(stderr,"%s %u:%u %s\n",path,unsigned(d.line),unsigned(d.column),d.message.c_str());
	Check(diagnostics.empty() && document.Model().id == "openq4.hud.remastered.vehicle","Remastered vehicle HUD validates");
	RemasteredLayoutPreference(host,1);
	Runtime runtime(host);
	Check(runtime.Initialize(),"retained runtime");
	Check(runtime.LoadDocument(source,"guis/hud/remastered_vehicle.q4ui",diagnostics),"Remastered vehicle HUD loads into a runtime");
	std::string error;
	Runtime::EventEffects effects;
	StateValues values = RemasteredLayout(1280,720);
	values.insert({{"vehicle_health",0.8},{"vehicle_shield",0.55},{"vehicle_weapon",3.0},{"vehicle_weaponammo",6.0},
		{"vehicle_weaponcharge",0.5},{"vehicle_locked",0.0},{"oq4_key_jump",std::string("SPACE")}});
	Check(runtime.SetState(values,error,0),error.c_str());
	Check(runtime.RunEvent("hudJoin",0,effects,error),"the join program runs");
	runtime.Frame(RemasteredViewport(1280,720),.1);
	Check(host.errors == 0,"the vehicle HUD frames without runtime errors");

	// The status bar spans 11-288 u in the stock and centres on 320 u: the
	// hull gauge sits at 241.5 u, the shield gauge at 334.5 u, both at y 420.
	Bounds hull, shield, panel;
	Check(runtime.GetBounds("vhull-gauge",hull) && Near(hull.x,160+241.5f*1.5f,1.f) && Near(hull.y,420*1.5f,1.f),
		"the hull gauge sits on the centred status bar");
	Check(runtime.GetBounds("vshield-gauge",shield) && Near(shield.x-hull.x,93*1.5f,1.f),"the shield gauge follows 93 u on");
	Check(Shown(runtime,"vweap3") && !Shown(runtime,"vweap2") && Shown(runtime,"vcount") && Shown(runtime,"vcharge"),
		"the rocket pods show their glyph, count and charge bar");
	const auto count = runtime.PresentedValue("vcount-text","text");
	Check(count && count->text == "6","the rocket count reads the ammunition");
	Check(Shown(runtime,"vhull-track") && !Shown(runtime,"vhull-track-warn") && !Shown(runtime,"vshield-label-warn"),
		"healthy gauges keep their own colours");
	Check(runtime.SetState({{"vehicle_weapon",0.0}},error,.2),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),.25);
	Check(Shown(runtime,"vweap0") && !Shown(runtime,"vcount") && Shown(runtime,"vcharge"),"the cannon shows its charge but no count");
	Check(runtime.SetState({{"vehicle_weapon",1.0}},error,.3),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),.35);
	Check(Shown(runtime,"vweap1") && !Shown(runtime,"vcharge"),"the machinegun has no charge bar");
	Check(runtime.SetState({{"vehicle_health",0.2},{"vehicle_shield",0.0}},error,.4),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),.45);
	Check(Shown(runtime,"vhull-track-warn") && Shown(runtime,"vhull-fill-warn") && Shown(runtime,"vhull-label-warn") &&
		Shown(runtime,"vshield-track-warn") && Shown(runtime,"vshield-icon-warn"),"a quarter hull and no shield turn their gauges #FF3300");
	// The fill drains toward the trailing end: its window moves, the art stays.
	const auto shift = runtime.PresentedValue("vhull-win","transform");
	Check(shift && Near(float(shift->data[0]),(119.6f-79*0.2f)*1.5f,0.1f),"the hull fill's leading edge sits at 20% from the trailing end");
	// The drawn fill follows the window (RmlUi clips only overflowing content):
	// at half hull the leading half of the track carries no fill.
	const auto hullFillLead = [&](double pct, double at) {
		Check(runtime.SetState({{"vehicle_health",pct},{"vehicle_shield",0.55}},error,at),error.c_str());
		host.draws.clear(); runtime.Frame(RemasteredViewport(1280,720),at+.005);
		float lo = 1e9f;
		for (const auto& draw : host.draws) for (const auto& v : draw.vertices) {
			if (v.a < .3f || v.a > .4f || std::fabs(v.r/v.a-.702f) > .03f || v.x < hull.x || v.x > hull.x+hull.width) continue;
			lo = std::min(lo,v.x);
		}
		return lo;
	};
	const float fullLead = hullFillLead(1.0,.46), halfLead = hullFillLead(0.5,.47);
	Check(fullLead < hull.x+hull.width && halfLead > fullLead+.3f*hull.width,"a half hull draws only the trailing half of its fill");

	// Entry: the lines begin 2 s after showExitmessage; the prompt shows Exit.
	Check(runtime.SetState({{"vehicle_weapon",3.0},{"vehicle_health",0.8},{"vehicle_shield",0.55}},error,.5),error.c_str());
	Check(runtime.RunEvent("showExitmessage",.5,effects,error),"entering runs");
	runtime.Frame(RemasteredViewport(1280,720),.6);
	Check(Shown(runtime,"vmsg") && Shown(runtime,"vpr") && Shown(runtime,"vpr-exit") && !Shown(runtime,"vpr-lock"),
		"entering shows the lines' group and the Exit prompt");
	const auto line1 = runtime.PresentedValue("vmsg1-text","text"), line2 = runtime.PresentedValue("vmsg2-text","text");
	Check(line1 && line1->text == "#str_200942" && line2 && line2->text == "#str_200941","the rocket pods' lines name the other weapon first");
	const auto closed = runtime.PresentedValue("vmsg1-win","transform");
	Check(closed && Near(float(closed->data[0]),-348*1.5f,0.5f),"the first line is closed before 2 s");
	runtime.Frame(RemasteredViewport(1280,720),.5+2.3);
	const auto open = runtime.PresentedValue("vmsg1-win","transform");
	Check(open && Near(float(open->data[0]),0,0.5f),"the first line has wiped open at 2.25 s");
	const auto key = runtime.PresentedValue("vpr-key","text");
	Check(key && key->text == "SPACE","the prompt names the jump key");
	Bounds prompt;
	Check(runtime.GetBounds("vpr",prompt) && Near(prompt.x+prompt.width,320+629*1.5f,1.5f),"the prompt trails at 629 u");
	runtime.Frame(RemasteredViewport(1280,720),.5+5.6);
	Check(!Shown(runtime,"vmsg"),"the lines are gone after 5.5 s");
	Check(runtime.SetState({{"vehicle_locked",1.0}},error,6.2),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),6.25);
	Check(Shown(runtime,"vpr-lock") && !Shown(runtime,"vpr-exit"),"a locked vehicle shows the lock instead of Exit");

	// Hits and electrical damage.
	Check(runtime.RunEvent("vehicleHit",6.3,effects,error),"a hit runs");
	runtime.Frame(RemasteredViewport(1280,720),6.35);
	const auto flash = runtime.PresentedValue("vhit","opacity");
	Check(flash && flash->data[0] > .8 && flash->data[0] < 1,"a hit flashes the edges");
	Bounds hitLeft, hitRight, hitTop, hitBottom;
	Check(runtime.GetBounds("vhit-l",hitLeft) && runtime.GetBounds("vhit-r",hitRight) && runtime.GetBounds("vhit-t",hitTop) &&
		runtime.GetBounds("vhit-b",hitBottom) && Near(hitLeft.x,0) && Near(hitRight.x+hitRight.width,1280,1.f) && Near(hitTop.y,0) &&
		Near(hitBottom.y+hitBottom.height,720,1.f) && Near(hitRight.width,0.22f*640*1.5f,1.f),"the flash's strips hold all four edges of the view");
	runtime.Frame(RemasteredViewport(1280,720),6.9);
	const auto gone = runtime.PresentedValue("vhit","opacity");
	Check(gone && gone->data[0] < 1e-3,"the flash is gone after 500 ms");
	Check(runtime.RunEvent("electricWarningOn",7,effects,error),"electrical damage runs");
	runtime.Frame(RemasteredViewport(1280,720),7.05);
	Check(Shown(runtime,"velec-l-on") && Shown(runtime,"velec-r-on"),"electrical damage shows both side warnings");
	Check(runtime.RunEvent("electricWarningOff",7.1,effects,error) && runtime.RunEvent("hideExitmessage",7.1,effects,error),"leaving runs");
	runtime.Frame(RemasteredViewport(1280,720),7.15);
	Check(!Shown(runtime,"velec-l-on") && !Shown(runtime,"vpr"),"leaving clears the warning and the prompt");

	// Minimal: the status bar and the prompt stay; the hit flash, warnings,
	// radio, boss bar and notices go.
	RemasteredLayoutPreference(host,2);
	Check(runtime.RunEvent("showExitmessage",7.2,effects,error),"boarding again runs");
	runtime.Frame(RemasteredViewport(1280,720),7.25);
	Bounds minimalHull;
	Check(runtime.GetBounds("vhull-gauge",minimalHull) && Near(minimalHull.x,hull.x) && Near(minimalHull.y,hull.y),
		"Minimal keeps the vehicle status bar in place");
	Check(!Shown(runtime,"hit-full") && !Shown(runtime,"cl-full") && !Shown(runtime,"cr-full") && !Shown(runtime,"tr-full") &&
		!Shown(runtime,"tc-full") && !Shown(runtime,"cc-full"),"Minimal leaves out the hit flash, warnings, radio, boss bar and notices");
	RemasteredLayoutPreference(host,1);
	Check(runtime.RunEvent("hideExitmessage",7.3,effects,error),"leaving again runs");
	runtime.Frame(RemasteredViewport(1280,720),7.35);
	Check(Shown(runtime,"hit-full") && Shown(runtime,"tr-full"),"Remastered brings them back");
	Check(host.errors == 0,"no runtime diagnostics across the vehicle HUD exercise");
	runtime.Shutdown();
}

// The air-defence cannon (guis/vehicles/airdef_cannon.gui, 14.12): the nailgun
// lens held still with its load bars and reticle, the transmission display the
// stock lacks, and the HUD prompt in place of the centred exit line.
static void CheckRemasteredAirdefHud(ScreenHost& host, const char* path) {
	const auto source = Read(path);
	Document document; std::vector<Diagnostic> diagnostics;
	if (!document.Load(source,diagnostics)) for (const auto& d : diagnostics) std::fprintf(stderr,"%s %u:%u %s\n",path,unsigned(d.line),unsigned(d.column),d.message.c_str());
	Check(diagnostics.empty() && document.Model().id == "openq4.hud.remastered.airdef","Remastered air-defence HUD validates");
	RemasteredLayoutPreference(host,1);
	Runtime runtime(host);
	Check(runtime.Initialize(),"retained runtime");
	Check(runtime.LoadDocument(source,"guis/hud/remastered_airdef.q4ui",diagnostics),"Remastered air-defence HUD loads into a runtime");
	std::string error;
	Runtime::EventEffects effects;
	host.softFocus = true;
	Check(runtime.SetState(RemasteredLayout(1280,720),error,0),error.c_str());
	Check(runtime.RunEvent("hudJoin",0,effects,error),"the join program runs");
	host.softened.clear(); host.draws.clear();
	runtime.Frame(RemasteredViewport(1280,720),.1);
	Check(host.errors == 0 && !host.draws.empty(),"the cannon HUD draws without runtime errors");
	Bounds optic;
	Check(runtime.GetBounds("ad-optic",optic) && Near(optic.x,160) && Near(optic.y,0) && Near(optic.width,960) && Near(optic.height,720),
		"the optic fills the 4:3 center");
	Check(!host.softened.empty(),"the lens softens the view beyond its aperture");
	Bounds reticle, load, ring;
	Check(Shown(runtime,"ad-reticle") && runtime.GetBounds("ad-reticle",reticle) && runtime.GetBounds("ad-load",load) &&
		runtime.GetBounds("ad-ring2",ring) && Near(load.x,optic.x) && Near(ring.width,optic.width),"the reticle, load bars and ring are laid out in the optic");
	Check(!Shown(runtime,"vpr") && !Shown(runtime,"radio"),"no prompt or transmission before boarding or chatter");

	// Boarding shows the jump key's Exit; a script's lock shows Vehicle locked.
	Check(runtime.SetState({{"oq4_key_jump",std::string("SPACE")}},error,.2),error.c_str());
	Check(runtime.RunEvent("showExitmessage",.2,effects,error),"boarding runs");
	runtime.Frame(RemasteredViewport(1280,720),.25);
	Bounds prompt;
	Check(Shown(runtime,"vpr") && Shown(runtime,"vpr-exit") && !Shown(runtime,"vpr-lock") && runtime.GetBounds("vpr",prompt) &&
		Near(prompt.x+prompt.width,320+629*1.5f,1.5f),"the HUD prompt trails at 629 u with Exit");
	const auto key = runtime.PresentedValue("vpr-key","text");
	Check(key && key->text == "SPACE","the prompt names the jump control");
	Check(runtime.RunEvent("showLockmessage",.3,effects,error),"a lock runs");
	runtime.Frame(RemasteredViewport(1280,720),.35);
	Check(Shown(runtime,"vpr-lock") && !Shown(runtime,"vpr-exit"),"a lock reads Vehicle locked");
	Check(runtime.RunEvent("hideLockmessage",.4,effects,error),"unlocking runs");

	// The transmission display the stock cannon HUD lacks, and the save notice.
	Check(runtime.RunEvent("radioChatterUp",.4,effects,error) && runtime.RunEvent("saveMessage",.4,effects,error),"chatter and a save run");
	runtime.Frame(RemasteredViewport(1280,720),1.2);
	Check(Shown(runtime,"radio") && Shown(runtime,"saved") && Shown(runtime,"vpr-exit"),"the transmission display and the save notice show");

	// Wide and tall views keep the optic round and centred.
	Check(runtime.SetState(RemasteredLayout(2560,1080),error,1.3),error.c_str());
	runtime.Frame(RemasteredViewport(2560,1080),1.35);
	Check(runtime.GetBounds("ad-optic",optic) && Near(optic.x,560,1.f) && Near(optic.width,1440,1.f) && Near(optic.height,1080,1.f),
		"at 21:9 the optic keeps its proportions at the center");
	Check(runtime.SetState(RemasteredLayout(720,1280),error,1.4),error.c_str());
	runtime.Frame(RemasteredViewport(720,1280),1.45);
	Check(runtime.GetBounds("ad-optic",optic) && Near(optic.width,720,1.f) && Near(optic.height,540,1.f) && Near(optic.y,370,1.f),
		"in a tall view the optic fits the width");

	// Minimal keeps the optic, the prompt and the save notice.
	Check(runtime.SetState(RemasteredLayout(1280,720),error,1.5),error.c_str());
	RemasteredLayoutPreference(host,2);
	runtime.Frame(RemasteredViewport(1280,720),1.55);
	Check(!Shown(runtime,"tr-full") && Shown(runtime,"vpr") && Shown(runtime,"ad-reticle"),"Minimal keeps the optic and the prompt");
	RemasteredLayoutPreference(host,1);
	Check(runtime.RunEvent("hideExitmessage",1.6,effects,error),"leaving runs");
	runtime.Frame(RemasteredViewport(1280,720),1.65);
	Check(!Shown(runtime,"vpr"),"leaving clears the prompt");
	Check(host.errors == 0,"no runtime diagnostics across the cannon exercise");
	host.softFocus = false;
	runtime.Shutdown();
}

// The Awakening's space cannon (turretplayerhud.gui, 14.12): the ship-status
// panel under the transmission display with its damage, markers, darts and
// view cone, and the HUD prompt the turret HUD lacked.
static void CheckRemasteredTurretHud(ScreenHost& host, const char* path) {
	const auto source = Read(path);
	Document document; std::vector<Diagnostic> diagnostics;
	if (!document.Load(source,diagnostics)) for (const auto& d : diagnostics) std::fprintf(stderr,"%s %u:%u %s\n",path,unsigned(d.line),unsigned(d.column),d.message.c_str());
	Check(diagnostics.empty() && document.Model().id == "openq4.hud.remastered.turret","Remastered space cannon HUD validates");
	RemasteredLayoutPreference(host,1);
	Runtime runtime(host);
	Check(runtime.Initialize(),"retained runtime");
	Check(runtime.LoadDocument(source,"guis/hud/remastered_turret.q4ui",diagnostics),"Remastered space cannon HUD loads into a runtime");
	std::string error;
	Runtime::EventEffects effects;
	StateValues values = RemasteredLayout(1280,720);
	values.insert({{"oq4_key_jump",std::string("SPACE")}});
	Check(runtime.SetState(values,error,0),error.c_str());
	Check(runtime.RunEvent("hudJoin",0,effects,error),"the join program runs");
	runtime.Frame(RemasteredViewport(1280,720),.1);
	Check(host.errors == 0,"the space cannon HUD frames without runtime errors");
	// The panel: the stock 128 u box at 512,32 of the top-trailing anchor.
	Bounds panel;
	Check(Shown(runtime,"ship") && runtime.GetBounds("ship",panel) && Near(panel.x,320+512*1.5f,1.f) && Near(panel.y,32*1.5f,1.f) &&
		Near(panel.width,128*1.5f,1.f),"the ship panel sits under the transmission display");
	Bounds gauge;
	Check(!runtime.GetBounds("health-gauge",gauge) && !runtime.GetBounds("strip",gauge),"the gun keeps no gauges or weapon strip");
	Check(Shown(runtime,"vpr") && Shown(runtime,"vpr-exit"),"the crewed gun shows Exit at once");
	// Damage: each section lights on its event and clears on its _off.
	Check(runtime.RunEvent("ship_health_01_on",.2,effects,error) && runtime.RunEvent("ship_health_02_on",.2,effects,error),"damage runs");
	runtime.Frame(RemasteredViewport(1280,720),.25);
	Check(Shown(runtime,"ship-damage1") && Shown(runtime,"ship-damage2") && !Shown(runtime,"ship-damage3"),"the damaged sections light");
	Check(runtime.RunEvent("ship_health_01_off",.3,effects,error),"a repair runs");
	runtime.Frame(RemasteredViewport(1280,720),.35);
	Check(!Shown(runtime,"ship-damage1") && Shown(runtime,"ship-damage2"),"a repaired section clears");
	// A marker spins at 0.35 turn a second.
	Check(runtime.RunEvent("ship_target_03_on",.4,effects,error),"a marker runs");
	runtime.Frame(RemasteredViewport(1280,720),.4);
	runtime.Frame(RemasteredViewport(1280,720),.9);
	const auto spin = runtime.PresentedValue("ship-target3","transform");
	Check(Shown(runtime,"ship-target3") && !Shown(runtime,"ship-target1") && spin && std::fabs(float(spin->data[4])) > 40,
		"the objective marker shows and spins");
	// A dart pulses while it fades over 3 s.
	Check(runtime.RunEvent("warning_NE",1.0,effects,error),"a warning runs");
	runtime.Frame(RemasteredViewport(1280,720),1.01);
	const auto lit = runtime.PresentedValue("ship-warn-NE","opacity");
	Check(lit && float(lit->data[0]) > .8,"the dart lights");
	runtime.Frame(RemasteredViewport(1280,720),4.2);
	const auto faded = runtime.PresentedValue("ship-warn-NE","opacity");
	Check(faded && float(faded->data[0]) < .01,"the dart has faded after 3 s");
	// The view cone follows the gun relative to the ship's nose.
	Check(runtime.SetState({{"playerYaw",120.0},{"oq4_shipYaw",90.0}},error,4.3),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),4.35);
	const auto cone = runtime.PresentedValue("ship-cone","transform");
	Check(cone && Near(float(cone->data[4]),-30,.01f),"the cone turns with the gun's heading off the nose");
	// Minimal keeps the panel and the prompt.
	RemasteredLayoutPreference(host,2);
	runtime.Frame(RemasteredViewport(1280,720),4.4);
	Check(!Shown(runtime,"tr-full") && Shown(runtime,"ship") && Shown(runtime,"vpr"),"Minimal keeps the ship panel and the prompt");
	RemasteredLayoutPreference(host,1);
	Check(host.errors == 0,"no runtime diagnostics across the space cannon exercise");
	runtime.Shutdown();
}

// The Awakening's MCC cannon (cannonplayerhud.gui, 14.12): the MCC's hull
// readout stacked under the transmission display, filled in proportion and
// warning at 25%, and the HUD prompt's Exit.
static void CheckRemasteredMccHud(ScreenHost& host, const char* path) {
	const auto source = Read(path);
	Document document; std::vector<Diagnostic> diagnostics;
	if (!document.Load(source,diagnostics)) for (const auto& d : diagnostics) std::fprintf(stderr,"%s %u:%u %s\n",path,unsigned(d.line),unsigned(d.column),d.message.c_str());
	Check(diagnostics.empty() && document.Model().id == "openq4.hud.remastered.mcc","Remastered MCC cannon HUD validates");
	RemasteredLayoutPreference(host,1);
	Runtime runtime(host);
	Check(runtime.Initialize(),"retained runtime");
	Check(runtime.LoadDocument(source,"guis/hud/remastered_mcc.q4ui",diagnostics),"Remastered MCC cannon HUD loads into a runtime");
	std::string error;
	Runtime::EventEffects effects;
	StateValues values = RemasteredLayout(1280,720);
	values.insert({{"oq4_key_jump",std::string("SPACE")}});
	Check(runtime.SetState(values,error,0),error.c_str());
	Check(runtime.RunEvent("hudJoin",0,effects,error),"the join program runs");
	runtime.Frame(RemasteredViewport(1280,720),.1);
	Check(host.errors == 0,"the MCC cannon HUD frames without runtime errors");
	// The readout: 36 u above the stock group, directly under the transmission display.
	Bounds gauge;
	Check(runtime.GetBounds("mcc-gauge",gauge) && Near(gauge.x,320+494*1.5f,1.f) && Near(gauge.y,(80-36)*1.5f,1.f) &&
		Near(gauge.width,136*1.5f,1.f),"the hull gauge stacks under the transmission display");
	auto hull = runtime.PresentedValue("mcc-hull","text");
	Check(hull && hull->text == "2000","a whole MCC reads 2000");
	Check(Shown(runtime,"mcc-fill") && !Shown(runtime,"mcc-fill-warn") && Shown(runtime,"vpr") && Shown(runtime,"vpr-exit"),
		"the full track and Exit show");
	// The drawn extent of the hull fill: #B3D06E at 0.35, premultiplied.
	const auto fillExtent = [&]() {
		float lo = 1e9f, hi = -1e9f;
		for (const auto& draw : host.draws) for (const auto& v : draw.vertices) {
			if (v.a < .3f || v.a > .4f || std::fabs(v.r/v.a-.702f) > .03f || std::fabs(v.g/v.a-.816f) > .03f) continue;
			lo = std::min(lo,v.x); hi = std::max(hi,v.x);
		}
		return std::pair{lo,hi};
	};
	host.draws.clear(); runtime.Frame(RemasteredViewport(1280,720),.15);
	const auto fullFill = fillExtent();
	// The track fills in proportion: half the hull moves the window's trailing edge to the middle.
	Check(runtime.SetState({{"mcchealth",1000.0}},error,.2),error.c_str());
	host.draws.clear();
	runtime.Frame(RemasteredViewport(1280,720),.25);
	const auto halfFill = fillExtent();
	Check(fullFill.second > fullFill.first+.8f*gauge.width && Near(halfFill.first,fullFill.first,1.f) &&
		halfFill.second < fullFill.second-.3f*gauge.width && halfFill.second > fullFill.first+.3f*gauge.width,
		"half the hull draws half the fill from the leading end");
	const auto shift = runtime.PresentedValue("mcc-win","transform");
	const float expected = ((5.4f+114.2f*.5f)*136.f/125.f-136.f)*1.5f;
	hull = runtime.PresentedValue("mcc-hull","text");
	Check(shift && Near(float(shift->data[0]),expected,.1f) && hull && hull->text == "1000","half the hull fills half the track");
	// At or below 25% the readout warns.
	Check(runtime.SetState({{"mcchealth",400.0}},error,.3),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),.35);
	const auto colour = runtime.PresentedValue("mcc-hull","color");
	Check(Shown(runtime,"mcc-fill-warn") && Shown(runtime,"mcc-track-warn") && !Shown(runtime,"mcc-fill") && colour &&
		Near(float(colour->data[0]),1) && Near(float(colour->data[1]),.2f,.01f),"a quarter hull warns in #FF3300");
	// Minimal keeps the readout and the prompt.
	RemasteredLayoutPreference(host,2);
	runtime.Frame(RemasteredViewport(1280,720),.4);
	Check(!Shown(runtime,"tr-full") && Shown(runtime,"vpr") && runtime.GetBounds("mcc-gauge",gauge),"Minimal keeps the hull readout and the prompt");
	RemasteredLayoutPreference(host,1);
	Check(host.errors == 0,"no runtime diagnostics across the MCC cannon exercise");
	runtime.Shutdown();
}

// The Awakening's speeder bike (speederplayerhud.gui, 14.12): the vehicle status
// bar with the bike's own glyphs, the race display 18 u lower with its padded
// timer and last-10-seconds red, the boost in the HUD prompt and the streak.
static void CheckRemasteredSpeederHud(ScreenHost& host, const char* path) {
	const auto source = Read(path);
	Document document; std::vector<Diagnostic> diagnostics;
	if (!document.Load(source,diagnostics)) for (const auto& d : diagnostics) std::fprintf(stderr,"%s %u:%u %s\n",path,unsigned(d.line),unsigned(d.column),d.message.c_str());
	Check(diagnostics.empty() && document.Model().id == "openq4.hud.remastered.speeder","Remastered speeder HUD validates");
	RemasteredLayoutPreference(host,1);
	Runtime runtime(host);
	Check(runtime.Initialize(),"retained runtime");
	Check(runtime.LoadDocument(source,"guis/hud/remastered_speeder.q4ui",diagnostics),"Remastered speeder HUD loads into a runtime");
	std::string error;
	Runtime::EventEffects effects;
	host.softFocus = true;
	StateValues values = RemasteredLayout(1280,720);
	values.insert({{"vehicle_health",0.8},{"vehicle_shield",0.55},{"vehicle_weapon",0.0},{"vehicle_weaponcharge",0.5},
		{"vehicle_locked",0.0},{"oq4_key_jump",std::string("SPACE")},{"oq4_key_crouch",std::string("C")},{"oq4_boost",0.0}});
	Check(runtime.SetState(values,error,0),error.c_str());
	Check(runtime.RunEvent("hudJoin",0,effects,error),"the join program runs");
	host.streaks.clear(); host.draws.clear();
	runtime.Frame(RemasteredViewport(1280,720),.1);
	Check(host.errors == 0,"the speeder HUD frames without runtime errors");
	Check(host.streaks.empty(),"no streak before a boost");

	// The bike lists its machinegun first: weapon 0 shows the machinegun's
	// panel without a charge bar, weapon 1 the cannon's with it.
	Bounds hull;
	Check(runtime.GetBounds("vhull-gauge",hull) && Near(hull.x,160+241.5f*1.5f,1.f),"the status bar centres on the bottom edge");
	Check(Shown(runtime,"vweap0") && !Shown(runtime,"vweap1") && !Shown(runtime,"vcharge") && !Shown(runtime,"vcount"),
		"the machinegun shows its panel, no charge bar and no count");
	Check(runtime.SetState({{"vehicle_weapon",1.0}},error,.2),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),.25);
	Check(Shown(runtime,"vweap1") && Shown(runtime,"vcharge") && !Shown(runtime,"vcount"),"the cannon shows its recharge");
	Check(runtime.SetState({{"vehicle_health",0.2}},error,.3),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),.35);
	Check(Shown(runtime,"vhull-track-warn") && Shown(runtime,"vhull-label-warn"),"the hull warning the expansion removed is back");
	Check(runtime.SetState({{"vehicle_health",0.8}},error,.4),error.c_str());

	// The race display: the plate 18 u below the stock, the pips lit per checkpoint.
	host.draws.clear(); runtime.Frame(RemasteredViewport(1280,720),.45);
	float top = 1e9f, bottom = -1e9f, left = 1e9f, right = -1e9f;
	for (const auto& draw : host.draws) for (const auto& v : draw.vertices) {
		// The plate's black at 0.454, in the top half (the status bar's backing is lower).
		if (std::fabs(v.a-.454f) > .003f || v.r > .01f || v.y > 360) continue;
		top = std::min(top,v.y); bottom = std::max(bottom,v.y); left = std::min(left,v.x); right = std::max(right,v.x);
	}
	Check(Near(top,(15+18)*1.5f,1.5f) && Near(bottom,(60.5f+18)*1.5f,1.5f) && Near(left,160+68.1f*1.5f,1.5f) &&
		Near(right,160+571.4f*1.5f,1.5f),"the race plate sits 18 u lower at the top centre");
	Check(!Shown(runtime,"race-pip0"),"no checkpoint lights no pip");
	Check(runtime.RunEvent("showprogress30",.5,effects,error),"a checkpoint runs");
	runtime.Frame(RemasteredViewport(1280,720),.55);
	Check(Shown(runtime,"race-pip0") && Shown(runtime,"race-pip2") && !Shown(runtime,"race-pip3"),"three checkpoints light three pips");

	// The timer shows as the script runs it, the hundredths two digits wide.
	Check(!Shown(runtime,"race-sec") && !Shown(runtime,"race-hun"),"no placeholder timer before the race");
	Check(runtime.SetState({{"currenttime",42.0},{"currenttime2",5.0}},error,.6),error.c_str());
	Check(runtime.RunEvent("updatetimer",.6,effects,error) && runtime.RunEvent("updatetimer2",.6,effects,error),"the countdown runs");
	runtime.Frame(RemasteredViewport(1280,720),.65);
	const auto seconds = runtime.PresentedValue("race-sec","text"), tens = runtime.PresentedValue("race-hun-tens","text"),
		units = runtime.PresentedValue("race-hun-units","text");
	Check(Shown(runtime,"race-sec") && Shown(runtime,"race-hun") && Shown(runtime,"race-point") && seconds && seconds->text == "42" &&
		tens && tens->text == "0" && units && units->text == "5","the timer reads 42.05");
	Bounds sec;
	Check(runtime.GetBounds("race-sec",sec) && Near(sec.x+sec.width,160+158*1.5f,1.f),"the seconds trail at 158 u");
	// The last 10 s turn the seconds #FF3333 over 100 ms, and back.
	Check(runtime.SetState({{"currenttime",9.0}},error,.7),error.c_str());
	Check(runtime.RunEvent("updatetimer",.7,effects,error),"the countdown runs into its last 10 s");
	runtime.Frame(RemasteredViewport(1280,720),.75);
	auto colour = runtime.PresentedValue("race-sec","color");
	Check(colour && colour->data[1] < .8 && colour->data[1] > .2,"the seconds are turning red");
	runtime.Frame(RemasteredViewport(1280,720),.85);
	colour = runtime.PresentedValue("race-sec","color");
	Check(colour && Near(float(colour->data[0]),1) && Near(float(colour->data[1]),.2f,.01f) && Near(float(colour->data[2]),.2f,.01f),
		"the last 10 s read #FF3333");
	Check(runtime.SetState({{"currenttime",30.0}},error,.9),error.c_str());
	Check(runtime.RunEvent("updatetimer",.9,effects,error),"a new area adds time");
	runtime.Frame(RemasteredViewport(1280,720),1.05);
	colour = runtime.PresentedValue("race-sec","color");
	Check(colour && Near(float(colour->data[0]),.882f,.01f) && Near(float(colour->data[2]),.404f,.01f),"more time turns the seconds gold");
	Check(runtime.RunEvent("hidespeedertimer",1.1,effects,error) && runtime.RunEvent("hidespeedertimer2",1.1,effects,error),"the timer hides");
	runtime.Frame(RemasteredViewport(1280,720),1.15);
	Check(!Shown(runtime,"race-sec") && !Shown(runtime,"race-hun"),"a hidden timer leaves the plate");

	// The prompt: Boost with the crouch key while ready, Boosting while it
	// pushes, nothing while it recharges; Exit (or the lock) trails.
	Check(runtime.RunEvent("showExitmessage",1.2,effects,error),"boarding runs");
	runtime.Frame(RemasteredViewport(1280,720),1.25);
	const auto crouch = runtime.PresentedValue("vpr-boost-key","text");
	Check(Shown(runtime,"vpr") && Shown(runtime,"vpr-boost") && !Shown(runtime,"vpr-boosting") && Shown(runtime,"vpr-exit") && crouch &&
		crouch->text == "C","a ready boost shows the crouch key before Exit");
	Bounds boostItem, exitItem, prompt;
	Check(runtime.GetBounds("vpr-boost",boostItem) && runtime.GetBounds("vpr-exit",exitItem) && boostItem.x < exitItem.x &&
		Near(exitItem.x-(boostItem.x+boostItem.width),11*1.5f,.6f) && runtime.GetBounds("vpr",prompt) &&
		Near(prompt.x+prompt.width,320+629*1.5f,1.5f),"Boost leads Exit by 11 u and the prompt trails at 629 u");
	Check(runtime.SetState({{"oq4_boost",1.0},{"oq4_boost_envelope",0.5}},error,1.3),error.c_str());
	host.streaks.clear();
	runtime.Frame(RemasteredViewport(1280,720),1.35);
	Check(Shown(runtime,"vpr-boosting") && !Shown(runtime,"vpr-boost"),"a pushing boost shows the hazard and Boosting");
	Check(host.streaks.size() == 1 && Near(host.streaks[0],.5f),"the view streaks with the boost's envelope");
	host.reducedMotion = true; host.streaks.clear();
	runtime.Frame(RemasteredViewport(1280,720),1.4);
	Check(host.streaks.empty(),"reduced motion never streaks");
	host.reducedMotion = false;
	Check(runtime.SetState({{"oq4_boost",2.0},{"oq4_boost_envelope",0.0}},error,1.45),error.c_str());
	host.streaks.clear();
	runtime.Frame(RemasteredViewport(1280,720),1.5);
	Check(!Shown(runtime,"vpr-boosting") && !Shown(runtime,"vpr-boost") && Shown(runtime,"vpr-exit") && host.streaks.empty(),
		"a recharging boost leaves only Exit and no streak");
	Check(runtime.SetState({{"vehicle_locked",1.0}},error,1.55),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),1.6);
	Check(Shown(runtime,"vpr-lock") && !Shown(runtime,"vpr-exit"),"the race's lock shows Vehicle locked");

	// The entry lines the expansion's HUD never played: the cannon's first.
	const auto line1 = runtime.PresentedValue("vmsg1-text","text"), line2 = runtime.PresentedValue("vmsg2-text","text");
	Check(Shown(runtime,"vmsg") && line1 && line1->text == "#str_200939" && line2 && line2->text == "#str_200940",
		"boarding with the cannon names the machinegun first");

	// Minimal keeps the status bar, the race display and the prompt, never the streak.
	RemasteredLayoutPreference(host,2);
	Check(runtime.SetState({{"oq4_boost",1.0},{"oq4_boost_envelope",1.0}},error,1.7),error.c_str());
	host.streaks.clear();
	runtime.Frame(RemasteredViewport(1280,720),1.75);
	Check(!Shown(runtime,"tr-full") && !Shown(runtime,"hit-full") && Shown(runtime,"race-plate") && Shown(runtime,"vpr") &&
		runtime.GetBounds("vhull-gauge",hull) && host.streaks.empty(),"Minimal keeps the race display, status bar and prompt without the streak");
	RemasteredLayoutPreference(host,1);
	Check(host.errors == 0,"no runtime diagnostics across the speeder exercise");
	host.softFocus = false;
	runtime.Shutdown();
}

// The objectives display and notices (wristcomm.gui, 14.11).
static void CheckRemasteredObjectivesHud(ScreenHost& host, const char* path, bool strogg) {
	const auto source = Read(path);
	Document document; std::vector<Diagnostic> diagnostics;
	if (!document.Load(source,diagnostics)) for (const auto& d : diagnostics) std::fprintf(stderr,"%s %u:%u %s\n",path,unsigned(d.line),unsigned(d.column),d.message.c_str());
	Check(diagnostics.empty() && document.Model().id == std::string("openq4.hud.remastered.objectives")+(strogg ? ".strogg" : ""),
		"Remastered objectives validate");
	RemasteredLayoutPreference(host,1);
	Runtime runtime(host);
	Check(runtime.Initialize(),"retained runtime");
	Check(runtime.LoadDocument(source,"guis/hud/remastered_objectives.q4ui",diagnostics),"Remastered objectives load into a runtime");
	std::string error;
	Runtime::EventEffects effects;
	Check(runtime.SetState(RemasteredLayout(1280,720),error,0),error.c_str());
	Check(runtime.RunEvent("hudJoin",0,effects,error),"the join program runs");
	runtime.Frame(RemasteredViewport(1280,720),.1);
	Check(host.errors == 0 && !Shown(runtime,"on") && !Shown(runtime,"od") && !Shown(runtime,"odisp"),"nothing shows at rest");

	// The new-objective notice: the top-leading 200 u plate, its own title.
	Check(runtime.SetState({{"oq4_new_title",std::string("Reach the hangar control room")},
		{"oq4_new_shot",std::string("gfx/objectives/none")}},error,.2),error.c_str());
	Check(runtime.RunEvent("newObjective",.2,effects,error),"a new objective runs");
	runtime.Frame(RemasteredViewport(1280,720),.2+.3);
	Check(Shown(runtime,"on"),"the new-objective notice shows");
	const auto frame = runtime.PresentedValue("on-frame","opacity"), text = runtime.PresentedValue("on-text","opacity");
	Check(frame && Near(float(frame->data[0]),.4f,.01f) && text && Near(float(text->data[0]),1,.01f),"the frame rests at 0.40 under full text");
	const auto title = runtime.PresentedValue("on-title","text");
	Check(title && title->text == "Reach the hangar control room","the notice names the new objective");
	Bounds head;
	Check(runtime.GetBounds("on-head",head) && Near(head.x,9*1.5f,1.f),"the heading sits at the leading corner");
	// The completion notice: mirrored at the top-trailing corner, on its own timer.
	Check(runtime.SetState({{"oq4_done_title",std::string("Find the comms relay")}},error,.6),error.c_str());
	Check(runtime.RunEvent("newObjectiveComplete",.6,effects,error),"a completion runs");
	runtime.Frame(RemasteredViewport(1280,720),.6+.3);
	Bounds done;
	Check(Shown(runtime,"od") && runtime.GetBounds("od-head",done) && Near(done.x+done.width,320+631*1.5f,1.f),
		"the completion heading trails at 631 u");
	Check(runtime.RunEvent("oq4_closeNewObjective",1,effects,error),"the new notice closes");
	runtime.Frame(RemasteredViewport(1280,720),1.2);
	Check(!Shown(runtime,"on") && Shown(runtime,"od"),"closing one notice leaves the other up");
	Check(runtime.RunEvent("oq4_closeDoneObjective",1.3,effects,error),"the completion closes");
	runtime.Frame(RemasteredViewport(1280,720),1.5);
	Check(!Shown(runtime,"od"),"both notices are gone");

	// The display: five objectives scroll three at a time, then the completed ones.
	StateValues list = {{"oq4_obj_count",5.0},{"oq4_done_count",2.0},{"oq4_obj_scroll",0.0},{"oq4_done0",std::string("Done A")},
		{"oq4_done1",std::string("Done B")}};
	for (int i = 0; i < 5; ++i) {
		list[("oq4_obj"+std::to_string(i)+"_title")] = std::string("Objective ")+std::to_string(i);
		list[("oq4_obj"+std::to_string(i)+"_text")] = std::string("Description of objective ")+std::to_string(i);
	}
	Check(runtime.SetState(list,error,1.6),error.c_str());
	Check(runtime.RunEvent("wristcommShow",1.6,effects,error),"the display opens");
	runtime.Frame(RemasteredViewport(1280,720),1.6+.3);
	Check(Shown(runtime,"odisp") && Shown(runtime,"odisp-e0") && Shown(runtime,"odisp-e2") && !Shown(runtime,"odisp-e3"),
		"three entries show at once");
	Check(Shown(runtime,"odisp-scroll") && Shown(runtime,"odisp-more") && !Shown(runtime,"odisp-done"),
		"a longer list shows its scrollbar and the collapsed COMPLETED header");
	Bounds e0, e1;
	Check(runtime.GetBounds("odisp-e0",e0) && runtime.GetBounds("odisp-e1",e1) && Near(e1.y-e0.y,143*1.5f,1.5f),
		"entries keep the stock 143 u pitch");
	Check(runtime.SetState({{"oq4_obj_scroll",3.0}},error,2),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),2.05);
	Check(!Shown(runtime,"odisp-e0") && Shown(runtime,"odisp-e3") && Shown(runtime,"odisp-e4") && Shown(runtime,"odisp-done") &&
		Shown(runtime,"odisp-d1") && !Shown(runtime,"odisp-more"),"scrolled to the end, the completed objectives follow");
	Check(runtime.SetState({{"oq4_obj_count",0.0},{"oq4_done_count",0.0},{"oq4_obj_scroll",0.0}},error,2.1),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),2.15);
	Check(Shown(runtime,"odisp-empty") && !Shown(runtime,"odisp-scroll"),"no objective shows the empty plate");
	// Held (the stock), the display has no prompt; with Toggle Objectives the
	// HUD prompt names the scores control and Close at the trailing corner.
	Check(!Shown(runtime,"opr"),"a held display shows no Close prompt");
	Check(runtime.SetState({{"oq4_obj_toggle",true},{"oq4_key_scores",std::string("TAB")}},error,2.16),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),2.17);
	const auto scores = runtime.PresentedValue("opr-key","text");
	Bounds closePrompt;
	Check(Shown(runtime,"opr") && scores && scores->text == "TAB" && runtime.GetBounds("opr",closePrompt) &&
		Near(closePrompt.x+closePrompt.width,320+629*1.5f,1.5f) && Near(closePrompt.y+closePrompt.height,476*1.5f,1.5f),
		"a toggled display shows the scores key and Close at the bottom-trailing corner");
	Check(runtime.RunEvent("wristcommHide",2.2,effects,error),"the display closes");
	runtime.Frame(RemasteredViewport(1280,720),2.25);
	Check(!Shown(runtime,"odisp") && !Shown(runtime,"opr"),"the display and its prompt are gone");

	// A failed objective names itself.
	Check(runtime.SetState({{"oq4_failed_title",std::string("Protect the convoy")}},error,2.3),error.c_str());
	Check(runtime.RunEvent("objectiveFailed",2.3,effects,error),"a failure runs");
	runtime.Frame(RemasteredViewport(1280,720),2.35);
	const auto failed = runtime.PresentedValue("of-title","text");
	Check(Shown(runtime,"of") && failed && failed->text == "Protect the convoy","the failure names the objective");

	// Minimal drops the new and completed notices; a failure still shows.
	RemasteredLayoutPreference(host,2);
	runtime.Frame(RemasteredViewport(1280,720),2.4);
	Check(!Shown(runtime,"tl-full") && !Shown(runtime,"tr-full") && Shown(runtime,"of"),"Minimal keeps only the failure of the notices");
	RemasteredLayoutPreference(host,1);
	runtime.Frame(RemasteredViewport(1280,720),2.45);
	Check(Shown(runtime,"tl-full") && Shown(runtime,"tr-full"),"Remastered brings the notices back");
	Check(host.errors == 0,"no runtime diagnostics across the objectives exercise");
	runtime.Shutdown();
}

// The scoreboard (scoreboard.gui, 14.14): panels, rows, your marker, bots,
// the dead, ping bars and the footer.
static void CheckRemasteredScoreboardHud(ScreenHost& host, const char* path) {
	const auto source = Read(path);
	Document document; std::vector<Diagnostic> diagnostics;
	if (!document.Load(source,diagnostics)) for (const auto& d : diagnostics) std::fprintf(stderr,"%s %u:%u %s\n",path,unsigned(d.line),unsigned(d.column),d.message.c_str());
	Check(diagnostics.empty() && document.Model().id == "openq4.hud.remastered.scoreboard","Remastered scoreboard validates");
	RemasteredLayoutPreference(host,1);
	Runtime runtime(host);
	Check(runtime.Initialize(),"retained runtime");
	Check(runtime.LoadDocument(source,"guis/hud/remastered_scoreboard.q4ui",diagnostics),"Remastered scoreboard loads into a runtime");
	std::string error;
	Runtime::EventEffects effects;
	StateValues values = RemasteredLayout(1280,720);
	values.insert({{"servername",std::string("openQ4 Community")},{"serverip",std::string("203.0.113.24:28004")},
		{"servergametype",std::string("Capture the Flag")},{"servermap",std::string("q4ctf1")},{"serverlimit",std::string("Capture Limit: 5")},
		{"servertimelimit",std::string("Time Limit: 15")},{"timeleft",std::string("7:12")},{"infinity",false},
		{"oq4_sb_pitch",30.0},{"oq4_sb_kills",true},{"oq4_sb_rank",std::string("3RD OF 13")},
		{"oq4_sb0_kind",1.0},{"oq4_sb0_score",std::string("3")},{"oq4_sb0_count",2.0},
		{"oq4_sb0_0_name",std::string("You")},{"oq4_sb0_0_score",21.0},{"oq4_sb0_0_ping",24.0},{"oq4_sb0_0_flags",1.0},
		{"oq4_sb0_1_name",std::string("Bolt")},{"oq4_sb0_1_score",9.0},{"oq4_sb0_1_flags",4.0},
		{"oq4_sb1_kind",2.0},{"oq4_sb1_score",std::string("2")},{"oq4_sb1_count",1.0},
		{"oq4_sb1_0_name",std::string("Vex")},{"oq4_sb1_0_score",24.0},{"oq4_sb1_0_ping",150.0},{"oq4_sb1_0_flags",2.0},
		{"oq4_sb2_kind",3.0},{"oq4_sb2_count",1.0},{"oq4_sb2_0_name",std::string("Quill")},{"oq4_sb2_0_ping",44.0}});
	Check(runtime.SetState(values,error,0),error.c_str());
	Check(runtime.RunEvent("hudJoin",0,effects,error),"the join program runs");
	runtime.Frame(RemasteredViewport(1280,720),.1);
	Check(host.errors == 0,"the scoreboard frames without runtime errors");
	Check(Shown(runtime,"sb0") && Shown(runtime,"sb0-band1") && !Shown(runtime,"sb0-band2") && Shown(runtime,"sb1-band2") &&
		Shown(runtime,"sb2-band3"),"your team, the other team and the spectators each open with their band");
	Check(Shown(runtime,"sb0r0") && Shown(runtime,"sb0r1") && !Shown(runtime,"sb0r2"),"a panel shows its rows");
	Check(Shown(runtime,"sb0r0-band-me") && Shown(runtime,"sb0r0-marker") && !Shown(runtime,"sb0r1-marker"),"your row adds the marker");
	Check(Shown(runtime,"sb0r1-bot") && !Shown(runtime,"sb0r1-clan-c") && !Shown(runtime,"sb0r1-bars"),"a bot carries the BOT tag and no ping bars");
	const auto dim = runtime.PresentedValue("sb1r0-content","opacity");
	Check(dim && Near(float(dim->data[0]),.5f,.01f) && Shown(runtime,"sb1r0-skull"),"a dead player's row drops to 0.50 behind a skull");
	Check(Shown(runtime,"sb0r0-bars4") && Shown(runtime,"sb1r0-bars2"),"ping shows the matching network-quality bars");
	Check(Shown(runtime,"sb0-h-kills-g") && !Shown(runtime,"sb2-h-score-g"),"KILLS shows in flag modes; spectators have no score columns");
	Bounds r0, r1;
	Check(runtime.GetBounds("sb0r0",r0) && runtime.GetBounds("sb0r1",r1) && Near(r1.y-r0.y,30*1.5f,1.f),"rows keep the published pitch");
	const auto rank = runtime.PresentedValue("sb-rank","text");
	Check(Shown(runtime,"sb-rank-g") && rank && rank->text == "3RD OF 13","the rank line reads your place");
	Check(Shown(runtime,"sb-timer"),"a timed match shows the timer");
	Check(runtime.SetState({{"oq4_sb1_kind",-1.0},{"oq4_sb0_kind",0.0},{"oq4_sb_kills",false},{"oq4_sb_pitch",35.0},{"infinity",true}},error,.2),error.c_str());
	runtime.Frame(RemasteredViewport(1280,720),.25);
	Check(Shown(runtime,"sb0-band0") && !Shown(runtime,"sb1") && !Shown(runtime,"sb0-h-kills-g") && !Shown(runtime,"sb-timer"),
		"deathmatch lists the players in one panel without KILLS");
	Check(runtime.GetBounds("sb0r0",r0) && runtime.GetBounds("sb0r1",r1) && Near(r1.y-r0.y,35*1.5f,1.f),"the deathmatch pitch applies");

	// Tourney: the bracket tree answers the stock arena events.
	Check(runtime.SetState({{"gametype",2.0},{"name1_round1_bracket1",std::string("Vex")},{"name2_round1_bracket1",std::string("Kaz")},
		{"score1_round1_bracket1",std::string("3")},{"score2_round1_bracket1",std::string("1")},{"round",1.0},{"bracket",1.0},
		{"empty",false}},error,.3),error.c_str());
	Check(runtime.RunEvent("arenaInit",.3,effects,error),"an arena starts");
	runtime.Frame(RemasteredViewport(1280,720),.35);
	Check(Shown(runtime,"sbt") && !Shown(runtime,"sb") && Shown(runtime,"tb11-words") && !Shown(runtime,"tb11-empty"),
		"Tourney shows the bracket tree instead of the soft board");
	Bounds panelTop;
	Check(runtime.GetBounds("sb0",panelTop) && panelTop.y >= 206*1.5f - 1,"the players list sits under the tree");
	Check(runtime.SetState({{"sel",1.0}},error,.4) && runtime.RunEvent("arenaFocus",.4,effects,error),"your arena is focused");
	runtime.Frame(RemasteredViewport(1280,720),.45);
	Check(Shown(runtime,"tb11-sel1") && !Shown(runtime,"tb11-focus"),"your half of your arena turns aqua");
	Check(runtime.SetState({{"winner",1.0}},error,.5) && runtime.RunEvent("arenaDone",.5,effects,error),"the arena finishes");
	runtime.Frame(RemasteredViewport(1280,720),.55);
	const auto loser = runtime.PresentedValue("tb11-n2","color"), victor = runtime.PresentedValue("tb11-s1","color");
	Check(Shown(runtime,"tb11-done") && !Shown(runtime,"tb11-sel1") && loser && Near(float(loser->data[3]),.5f,.01f) &&
		victor && Near(float(victor->data[2]),1,.01f),"a finished arena turns blue, its loser dims and the winner's score whitens");
	Check(runtime.SetState({{"round",2.0},{"bracket",3.0},{"empty",true}},error,.6) && runtime.RunEvent("arenaInit",.6,effects,error),
		"an empty arena starts");
	runtime.Frame(RemasteredViewport(1280,720),.65);
	Check(Shown(runtime,"tb23-empty") && !Shown(runtime,"tb23-words"),"an empty arena reads EMPTY");
	Check(host.errors == 0,"no runtime diagnostics across the scoreboard exercise");
	runtime.Shutdown();
}

// --hud-bench <document>: per-operation cost of a Remastered HUD document in
// the runtime (events, state batches, frames), for performance work.
static void BenchRemasteredHud(const char* path) {
	ScreenHost host;
	const auto source = Read(path);
	std::vector<Diagnostic> diagnostics;
	RemasteredLayoutPreference(host,1);
	Runtime runtime(host);
	Check(runtime.Initialize(),"retained runtime");
	Check(runtime.LoadDocument(source,"guis/hud/bench.q4ui",diagnostics),"bench document loads");
	std::string error;
	Runtime::EventEffects effects;
	StateValues values = RemasteredLayout(1280,720);
	values.insert({{"gametype",1.0}});
	runtime.SetState(values,error,0);
	runtime.RunEvent("hudJoin",0,effects,error);
	runtime.Frame(RemasteredViewport(1280,720),.01);
	using clock = std::chrono::steady_clock;
	const auto ms = [](clock::time_point a, clock::time_point b) { return std::chrono::duration<double,std::milli>(b-a).count(); };
	double t = .02;
	auto start = clock::now();
	// An event the game sends often: a health update on foot, a transmit change in multiplayer.
	const char* event = runtime.HasEvent("updateHealth") ? "updateHealth" : "hide_transmit";
	for (int i = 0; i < 50; ++i) { runtime.RunEvent(event,t,effects,error); t += .016; }
	const double eventMs = ms(start,clock::now())/50;
	start = clock::now();
	for (int i = 0; i < 50; ++i) { runtime.SetState({{"hud.scale",1.0+(i%2)*1e-6}},error,t); t += .016; }
	const double stateMs = ms(start,clock::now())/50;
	start = clock::now();
	for (int i = 0; i < 50; ++i) { runtime.Frame(RemasteredViewport(1280,720),t); t += .016; }
	const double frameMs = ms(start,clock::now())/50;
	double changedFrameMs = 0, changedFrameOnlyMs = 0; std::uint64_t scanned = 0;
	for (int i = 0; i < 50; ++i) {
		const auto a = clock::now();
		// A readout change, as the game makes: a number on foot, the clock in multiplayer.
		if (!runtime.SetState({{"player_health",double(50+i%2)}},error,t))
			runtime.SetState({{"oq4_clock",std::string(i%2 ? "7:05" : "7:04")}},error,t);
		const auto b = clock::now();
		runtime.Frame(RemasteredViewport(1280,720),t); t += .016;
		const auto c = clock::now();
		changedFrameMs += ms(a,c); changedFrameOnlyMs += ms(b,c); scanned += runtime.Statistics().motionInputsScanned;
	}
	changedFrameMs /= 50; changedFrameOnlyMs /= 50;
	std::printf("HUD_BENCH_CHANGED frame_after_change_ms=%.3f scanned=%llu\n",changedFrameOnlyMs,(unsigned long long)(scanned/50));
	std::printf("HUD_BENCH document=%s event_ms=%.3f state_ms=%.3f frame_ms=%.3f state_frame_ms=%.3f errors=%d\n",path,eventMs,stateMs,frameMs,changedFrameMs,host.errors);
	runtime.Frame(RemasteredViewport(1280,720),t);
	const auto s = runtime.Statistics();
	std::printf("HUD_BENCH_FRAME update_ms=%.3f render_ms=%.3f vector_compile_ms=%.3f vectors=%llu compiled=%llu hits=%llu geometry_compiles=%llu draws=%llu vertices=%llu layers=%llu\n",
		s.updateMilliseconds,s.renderMilliseconds,s.vectorCompileMilliseconds,(unsigned long long)s.vectorElements,(unsigned long long)s.vectorPathsCompiled,
		(unsigned long long)s.vectorCacheHits,(unsigned long long)s.geometryCompiles,(unsigned long long)s.drawCalls,(unsigned long long)s.submittedVertices,
		(unsigned long long)s.layerComposites);
	runtime.Shutdown();
}
