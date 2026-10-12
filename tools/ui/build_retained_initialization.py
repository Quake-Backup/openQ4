#!/usr/bin/env python3
"""Build the layered initialization screen from visual specification 14.17."""
import argparse
import json
import math
import re
from pathlib import Path
import build_retained_screens as ui
import build_retained_scopes as optics

U = ui.U
GLYPHS = json.loads(Path(__file__).with_name('initialization_paths.json').read_text())['glyphs']
TONE = {26:'#171A09',27:'#181B09',28:'#181C09',29:'#191D0A',30:'#1A1E0A',31:'#1C1F0A',
        32:'#1C200B',33:'#1D210B',35:'#1E230C',36:'#1F240C',37:'#1F250C',38:'#20260C',39:'#21270C'}

def sector(ident, inner, outer, start, end, color):
    # Sub-degree chords remain below a pixel at 4K. Every ring is editable.
    steps=max(2, math.ceil(abs(end-start)))
    def point(r,a):
        a=math.radians(a)
        return U*(320+r*math.sin(a)), U*(240-r*math.cos(a))
    points=[point(outer,start+(end-start)*i/steps) for i in range(steps+1)]
    points += [point(inner,end-(end-start)*i/steps) for i in range(steps+1)]
    return ui.path(ident,points,fill=ui.solid(color))

def wordmark():
    paths=[]
    for name,data in GLYPHS.items():
        tokens=re.findall(r'[MZ]|-?\d+(?:\.\d+)?',data)
        commands=[];i=0;move=True
        while i<len(tokens):
            if tokens[i]=='M': move=True;i+=1;continue
            if tokens[i]=='Z': commands.append({'id':f'p{len(commands)}','op':'close'});i+=1;continue
            x=129.42+.3667*float(tokens[i]);y=148.76+.3667*float(tokens[i+1])
            commands.append({'id':f'p{len(commands)}','op':'move' if move else 'line','points':[[U*x,U*y]]})
            i+=2;move=False
        paths.append(optics.compound('letter-'+name,[commands],ui.rgb('#F2F0EC')))
    return paths

