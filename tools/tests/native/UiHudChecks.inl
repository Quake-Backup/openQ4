// Copyright (C) 2026 DarkMatter Productions. GPL-3.0-or-later.
static void CheckHud(const char* path) {
	const auto source=Read(path); Document doc; std::vector<Diagnostic> diagnostics;
	if (!doc.Load(source,diagnostics)) for (const auto& d:diagnostics) std::fprintf(stderr,"%s\n",d.message.c_str());
	Check(diagnostics.empty(),"HUD artwork validates as an editable canonical document");
	ScreenHost host; Runtime runtime(host);
	// RmlUi sees a VFS resource name, never a host drive path/protocol.
	const char* resource="guis/hud/artwork.q4ui";
	Check(runtime.LoadDocument(source,resource,diagnostics),"HUD catalog loads through retained runtime");
	Viewport viewport; viewport.width=1280;viewport.height=720;
	std::string error;
	int tested=0;
	for (const auto& node:doc.Model().root.children) {
		HudPrimitive primitive;primitive.artwork=node.id;primitive.quad={100,100,180,100,180,180,100,180};
		Check(runtime.SetHudPresentation({primitive},viewport,error),error.c_str());
		host.draws.clear();runtime.Frame(viewport,.001*tested++);
		Check(host.errors==0,"every HUD symbol and panel compiles without runtime errors");
		if (node.id!="clear") Check(!host.draws.empty(),"HUD artwork produces native retained geometry");
	}
	HudPrimitive item;item.artwork="solid";item.quad={20,20,60,30,70,60,30,50};item.uv={.25f,.25f,.75f,.75f};
	Check(runtime.SetHudPresentation({item},viewport,error),"clipped sheared HUD primitive prepares");
	host.draws.clear();runtime.Frame(viewport,1);
	Check(!host.draws.empty(),"clipped sheared primitive renders");
	for (const auto& draw:host.draws) for (const auto& v:draw.vertices) {
		// Normalised coordinates invert the viewport mapping; every emitted
		// vertex must lie inside all four directed edges of the sheared clip.
		const float x=v.x*640/viewport.width,y=v.y*480/viewport.height;
		for (int e=0;e<4;++e) {
			const int n=(e+1)%4;
			const float side=(item.quad[2*n]-item.quad[2*e])*(y-item.quad[2*e+1])-
				(item.quad[2*n+1]-item.quad[2*e+1])*(x-item.quad[2*e]);
			Check(side>=-.001f,"HUD clipping preserves the exact transformed boundary");
		}
	}
	const auto first=host.draws.front().vertices.front();
	item.colour={.2f,.4f,.8f,.5f};
	Check(runtime.SetHudPresentation({item},viewport,error),"HUD recolour prepares without editing catalog");
	host.draws.clear();runtime.Frame(viewport,2);
	Check(!host.draws.empty() && host.draws.front().vertices.front().a < first.a,"HUD tint updates cached geometry");
	Check(runtime.SetHudPresentation({item},viewport,error),"unchanged frame accepted");
	runtime.Frame(viewport,3);Check(runtime.Statistics().vectorUploads==0,"unchanged HUD geometry is retained");
	item.uv={1,1,0,0};Check(runtime.SetHudPresentation({item},viewport,error),"mirrored HUD art prepares");runtime.Frame(viewport,4);
	item.artwork="missing";Check(!runtime.SetHudPresentation({item},viewport,error),"unknown artwork fails before drawing any part of the HUD");
	host.draws.clear();runtime.Frame(viewport,5);Check(host.draws.empty(),"failed frame cannot present stale or partial geometry");
	item.artwork="solid";viewport.width=720;viewport.height=1280;
	Check(runtime.SetHudPresentation({item},viewport,error),"portrait viewport prepares");runtime.Frame(viewport,6);
	Check(runtime.SetHudPresentation({},viewport,error),"hidden HUD clears presentation");
	host.draws.clear();runtime.Frame(viewport,7);Check(host.draws.empty(),"hidden HUD draws nothing");
	runtime.CloseDocument();
	Check(runtime.LoadDocument(source,resource,diagnostics) && runtime.SetHudPresentation({item},viewport,error),"HUD recreates after document/resource reload");
	runtime.Frame(viewport,8);Check(host.errors==0,"HUD lifetime clean");
	HudPrimitive glyph;glyph.material="hud-test-font";glyph.glyph=true;
	glyph.quad={0,0,32,0,32,32,0,32};glyph.uv={.125f,.25f,.375f,.75f};
	Check(runtime.SetHudPresentation({glyph},viewport,error),"font atlas is prepared before the frame");
	host.draws.clear();runtime.Frame(viewport,9);
	Check(host.draws.size()==1 && Near(host.draws[0].vertices[0].u,.125f,.001f) &&
		Near(host.draws[0].vertices[2].v,.75f,.001f),"glyph atlas UVs survive the retained submission");
	host.missingMaterials.insert("hud-missing-photo");glyph.material="hud-missing-photo";
	Check(!runtime.SetHudPresentation({item,glyph},viewport,error),"a missing picture fails the whole frame before publication");
	host.draws.clear();runtime.Frame(viewport,10);Check(host.draws.empty(),"missing picture cannot leave a partial HUD");
	runtime.Shutdown();
	Check(runtime.LoadDocument(source,resource,diagnostics) && runtime.SetHudPresentation({item},viewport,error),"HUD survives the renderer's shutdown/reload path");
	runtime.Frame(viewport,11);
	std::printf("HUD: %d editable vector symbols/panels exercised\n",tested);
}
