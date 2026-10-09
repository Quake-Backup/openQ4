#!/usr/bin/env python3
"""Author the native weapon wheels (visual design 14.6 and 14.16).

Symbols are the atlas's first-party reconstructions, never retail textures.
All inventory, selection, placement and ammo values come from the player.
"""
from __future__ import annotations
import argparse
import json
import math
import re
import xml.etree.ElementTree as ET
from pathlib import Path
import build_retained_screens as ui

ROOT = Path(__file__).resolve().parents[2]
ICONS = json.loads(Path(__file__).with_name('weapon_wheel_icons.json').read_text(encoding='utf-8'))['icons']
ROSTER = ['blaster','machinegun','shotgun','hyperblaster','grenadelauncher','nailgun','rocketlauncher','railgun','lightninggun','darkmattergun','napalmgun']
COLORS = ['#DDEBC3','#FFFF00','#FF8C00','#0073FF','#338F12','#99CCCC','#FF4000','#00FF00','#FFFF99','#C433FF','#FFBF40']
S = lambda key: {'state': key}
op = lambda name, *args: {'op': name, 'args': list(args)}
choose = lambda condition, yes, no: op('select', condition, yes, no)


def at(r, a):
    return (280+r*math.sin(a), 280-r*math.cos(a))


def arc(r, a, b):
    return [at(r, a+(b-a)*i/16) for i in range(17)]


def sector(i, n, inner=104, outer=232, gap=6, strogg=False):
    a=i*math.tau/n; span=math.tau/n; l=a-span/2; r=a+span/2
    d0=math.asin(gap/2/inner); d1=math.asin(gap/2/outer)
    if strogg:
        t=-.07
        return [at(inner,l+d0),at(outer,l+d1+t),at(outer+10,l+d1+t+17/outer),
                at(outer+10,l+.42*span+t),at(outer,l+.42*span+t+17/outer),
                at(outer,r-d1+t-14/outer),at(outer-14,r-d1+t),at(inner,r-d0),
                at(inner,a+.045),at(inner+8,a),at(inner,a-.045)]
    return arc(outer,l+d1,r-d1)+arc(inner,r-d0,l+d0)


def shape(ident, pts, tint, width=None, blend=None, closed=True):
    return ui.path(ident,pts,fill=ui.solid(tint) if width is None else None,
                   stroke=ui.stroke(ui.solid(tint),width,join='round') if width else None,
                   blend=blend,closed=closed)


def circle(ident, radius, tint, width=None):
    result=shape(ident,[],tint,width)
    k=radius*.552284749831
    result['commands']=[{'id':'start','op':'move','points':[[280+radius,280]]}]
    for i,pts in enumerate([[(280+radius,280+k),(280+k,280+radius),(280,280+radius)],
        [(280-k,280+radius),(280-radius,280+k),(280-radius,280)],
        [(280-radius,280-k),(280-k,280-radius),(280,280-radius)],
        [(280+k,280-radius),(280+radius,280-k),(280+radius,280)]]):
        result['commands'].append({'id':'arc'+str(i),'op':'cubic','points':pts})
    result['commands'].append({'id':'close','op':'close'})
    return result


def art(ident, paths, **props):
    return ui.vector(ident, {**ui.FULL,'opacity':ui.number(1),'display':ui.keyword('block'),**props}, paths)