def document():
    doc=ui.Document('openq4.initialization')
    doc.state['message']={'type':'string','initial':'#str_104343'}
    doc.state['step']={'type':'number','initial':0}
    doc.state['motion_reduced']={'type':'boolean','initial':False,'cvar':'ui_retainedReducedMotion'}
    def ring(ident,a,b,tone): return optics.ring(ident,(a+b)/2,b-a,ui.rgb(TONE[tone]))
    field=ui.vector('field',dict(ui.FULL),[ui.path('fill',[(0,0),({'fraction':1},0),({'fraction':1},{'fraction':1}),(0,{'fraction':1})],fill=ui.solid(ui.rgb(TONE[26])))])
    # One radially painted disk replaces overlapping annular coverage meshes.
    # Its narrow color steps retain the machined ring edges at any resolution.
    bands=[(0,26),(169,36),(175.5,37),(204.5,38),(210.5,30),(264.8,29),(271.8,28),(273,27),(283.5,28),(285.5,29),(295.5,28),(297.5,27),(300.5,26)]
    stops=[(0,ui.rgb(TONE[26]))]
    for index,(radius,tone) in enumerate(bands[1:]):
        stops.extend([((radius-.08)/301,ui.rgb(TONE[bands[index][1]])),(radius/301,ui.rgb(TONE[tone]))])
    stops.append((1,ui.rgb(TONE[26])))
    disk=optics.compound('machined-rings',[optics.circle(301)],ui.rgb(TONE[26]))
    disk['fill']=ui.linear((U*320,U*240),(U*621,U*240),stops);disk['fill']['type']='radial'
    rings=[disk]
    band=[];teeth=[]
    for c in (0,180):
        for a,b,s,e in ((175.5,183.5,c-45,c+45),(183.5,204.5,c-45,c-43),(183.5,204.5,c+43,c+45)):
            band.append(sector(f'plate-{len(band)}',a,b,s,e,ui.rgb(TONE[39])))
    tooth_bands=[(204.5,39),(210.5,38),(213.5,36),(216.5,35),(233.5,36),(237,33),(244,32),(247.5,32)]
    tooth_stops=[(0,ui.rgb(TONE[39]))]
    for index,(r,tone) in enumerate(tooth_bands[1:]):
        tooth_stops.extend([((r-.08)/248,ui.rgb(TONE[tooth_bands[index][1]])),(r/248,ui.rgb(TONE[tone]))])
    tooth_stops.append((1,ui.rgb(TONE[32])))
    teeth_path=optics.compound('teeth',[sector('tooth',204.5,247.5,k*45-11.25,k*45+11.25,[1,1,1,1])['commands'] for k in range(8)],[1,1,1,1])
    teeth_path['fill']=ui.linear((U*320,U*240),(U*568,U*240),tooth_stops);teeth_path['fill']['type']='radial'
    teeth=[teeth_path]
    sweep=[]
    for a,b,strength in ((161,250,.26),(250,300,.13)):
        stops=[(0,ui.rgb('#5E6E24',0)),(1-67.5/360,ui.rgb('#5E6E24',0))]
        for angle in (60,50,40,30,20,10,7.5,5,2.5):
            amplitude=(1-(angle-7.5)/60)**1.6 if angle>=7.5 else angle/7.5
            stops.append((1-angle/360,ui.rgb('#5E6E24',strength*amplitude)))
        stops.append((1,ui.rgb('#5E6E24',0)))
        p=sector(f'sweep-{len(sweep)}',a,b,-67.5,0,[1,1,1,1]);p['blend']='additive'
        p['fill']=ui.linear((U*320,U*240),(U*320,0),stops);p['fill']['type']='conic';sweep.append(p)
    halo=optics.radial('halo',[(0,.1),(75,.07),(134,.02),(168,0)],'#FF8340','additive')
    ring_layers=[optics.art('rings',rings),optics.art('plate-band',band,transform=ui.rotation(0)),
                 optics.art('teeth',teeth,transform=ui.rotation(0)),optics.art('sweep',sweep,transform=ui.rotation(-38)),optics.art('halo',halo,opacity=ui.number(.8))]
    # These dim olive edges and the soft light sweep need no per-pixel coverage.
    # Their local triangles can rotate intact; lettering and all bright detail
    # keep analytic antialiasing.
    for path in band + teeth + sweep:
        path['antialias'] = False
    for ident,duration,angle in (('plate-band',180000,-360),('teeth',120000,360),('sweep',10000,322)):
        begin=-38 if ident=='sweep' else 0
        doc.timelines.add(ident+'-turn',duration,[ui.track(ident,'transform',[(0,ui.rotation(begin)),(duration,ui.rotation(angle))])],iterations=0)
        doc.events.setdefault('onInit',[]).append({'op':'playTimeline','timeline':ident+'-turn'})
    doc.timelines.add('halo-breathe',6000,[ui.track('halo','opacity',[(0,ui.number(.4)),(3000,ui.number(1)),(6000,ui.number(.4))])],iterations=0)
    doc.events['onInit'].append({'op':'playTimeline','timeline':'halo-breathe'})
    # The source rings sit 25.5 u above the optical center of the 4:3 canvas.
    # Fixed clamps stay above the rotating plates, as in the stock assembly.
    clamps=[]
    for n in range(4):
        clamps.append(sector(f'clamp-{n}',174,211,n*90+41,n*90+49,ui.rgb(TONE[26])))
        clamps.append(sector(f'clamp-edge-{n}',174,211,n*90+41,n*90+42,ui.rgb(TONE[32])))
    ring_layers.append(optics.art('clamps',clamps))
    for n in range(14):
        ident=f'ember-{n}';x=130+(n*71)%370;y=310+(n*37)%90;r=.6+(n%4)*.2
        ember=ui.vector(ident,{**ui.absolute(left=U*x,top=U*y,width=U*r*2,height=U*r*2),
            'opacity':ui.number(0),'transform':ui.transform(),'display':ui.keyword('block')},[ui.path('spark',[(U*r,0),(U*r*2,U*r),(U*r,U*r*2),(0,U*r)],fill=ui.solid(ui.rgb('#FFB070')))])
        duration=6000+(n*379)%5000
        doc.timelines.add(ident+'-rise',duration,[ui.track(ident,'transform',[(0,ui.transform()),(duration,ui.transform(ty=-U*(110+n*7)))]),
            ui.track(ident,'opacity',[(0,ui.number(0)),(duration//4,ui.number(.3)),(duration*3//4,ui.number(.5)),(duration,ui.number(0))])],iterations=0)
        doc.events['onInit'].append({'op':'playTimeline','timeline':ident+'-rise'})
        doc.bind(ident+'.display',ident,'display',{'op':'select','args':[{'state':'motion_reduced'},'none','block']})
        ring_layers.append(ember)
    ring_group=ui.group('ring-assembly',{**ui.FULL,'transform':ui.transform(tx=-U,ty=-25.5*U)},ring_layers)
    emblem=ui.path('emblem',[(U*(190+x),U*(125+y)) for x,y in ui.EMBLEM],fill=ui.solid(ui.rgb('#0E1000')))
    letters=wordmark()
    glow=[]
    for n,width,alpha in ((0,18,.016),(1,13,.027),(2,8,.042)):
        for letter in letters:
            p=json.loads(json.dumps(letter));p['id']=f"glow-{n}-{p['id']}";p['fill']=ui.solid(ui.rgb('#FF8340',alpha))
            p['stroke']=ui.stroke(ui.solid(ui.rgb('#FF8340',alpha)),width,join='round');p['blend']='additive';glow.append(p)
    # A moving narrow light is clipped to the fixed vector letter silhouettes.
    beam=ui.vector('glint-beam',{**ui.FULL,'transform':ui.transform(tx=-U*640)},[ui.path('beam',[(0,0),(U*640,0),(U*640,U*480),(0,U*480)],
        fill=ui.linear((0,0),(U*640,0),[(0,[1,1,1,0]),(.47,[1,1,1,0]),(.50,[1,1,1,.5]),(.53,[1,1,1,0]),(1,[1,1,1,0])]))])
    glint=ui.group('wordmark-glint',{**ui.FULL,'display':ui.keyword('block')},[beam]);glint['mask']={'paths':json.loads(json.dumps(letters))}
    for p in glint['mask']['paths']:p['fill']=ui.solid([1,1,1,1])
    doc.bind('glint.display','wordmark-glint','display',{'op':'select','args':[{'state':'motion_reduced'},'none','block']})
    doc.timelines.add('glint-pass',9000,[ui.track('glint-beam','transform',[(0,ui.transform(tx=-U*640)),(8100,ui.transform(tx=-U*640)),(9000,ui.transform(tx=U*640))])],iterations=0)
    doc.events['onInit'].append({'op':'playTimeline','timeline':'glint-pass'})
    status=ui.label('status','#str_104343',{**ui.absolute(left=U*90,top=U*404,width=U*460,height=U*27),**ui.typeface('marine',16,24,ui.rgb('#F09E0D')),'text-align':ui.keyword('center'),'opacity':ui.number(1)})
    doc.bind('status.text','status','text',{'state':'message'})
    doc.timelines.add('status-arrive',120,[ui.track('status','opacity',[(0,ui.number(.35)),(120,ui.number(1))])])
    doc.events['stepChanged']=[{'op':'playTimeline','timeline':'status-arrive'}]
    segments=[]
    for i in range(7):
        ident=f'progress-{i}'
        p=ui.path('segment',[(0,0),(U*21,0),(U*21,U),(0,U)],fill=ui.solid(ui.rgb('#F09E0D')))
        segments.append(ui.vector(ident,{**ui.absolute(left=U*(236+i*24),top=U*437,width=U*21,height=U),'opacity':ui.number(.15)},[p]))
        doc.bind(ident+'.opacity',ident,'opacity',{'op':'select','args':[{'op':'<=','args':[i,{'state':'step'}]},.55,.15]})
        pulse=ident+'-pulse'
        segments.append(ui.vector(pulse,{**ui.absolute(left=U*(236+i*24),top=U*437,width=U*21,height=U),'opacity':ui.number(.62),'display':ui.keyword('block')},[p]))
        doc.bind(pulse+'.display',pulse,'display',{'op':'select','args':[{'op':'&&','args':[{'op':'==','args':[i,{'state':'step'}]},{'op':'!','args':[{'state':'motion_reduced'}]}]},'block','none']})
        doc.timelines.add(pulse,800,[ui.track(pulse,'opacity',[(0,ui.number(.62)),(400,ui.number(.95)),(800,ui.number(.62))])],iterations=0)
        doc.events['onInit'].append({'op':'playTimeline','timeline':pulse})
    # Explicit reduced-motion rest poses: no shortened infinite animations.
    doc.timelines.add('motion-still',1,[ui.track('plate-band','transform',[(0,ui.rotation(0)),(1,ui.rotation(0))]),
        ui.track('teeth','transform',[(0,ui.rotation(0)),(1,ui.rotation(0))]),ui.track('sweep','transform',[(0,ui.rotation(-38)),(1,ui.rotation(-38))]),
        ui.track('halo','opacity',[(0,ui.number(.8)),(1,ui.number(.8))])])
    doc.events['onInit']=[{'op':'if','condition':{'state':'motion_reduced'},'then':[{'op':'playTimeline','timeline':'motion-still'}],'else':doc.events['onInit']}]
    center=optics.center('canvas',[ring_group,optics.art('emblem-art',[emblem]),optics.art('wordmark-halo',glow),optics.art('wordmark',letters),glint,status,*segments])
    result=doc.build(ui.group('screen',{**ui.FULL,'pointer-events':ui.keyword('none')},[field,center]));result['canvas']['minWidth']=960
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--check',action='store_true');args=p.parse_args()
    target=ui.PAK0/'guis/loading/initialization.q4ui';text=ui.render(document()).replace('build_retained_screens.py','build_retained_initialization.py')
    if args.check:
        if not target.exists() or target.read_text(encoding='utf-8')!=text.replace('\r\n','\n'): raise SystemExit('stale initialization document')
    else:
        target.write_bytes(text.encode('utf-8'));print(target.relative_to(ui.ROOT))

if __name__=='__main__': main()
