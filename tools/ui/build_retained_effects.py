#!/usr/bin/env python3
"""Author game-driven scene focus, recovery and death, without script edits."""
import argparse
import build_retained_screens as ui

def product(*args): return {'op':'*','args':list(args)}
def state(name): return {'state':name}

def document():
    doc=ui.Document('openq4.view-effects')
    for name in ('recovery_amount','death_amount','death_shock','wheel_amount'):
        doc.state[name]={'type':'number','initial':0}
    def filter(ident,source,blur,saturation):
        node=ui.group(ident,{**ui.FULL,'display':ui.keyword('none'),
                      'backdrop-blur':ui.length(0),'backdrop-saturate':ui.number(1)})
        doc.bind(ident+'.display',ident,'display',{'op':'select','args':[{'op':'>','args':[state(source),.001]},'block','none']})
        doc.bind(ident+'.blur',ident,'backdrop-blur',product(state(source),blur))
        doc.bind(ident+'.saturation',ident,'backdrop-saturate',{'op':'-','args':[1,product(state(source),saturation)]})
        return node
    def veil(ident,color,source):
        # Independent view-space edges form a smooth vignette at any aspect;
        # corners naturally darken more without squeezing camera geometry.
        paths=[]
        for n,(start,end) in enumerate((((0,0),({'fraction':1},0)),((0,0),(0,{'fraction':1})))):
            paths.append(ui.path('wash-'+str(n),[(0,0),({'fraction':1},0),({'fraction':1},{'fraction':1}),(0,{'fraction':1})],
              fill=ui.linear(start,end,[(0,ui.rgb(color,.66)),(.1,ui.rgb(color,.34)),(.28,ui.rgb(color,.05)),(.5,ui.rgb(color,0)),(.72,ui.rgb(color,.05)),(.9,ui.rgb(color,.34)),(1,ui.rgb(color,.66))])))
        node=ui.vector(ident,{**ui.FULL,'opacity':ui.number(0)},paths)
        doc.bind(ident+'.opacity',ident,'opacity',state(source))
        return node
    # Isotropic dp sigma, two-pass Gaussian filtering on the live scene. The
    # wheel pass is submitted before the HUD; wheel artwork stays unfiltered.
    nodes=[filter('wheel-focus','wheel_amount',8,.06),
           filter('recovery-focus','recovery_amount',20,.30),
           veil('recovery-periphery','#33271E','recovery_amount'),
           filter('death-focus','death_amount',3.4,.94),
           veil('death-vignette','#08090B','death_amount'),
           veil('death-afterimage','#782B24','death_shock')]
    result=doc.build(ui.group('screen',{**ui.FULL,'pointer-events':ui.keyword('none')},nodes))
    result['canvas']['minWidth']=960
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--check',action='store_true');args=parser.parse_args()
    target=ui.PAK0/'guis/hud/view_effects.q4ui';text=ui.render(document()).replace('build_retained_screens.py','build_retained_effects.py')
    if args.check:
        if not target.exists() or target.read_text(encoding='utf-8')!=text.replace('\r\n','\n'):raise SystemExit('stale view effects document')
    else:
        target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(text.encode('utf-8'));print(target.relative_to(ui.ROOT))

if __name__=='__main__':main()