def svg_commands(data):
    """Convert the authored absolute SVG subset, retaining cubic curves.

    Elliptical arcs become <=90 degree cubics, so the native vector renderer
    still controls flattening at the final display density.
    """
    tokens=re.findall(r'[A-Za-z]|[-+]?(?:\d*\.\d+|\d+\.?\d*)(?:[eE][-+]?\d+)?',data)
    result=[]; pos=0; cmd=''; x=y=sx=sy=0
    def emit(kind,*pts): result.append({'id':'p'+str(len(result)),'op':kind,**({'points':[[round(a,5),round(b,5)] for a,b in pts]} if pts else {})})
    while pos<len(tokens):
        if tokens[pos].isalpha(): cmd=tokens[pos];pos+=1
        if cmd=='Z': emit('close');x,y=sx,sy;cmd='';continue
        count={'M':2,'L':2,'H':1,'V':1,'C':6,'Q':4,'A':7}[cmd]
        v=list(map(float,tokens[pos:pos+count]));pos+=count
        if cmd in ('M','L'):
            x,y=v;emit('move' if cmd=='M' else 'line',(x,y))
            if cmd=='M': sx,sy=x,y;cmd='L'
        elif cmd=='H': x=v[0];emit('line',(x,y))
        elif cmd=='V': y=v[0];emit('line',(x,y))
        elif cmd in ('C','Q'):
            emit('cubic' if cmd=='C' else 'quadratic',*zip(v[::2],v[1::2]));x,y=v[-2:]
        elif cmd=='A':
            rx,ry,rot,large,sweep,ex,ey=v;rx,ry=abs(rx),abs(ry)
            if rx==0 or ry==0: emit('line',(ex,ey));x,y=ex,ey;continue
            phi=math.radians(rot);c,s=math.cos(phi),math.sin(phi)
            dx,dy=(x-ex)/2,(y-ey)/2;xp,yp=c*dx+s*dy,-s*dx+c*dy
            scale=max(1,math.sqrt(xp*xp/(rx*rx)+yp*yp/(ry*ry)));rx*=scale;ry*=scale
            den=rx*rx*yp*yp+ry*ry*xp*xp
            f=math.sqrt(max(0,(rx*rx*ry*ry-den)/den)) if den else 0
            if bool(large)==bool(sweep): f=-f
            cxp,cyp=f*rx*yp/ry,-f*ry*xp/rx
            cx,cy=c*cxp-s*cyp+(x+ex)/2,s*cxp+c*cyp+(y+ey)/2
            start=math.atan2((yp-cyp)/ry,(xp-cxp)/rx);end=math.atan2((-yp-cyp)/ry,(-xp-cxp)/rx)
            delta=(end-start)%math.tau
            if not sweep: delta-=math.tau
            steps=max(1,math.ceil(abs(delta)/(math.pi/2)))
            def pt(a): return (cx+c*rx*math.cos(a)-s*ry*math.sin(a),cy+s*rx*math.cos(a)+c*ry*math.sin(a))
            def tangent(a): return (-c*rx*math.sin(a)-s*ry*math.cos(a),-s*rx*math.sin(a)+c*ry*math.cos(a))
            for k in range(steps):
                a=start+delta*k/steps;b=start+delta*(k+1)/steps;f=4/3*math.tan((b-a)/4)
                p0,p1=pt(a),pt(b);t0,t1=tangent(a),tangent(b)
                emit('cubic',(p0[0]+f*t0[0],p0[1]+f*t0[1]),(p1[0]-f*t1[0],p1[1]-f*t1[1]),p1)
            x,y=ex,ey
    return result


def icon(name, tint):
    if name not in ICONS:
        # Expansion symbols: a spiked core / symmetric ice crystal.
        pts=[]
        count=8 if name=='corecannon' else 6
        for i in range(count*2):
            a=i*math.pi/count;r=11 if i%2==0 else 4
            pts.append((12+math.sin(a)*r,12-math.cos(a)*r))
        return [shape('symbol',pts,tint)],24,24
    data=ICONS[name]; _,_,w,h=map(float,data['vb'].split());paths=[]
    for e in ET.fromstring('<svg>'+data['svg']+'</svg>').iter('path'):
        color=tint[:3]+[tint[3]*float(e.get('fill-opacity','1'))]
        paths.append({'id':'symbol-'+str(len(paths)),'commands':svg_commands(e.attrib['d']),
                      'fillRule':e.get('fill-rule','nonzero'),'fill':ui.solid(color)})
    return paths,w,h


def document(strogg=False, awakening=False):
    family='strogg' if strogg else 'marine'; variant=family+('_awakening' if awakening else '')
    doc=ui.Document('openq4.weapon-wheel.'+variant)
    roster=ROSTER[:10]+['corecannon','napalmgun','freezegun'] if awakening else ROSTER
    colors=COLORS[:10]+['#FF5AC8','#FFBF40','#9EEBFF'] if awakening else COLORS
    n=len(roster);accent='#F59512' if strogg else '#B3D06E';ring='#F2AD0B' if strogg else '#B2CC80';selected='#FFCC00' if strogg else '#FF8000'
    for key,kind,initial in [('blend','number',0),('offset','number',0),('fit','number',1),('selected','number',0),('previous','number',0),('mp','boolean',False),('team','number',-1),('weaponName','string',''),('ammo','string','')]:
        doc.state[key]={'type':kind,'initial':initial}
    doc.state['reduced']={'type':'boolean','initial':False,'cvar':'ui_retainedReducedMotion'}
    wheel=[]
    wash=circle('wash',280,[0,0,0,1]);wash['fill']=ui.linear((280,280),(560,280),[(0,[0,0,0,.35]),(.7,[0,0,0,.35]),(1,[0,0,0,0])]);wash['fill']['type']='radial'
    wheel.append(art('wash',[wash]))
    ticks=[shape('tick'+str(k),[at(247,k*math.pi/60),at(252,k*math.pi/60)],ui.rgb(accent,.18),.9,closed=False) for k in range(120)]
    ticks += [shape('boundary'+str(i),[at(242,(i+.5)*math.tau/n),at(253,(i+.5)*math.tau/n)],ui.rgb(accent,.5),1.4,closed=False) for i in range(n)]
    wheel.append(art('bezel',ticks))
    opening=[];closing=[]
    for i,name in enumerate(roster):
        key=f'slot{i}';a=i*math.tau/n;owned=S(key+'.owned');sel=op('==',S('selected'),i);empty=op('==',S(key+'.count'),0)
        for suffix,kind,initial in [('owned','boolean',False),('count','number',0),('fill','number',0),('reserve','string','')]:
            doc.state[key+'.'+suffix]={'type':kind,'initial':initial}
        nodes=[]
        nodes.append(art(key+'-outline',[shape('outline',sector(i,n,strogg=strogg),ui.rgb(accent,.15),1)]))
        plate=art(key+'-plate',[shape('plate',sector(i,n,strogg=strogg),[0,0,0,.55])])
        nodes.append(plate)
        doc.bind(key+'.plate',plate['id'],'opacity',choose(owned,1,0))
        light=art(key+'-selected',[shape('back',sector(i,n,outer=240,strogg=strogg),[0,0,0,.72]),shape('light',sector(i,n,outer=238,strogg=strogg),ui.rgb('#6B3A08' if strogg else '#616F26',.35),blend='additive'),shape('edge',sector(i,n,outer=240,strogg=strogg),ui.rgb(ring),2.4)])
        nodes.append(light);doc.bind(key+'.selected',light['id'],'opacity',choose(op('&&',owned,sel),1,0))
        # Team identity belongs to the selection border, not the SP HUD family.
        if not strogg:
            for team,col in [(0,'#AAE355'),(1,'#FF8E00')]:
                ident=key+'-team'+str(team);nodes.append(art(ident,[shape('edge',sector(i,n,outer=240),ui.rgb(col),2.4)]))
                doc.bind(ident,ident,'opacity',choose(op('&&',sel,op('&&',S('mp'),op('==',S('team'),team))),1,0))
        contents=[]
        def text(ident,value,r,size,tint,face='chain',dy=0):
            x,y=at(r,a);return ui.label(ident,'#str_230007',{**ui.absolute(left=x-38,top=y-size*.6+dy,width=76,height=size*1.5),**ui.typeface(face,size,size*1.2,ui.rgb(tint)),'text-align':ui.keyword('center')})
        nodes.append(text(key+'-index','#str_230007',118,12,accent,'lowpixel'))
        doc.state[key+'.index']={'type':'string','initial':''}
        doc.bind(key+'.index',key+'-index','text',S(key+'.index'))
        ix,iy=at(182 if strogg else 158,a)
        active=op('&&',sel,op('!',empty))
        for suffix,col in [('idle',ui.rgb(accent if strogg else '#A8A360')),('lit',ui.rgb(selected)),('mp',ui.rgb('#00D9FF' if i==0 else colors[i]))]:
            paths,w,h=icon('gauntlet' if suffix=='mp' and i==0 else name,col);sz=26
            for path in paths:
                for cmd in path['commands']:
                    if 'points' in cmd: cmd['points']=[[x*sz/h,y*sz/h] for x,y in cmd['points']]
            ident=key+'-icon-'+suffix
            contents.append(ui.vector(ident,{**ui.absolute(left=ix-w*sz/h/2,top=iy-sz/2,width=w*sz/h,height=sz),'opacity':ui.number(1)},paths))
            normal=choose(active,1 if suffix=='lit' else 0, .6 if suffix=='idle' else 0)
            multi=choose(active,1,.6) if suffix=='mp' else 0
            doc.bind(ident,ident,'opacity',choose(S('mp'),multi,normal))
        ammo=text(key+'-reserve','',142 if strogg else 193,17,'#FCFFC8' if strogg else '#DDEBC3','r_strogg' if strogg else 'chain')
        contents.append(ammo);doc.bind(key+'.reserve',ammo['id'],'text',S(key+'.reserve'))
        for cell in range(8):
            lo=a-math.pi/n+.045+(math.tau/n-.09)*cell/8;hi=lo+(math.tau/n-.09)/8-.008
            pts=arc(221,lo,hi)+arc(207,hi,lo)
            tint='#FF9000' if strogg else colors[i]
            contents.append(art(f'{key}-track{cell}',[shape('track',pts,ui.rgb(tint,.2))]))
            ident=f'{key}-cell{cell}';contents.append(art(ident,[shape('fill',pts,ui.rgb(tint,.45))]))
            doc.bind(ident,ident,'opacity',op('clamp',op('-',op('*',S(key+'.fill'),8),cell),0,1))
        slash=art(key+'-empty',[shape('circle',[(ix+12*math.sin(t),iy-12*math.cos(t)) for t in [j*math.tau/32 for j in range(33)]],ui.rgb('#C72C1B'),1.6),shape('slash',[(ix-8,iy+8),(ix+8,iy-8)],ui.rgb('#C72C1B'),2,closed=False)])
        contents.append(slash);doc.bind(key+'.empty',slash['id'],'opacity',choose(empty,1,0))
        content=ui.group(key+'-contents',dict(ui.FULL,opacity=ui.number(1)),contents);nodes.append(content);doc.bind(key+'.contents',content['id'],'opacity',choose(owned,1,0))
        notch=art(key+'-previous',[shape('notch',[at(97,a-.028),at(102,a),at(97,a+.028)],ui.rgb(accent,.8))]);nodes.append(notch);doc.bind(key+'.previous',notch['id'],'opacity',choose(op('==',S('previous'),i),1,0))
        entry=ui.group(key+'-entry',dict(ui.FULL,opacity=ui.number(1),transform=ui.transform()),nodes);wheel.append(entry)
        delay=(i*7 if not strogg else ((i*7)%n)*12)
        opening += [ui.track(entry['id'],'opacity',[(0,ui.number(0)),(delay,ui.number(0))] [0:1] + ([(delay,ui.number(0))] if delay else []) + [(delay+90,ui.number(1))])]
        closing += [ui.track(entry['id'],'opacity',[(0,ui.number(1)),(60,ui.number(0))])]
    hubpaths=[circle('disc',86,[0,0,0,.55]) if not strogg else shape('disc',[at(86,j*math.tau/8+math.pi/8) for j in range(8)],[0,0,0,.55])]
    hubpaths += [circle('ring'+str(r),r,ui.rgb(accent,.35),1) for r in (80,86)]
    for k in range(16):
        x,y=at(93,k*math.tau/16);hubpaths.append(shape('dot'+str(k),[(x-1,y-1),(x+1,y-1),(x+1,y+1),(x-1,y+1)],ui.rgb(accent,.5)))
    for k in range(-20,21):
        y=k*3.6;half=math.sqrt(max(0,78*78-y*y));hubpaths.append(shape('scan'+str(k),[(280-half,280+y),(280+half,280+y)],ui.rgb(accent,.11),.7,closed=False))
    hub=ui.group('hub',dict(ui.FULL,transform=ui.transform()),[art('hub-art',hubpaths)])
    if strogg:
        for radius,secs in [(94,5),(100,-2)]:
            ident='orbit'+str(radius);paths=[shape('dash'+str(i),arc(radius,i*math.pi/12,i*math.pi/12+.13),ui.rgb(accent,.3),1,closed=False) for i in range(24)]
            wheel.append(art(ident,paths,transform=ui.transform()));doc.timelines.add(ident,abs(secs)*1000,[ui.track(ident,'transform',[(0,ui.rotation(0)),(abs(secs)*1000,ui.rotation(360 if secs>0 else -360))])],iterations=0)
        doc.events['onInit']=[{'op':'playTimeline','timeline':'orbit'+str(r)} for r in (94,100)]
    wheel.append(hub)
    for ident,top,size,face in [('name',253,12,'r_strogg' if strogg else 'marine'),('ammo',289,21,'r_strogg' if strogg else 'chain')]:
        hub['children'].append(ui.label('hub-'+ident,'#str_230007',{**ui.absolute(left=211,top=top,width=138,height=34),**ui.typeface(face,size,size*1.3,ui.rgb('#FCFFC8' if strogg else '#B0C891')),'opacity':ui.number(1),'text-align':ui.keyword('center'),'white-space':ui.keyword('normal')}))
        doc.bind('hub.'+ident,'hub-'+ident,'text',S('weaponName' if ident=='name' else ident))
    pointer=art('pointer',[shape('arrow',[(274,207),(280,198),(286,207)],ui.rgb(selected))],transform=ui.transform());wheel.append(pointer)
    doc.bind('pointer.rotation','pointer','transform',[0,0,1,1,op('*',S('selected'),360/n)])
    opening += [ui.track('hub','transform',[(0,ui.transform(sy=.02)),(150,ui.transform(),ui.EASE_OUT)])]
    closing += [ui.track('hub','transform',[(0,ui.transform()),(150,ui.transform(sy=.02))])]
    doc.timelines.add('open',max(250,(n-1)*12+90),opening)
    doc.timelines.add('close',150,closing)
    for event in ('open','close'): doc.events[event]=[{'op':'playTimeline','timeline':event}]
    doc.timelines.add('selection',200,[ui.track('hub-name','opacity',[(0,ui.number(.15)),(120,ui.number(1))])])
    doc.events['selection']=[{'op':'playTimeline','timeline':'selection'}]
    props={**ui.absolute(width=560,height=560),'left':ui.length(50,'%'),'top':ui.length(50,'%'),'margin-left':ui.length(-280),'margin-top':ui.length(-280),'transform':ui.transform()}
    root=ui.group('wheel-root',dict(ui.FULL,opacity=ui.number(0)),[ui.group('wheel-center',props,wheel)])
    doc.bind('root.blend','wheel-root','opacity',S('blend'))
    doc.bind('wheel.placement','wheel-center','transform',[S('offset'),0,S('fit'),S('fit'),0])
    return doc.build(root,{'extensions':{'openq4':{'reference':'ui-visual-design.md 14.6 / 14.16','family':family,'slots':n,'input':'Player-owned radial selection; no pointer hit testing','placement':'ui_weaponWheelOffset percent of viewport width from center, default +27; fitted at narrow edges'}}})


# The quick-chat wheel's phrases clockwise from the top (#str_231261-231268),
# the channel words and the zoom control's verb.
PHRASES = ['#str_2312' + str(61 + i) for i in range(8)]
ALL_KEY, TEAM_KEY, CHANNEL_KEY = '#str_231254', '#str_231001', '#str_231256'
# Your team's text color in multiplayer (14.6) and its selection light.
TEAM_TEXT = [('#AAE355', '#616F26'), ('#FF8E00', '#6B3A08')]


def held(node, prop, keys):
    """A track that starts where it is authored: a play retargets the first
    key to the property's current value, so a copy of that key 1 ms later
    makes the authored start take effect at once."""
    if len(keys) >= 2 and keys[1][0] > 1:
        keys = [keys[0], (1, keys[0][1])] + list(keys[1:])
    return ui.track(node, prop, keys)


def hub_art(accent):
    """The Marine hub: a black 0.55 disc inside the machinegun scope's double
    ring, its 16 compass dots and the signal screen's scanlines."""
    paths = [circle('disc', 86, [0, 0, 0, .55])] + [circle('ring' + str(r), r, ui.rgb(accent, .35), 1) for r in (80, 86)]
    for k in range(16):
        x, y = at(93, k * math.tau / 16)
        paths.append(shape('dot' + str(k), [(x - 1, y - 1), (x + 1, y - 1), (x + 1, y + 1), (x - 1, y + 1)], ui.rgb(accent, .5)))
    for k in range(-20, 21):
        y = k * 3.6
        half = math.sqrt(max(0, 78 * 78 - y * y))
        paths.append(shape('scan' + str(k), [(280 - half, 280 + y), (280 + half, 280 + y)], ui.rgb(accent, .11), .7, closed=False))
    return paths


def chat_document():
    """The quick-chat wheel (14.6 Quick chat, 13.5): the multiplayer weapon
    wheel's ring, bezel, hub and selection with eight localized phrases in
    Lowpixel 16 dp in place of the weapons. The hub names the channel, ALL in
    #FFFF8D or TEAM in your team's text color; in a team game the zoom
    control's key switches it. The selection ring, its light and the pointer
    take your team's text color in a team game, the stock olive otherwise.
    The game publishes the selection, channel, team, placement and blend."""
    from remastered_hud_base import HudDocument
    from remastered_hud_mp import glyph_chip
    doc = HudDocument('openq4.quick-chat')
    n = len(PHRASES)
    accent, ring = '#B3D06E', '#B2CC80'
    for key, kind, initial in [('blend', 'number', 0), ('offset', 'number', 0), ('fit', 'number', 1), ('selected', 'number', -1),
                               ('team', 'number', -1), ('channel', 'number', 0), ('zoomKey', 'string', '')]:
        doc.state[key] = {'type': kind, 'initial': initial}
    doc.state['reduced'] = {'type': 'boolean', 'initial': False, 'cvar': 'ui_retainedReducedMotion'}
    team = S('team')
    selections = [('olive', op('<', team, 0), ring, '#616F26')]
    selections += [(f'team{t}', op('==', team, t), edge, glow) for t, (edge, glow) in enumerate(TEAM_TEXT)]
    wheel = []
    wash = circle('wash', 280, [0, 0, 0, 1])
    wash['fill'] = ui.linear((280, 280), (560, 280), [(0, [0, 0, 0, .35]), (.7, [0, 0, 0, .35]), (1, [0, 0, 0, 0])])
    wash['fill']['type'] = 'radial'
    wheel.append(art('wash', [wash]))
    ticks = [shape('tick' + str(k), [at(247, k * math.pi / 60), at(252, k * math.pi / 60)], ui.rgb(accent, .18), .9, closed=False)
             for k in range(120)]
    ticks += [shape('boundary' + str(i), [at(242, (i + .5) * math.tau / n), at(253, (i + .5) * math.tau / n)], ui.rgb(accent, .5), 1.4,
                    closed=False) for i in range(n)]
    wheel.append(art('bezel', ticks))
    opening, closing = [], []
    for i, phrase in enumerate(PHRASES):
        key = f'phrase{i}'
        a = i * math.tau / n
        sel = op('==', S('selected'), i)
        nodes = [art(key + '-outline', [shape('outline', sector(i, n), ui.rgb(accent, .15), 1)]),
                 art(key + '-plate', [shape('plate', sector(i, n), [0, 0, 0, .55])])]
        for suffix, condition, edge, glow in selections:
            ident = f'{key}-selected-{suffix}'
            nodes.append(art(ident, [shape('back', sector(i, n, outer=240), [0, 0, 0, .72]),
                                     shape('light', sector(i, n, outer=238), ui.rgb(glow, .35), blend='additive'),
                                     shape('edge', sector(i, n, outer=240), ui.rgb(edge), 2.4)]))
            doc.bind(ident, ident, 'opacity', choose(op('&&', sel, condition), 1, 0))
        # The phrase across the segment's middle, on two lines when it needs them.
        x, y = at(168, a)
        text = ui.label(key + '-text', phrase, {'position': ui.keyword('relative'), 'display': ui.keyword('block'), 'width': ui.length(106),
                                                **ui.typeface('lowpixel', 16, 19, ui.rgb('#DDEBC3')), 'text-align': ui.keyword('center'),
                                                'white-space': ui.keyword('normal'), 'opacity': ui.number(1)})
        doc.bind(key + '.text', key + '-text', 'opacity', choose(sel, 1, .6))
        nodes.append(ui.group(key + '-label', {**ui.absolute(left=x - 56, top=y - 24, width=112, height=48), 'display': ui.keyword('flex'),
                                               'flex-direction': ui.keyword('column'), 'justify-content': ui.keyword('center'),
                                               'align-items': ui.keyword('center')}, [text]))
        entry = ui.group(key + '-entry', dict(ui.FULL, opacity=ui.number(1), transform=ui.transform()), nodes)
        wheel.append(entry)
        delay = i * 8
        opening.append(held(entry['id'], 'opacity', [(0, ui.number(0))] + ([(delay, ui.number(0))] if delay else []) +
                            [(delay + 90, ui.number(1))]))
        closing.append(held(entry['id'], 'opacity', [(0, ui.number(1)), (60, ui.number(0))]))
    hub = ui.group('hub', dict(ui.FULL, transform=ui.transform()), [art('hub-art', hub_art(accent))])
    # The channel: ALL, or TEAM in your team's text color.
    channel = S('channel')
    words = [('all', ALL_KEY, '#FFFF8D', op('==', channel, 0))]
    words += [(f'team{t}', TEAM_KEY, edge, op('&&', op('==', channel, 1), op('==', team, t))) for t, (edge, _) in enumerate(TEAM_TEXT)]
    for suffix, word, colour, shown in words:
        ident = 'hub-' + suffix
        hub['children'].append(ui.label(ident, word, {**ui.absolute(left=211, top=258, width=138, height=24),
                                                      **ui.typeface('marine', 16, 20, ui.rgb(colour)), 'text-align': ui.keyword('center'),
                                                      'opacity': ui.number(1), 'display': ui.keyword('none')}))
        doc.bind(ident + '.show', ident, 'display', choose(shown, 'block', 'none'))
    # In a team game the zoom control switches the channel: its keycap or the
    # controller button's glyph (the HUD prompts' construction, 13.4), 16 dp.
    chip = glyph_chip(doc, 'hub', S('zoomKey'), h=16 / 1.5, size=6.4, cut=8 / 3, pad=(8 / 3, 8 / 3), gap=10 / 3, min_width=16 / 1.5)
    verb = ui.label('hub-switch', CHANNEL_KEY, {'position': ui.keyword('relative'), 'display': ui.keyword('block'),
                                                 **ui.typeface('lowpixel', 12, 16, ui.rgb('#B0C891')), 'white-space': ui.keyword('nowrap')})
    hint = ui.group('hub-hint', {**ui.absolute(left=200, top=288, width=160, height=16), 'display': ui.keyword('none'),
                                 'flex-direction': ui.keyword('row'), 'justify-content': ui.keyword('center'),
                                 'align-items': ui.keyword('center')}, [chip, verb])
    doc.bind('hub.hint', 'hub-hint', 'display', choose(op('>=', team, 0), 'flex', 'none'))
    hub['children'].append(hint)
    wheel.append(hub)
    # The pointer on the inner ring faces the selection.
    for suffix, condition, edge, _ in [('olive', op('<', team, 0), '#FF8000', None)] + selections[1:]:
        ident = 'pointer-' + suffix
        wheel.append(art(ident, [shape('arrow', [(274, 207), (280, 198), (286, 207)], ui.rgb(edge))], transform=ui.transform()))
        doc.bind(ident + '.rotation', ident, 'transform', [0, 0, 1, 1, op('*', op('max', S('selected'), 0), 360 / n)])
        doc.bind(ident + '.show', ident, 'opacity', choose(op('&&', op('>=', S('selected'), 0), condition), 1, 0))
    opening.append(held('hub', 'transform', [(0, ui.transform(sy=.02)), (150, ui.transform(), ui.EASE_OUT)]))
    closing.append(held('hub', 'transform', [(0, ui.transform()), (150, ui.transform(sy=.02))]))
    doc.timelines.add('open', (n - 1) * 8 + 150, opening)
    doc.timelines.add('close', 150, closing)
    for event in ('open', 'close'):
        doc.events[event] = [{'op': 'playTimeline', 'timeline': event}]
    # A new channel re-tunes the hub's word.
    doc.timelines.add('channel', 200, [held(ident, 'opacity', [(0, ui.number(.15)), (120, ui.number(1))])
                                       for ident in ('hub-all', 'hub-team0', 'hub-team1')])
    doc.events['channel'] = [{'op': 'playTimeline', 'timeline': 'channel'}]
    props = {**ui.absolute(width=560, height=560), 'left': ui.length(50, '%'), 'top': ui.length(50, '%'), 'margin-left': ui.length(-280),
             'margin-top': ui.length(-280), 'transform': ui.transform()}
    root = ui.group('wheel-root', dict(ui.FULL, opacity=ui.number(0)), [ui.group('wheel-center', props, wheel)])
    doc.bind('root.blend', 'wheel-root', 'opacity', S('blend'))
    doc.bind('wheel.placement', 'wheel-center', 'transform', [S('offset'), 0, S('fit'), S('fit'), 0])
    return doc.build(root, {'extensions': {'openq4': {'reference': 'ui-visual-design.md 14.6 Quick chat / 13.5', 'phrases': n,
                                                      'input': 'Player-owned radial selection; no pointer hit testing',
                                                      'placement': 'ui_weaponWheelOffset, as the weapon wheel'}}})


def write(path, document, check):
    text = '// Generated by tools/ui/build_retained_weapon_wheel.py; editable vector art.\n' + json.dumps(document, ensure_ascii=False, indent=2) + '\n'
    text = re.sub(r'\[\n\s+(-?\d+(?:\.\d+)?),\n\s+(-?\d+(?:\.\d+)?)\n\s+\]', r'[\1, \2]', text)
    if check:
        assert path.read_text(encoding='utf-8') == text, path
    else:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding='utf-8', newline='\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--check',action='store_true');args=parser.parse_args()
    for strogg in (False,True):
        for awakening in (False,True):
            name=('strogg' if strogg else 'marine')+('_awakening' if awakening else '')
            path=ROOT/'content/baseoq4/pak0/guis/hud'/('weapon_wheel_'+name+'.q4ui')
            write(path,document(strogg,awakening),args.check)
    write(ROOT/'content/baseoq4/pak0/guis/hud/quick_chat.q4ui',chat_document(),args.check)
    print('Native weapon wheel documents are current')


if __name__=='__main__': main()
