"""Shared fourth-level menu construction and reversible depth choreography."""

def top_at(ui, level):
    return ui.transform((323+(level-2)*ui.SUBPAGE_STEP)*ui.U,-63*ui.U)

def fit_page(ui, root):
    """Keep the two frame edges on the viewport and center the isotropic body."""
    for node in root['children']:
        if node['id']=='band-bottom':
            for path in node['paths']:
                for command in path['commands']:
                    for point in command.get('points',[]):
                        if isinstance(point[1],(int,float)):
                            point[1]={'fraction':1,'dp':round(point[1]-720,4)}
        elif node['id']=='prompts':
            node['properties'].pop('top')
            node['properties']['bottom']=ui.length(720-ui.U*447-22)
        elif node['id']=='content':
            node['properties']['top']=ui.length(50,'%')
            node['properties']['margin-top']=ui.length(-360)
    return root

def breadcrumb(ui, doc, ident, key, slot):
    """The exact resting pose used when a title becomes a breadcrumb."""
    fits=ui.crumb_fits(key)
    for suffix,scale in [('',ui.CRUMB_SCALE)]+[('-'+lang,fit) for lang,fit in fits.items()]:
        doc.timelines.add(ident+'Rest'+suffix,1,[ui.track(ident,'transform',[(0,ui.crumb_transform(scale)),(1,ui.crumb_transform(scale))])])
    doc.events.setdefault('onActivate',[]).extend(ui.by_language(doc,ident+'Rest',fits))
    return ui.group(ident+'-clip',{**ui.absolute(left=ui.U*slot,top=0,width=ui.U*ui.CRUMB_SPAN,height=ui.U*40),'overflow':ui.keyword('hidden')},[
        ui.label(ident,key,{**ui.absolute(left=0,top=ui.U*19,width=ui.U*ui.PAGE_TITLE_W,height=ui.U*ui.PAGE_TITLE_H),
            **ui.typeface('marine',18,ui.U*ui.PAGE_TITLE_H,[1,1,1,.4]),'white-space':ui.keyword('nowrap'),'transform':ui.crumb_transform()})])

def child_transition(ui, doc, level, choices, parent_key):
    """A page may descend one notch, carrying the selected child's label.

    Use for every route from a sub-page to a detail page. Modal controls and
    sibling tabs keep their level. Back can retarget any in-flight descent.
    """
    U=ui.U;slot=39+(level-1)*97
    doc.state['detail_title']={'type':'string','initial':choices[0][1]}
    carry=ui.label('detail-carry',choices[0][1],{**ui.absolute(left=U*slot,top=U*19,width=U*314,height=U*24.4),
        **ui.typeface('marine',24,U*24.4,ui.rgb(ui.ORANGE)),'white-space':ui.keyword('nowrap'),'opacity':ui.number(0),'transform':ui.transform()})
    doc.bind('detail-carry.text','detail-carry','text',{'state':'detail_title'})
    end=ui.transform(tx=-.25*U*314/2,ty=-.25*U*24.4/2,sx=.75,sy=.75)
    fits=ui.crumb_fits(parent_key)
    for suffix,scale in [('',ui.CRUMB_SCALE)]+[('-'+lang,fit) for lang,fit in fits.items()]:
        doc.timelines.add('detailCrumb'+suffix,350,[ui.track('title','transform',[(0,ui.transform()),(150,ui.crumb_transform(scale)),(350,ui.crumb_transform(scale))])])
    for index,(name,key) in enumerate(choices):
        start=ui.transform(tx=U*(44-slot),ty=U*(192.8+30*index-19))
        doc.timelines.add('detailEnter'+name,350,[
            ui.track('content','opacity',[(0,ui.number(1)),(150,ui.number(0)),(350,ui.number(0))]),
            ui.track('content','transform',[(0,ui.transform()),(300,ui.transform(tx=U*640)),(350,ui.transform(tx=U*640))]),
            ui.track('band-top','transform',[(0,top_at(ui,level)),(50,top_at(ui,level),ui.ACCEL),(350,top_at(ui,level+1))]),
            ui.track('detail-title-clip','width',[(0,ui.length(U*ui.PAGE_TITLE_W)),(150,ui.length(U*ui.CRUMB_SPAN)),(350,ui.length(U*ui.CRUMB_SPAN))]),
            ui.track('title','color',[(0,ui.colour([1,1,1,.5])),(150,ui.colour([1,1,1,.4])),(350,ui.colour([1,1,1,.4]))]),
            ui.track('detail-carry','opacity',[(0,ui.number(1)),(350,ui.number(1))]),
            ui.track('detail-carry','transform',[(0,start),(50,start,ui.ACCEL),(350,end)]),
            ui.track('detail-carry','color',[(0,ui.colour(ui.rgb(ui.ORANGE))),(50,ui.colour(ui.rgb(ui.ORANGE)),ui.ACCEL),(350,ui.colour([1,1,1,.5]))])])
        doc.events['detailEnter'+name]=[{'op':'setState','values':{'detail_title':key}},{'op':'playTimeline','timeline':'detailEnter'+name},*ui.by_language(doc,'detailCrumb',fits)]
    for duration in ui.SUBPAGE_REVERSE_MS:
        doc.timelines.add('detailReverse'+str(duration),duration,[
            ui.track('content','opacity',[(0,ui.number(0)),(duration,ui.number(1))]),
            ui.track('content','transform',[(0,ui.transform(tx=U*640)),(duration,ui.transform())]),
            ui.track('band-top','transform',[(0,top_at(ui,level+1),ui.ACCEL),(duration,top_at(ui,level))]),
            ui.track('title','transform',[(0,ui.crumb_transform()),(duration,ui.transform())]),
            ui.track('detail-title-clip','width',[(0,ui.length(U*ui.CRUMB_SPAN)),(duration,ui.length(U*ui.PAGE_TITLE_W))]),
            ui.track('title','color',[(0,ui.colour([1,1,1,.4])),(duration,ui.colour([1,1,1,.5]))]),
            ui.track('detail-carry','opacity',[(0,ui.number(1)),(duration,ui.number(0))])])
        doc.events['detailReverse'+str(duration)]=[{'op':'playTimeline','timeline':'detailReverse'+str(duration)}]
    # A cached parent always returns to its exact rest pose on deactivation.
    doc.timelines.add('detailReset',1,[
        ui.track('title','transform',[(0,ui.transform()),(1,ui.transform())]),
        ui.track('detail-title-clip','width',[(0,ui.length(U*ui.PAGE_TITLE_W)),(1,ui.length(U*ui.PAGE_TITLE_W))]),
        ui.track('title','color',[(0,ui.colour([1,1,1,.5])),(1,ui.colour([1,1,1,.5]))]),
        ui.track('detail-carry','opacity',[(0,ui.number(0)),(1,ui.number(0))])])
    doc.events.setdefault('onDeactivate',[]).append({'op':'playTimeline','timeline':'detailReset'})
    return carry

def skill_document(ui):
    U=ui.U;doc=ui.Document('openq4.campaign-skill')
    doc.state.update({'skill':{'type':'number','initial':1},'turbo':{'type':'boolean','initial':False},
        'campaign_title':{'type':'string','initial':'#str_230039'},'skill_description':{'type':'string','initial':'#str_200019'}})
    skill_keys=['#str_200014','#str_200015','#str_200016','#str_200017','#str_41117']
    buttons=[]
    for i,key in enumerate(skill_keys):
        action=doc.session('skill'+str(i),'campaignSkill'+str(i))
        row=ui.navigation_plate(doc,'skill'+str(i),key,action=action)
        mark=ui.vector('selected'+str(i),{**ui.absolute(left=ui.NAV_LEAD+U*25,top=U*10,width=U*5,height=U*10),'opacity':ui.number(0)},
                       [ui.path('rail',[(0,0),(U*3,0),(U*3,U*10),(0,U*10)],fill=ui.solid(ui.rgb(ui.ORANGE)))])
        row['children'].append(mark);doc.bind('selected'+str(i)+'.opacity','selected'+str(i),'opacity',{'op':'select','args':[{'op':'==','args':[{'state':'skill'},i]},1,0]})
        buttons.append(row)
    for ident,key,command in [('turbo','#str_41119','campaignSkillTurbo'),('start','#str_200020','campaignSkillStart'),('back','#str_230045','campaignSkillBack')]:
        doc.session(ident,command);buttons.append(ui.navigation_plate(doc,ident,key,action=ident))
    doc.events['onBack']=[{'op':'action','action':'back'}]
    bands=ui.framing_bands('band');bands[0]['properties']['transform']=top_at(ui,4);bands[1]['properties']['transform']=ui.transform(*ui.BOTTOM_TO_PAGE)
    bounds={**ui.absolute(top=0,width=ui.CANVAS_W,height=720),'left':ui.length(50,'%'),'margin-left':ui.length(-ui.CANVAS_W/2)}
    content=ui.group('content',{**bounds,'transform':ui.transform(),'opacity':ui.number(0)},[
        ui.group('navigation',{**ui.absolute(left=0,top=U*172,width=U*357),'display':ui.keyword('flex'),'flex-direction':ui.keyword('column')},buttons),
        ui.label('description','#str_200019',{**ui.absolute(left=U*387,top=U*184,width=U*226,height=U*148),**ui.typeface('lowpixel',18,26,ui.rgb(ui.OLIVE))}),
        ui.label('turbo-value','#str_41121',{**ui.absolute(left=U*270,top=U*325,width=U*77,height=U*25),**ui.typeface('marine',18,26,ui.rgb(ui.ORANGE)),'pointer-events':ui.keyword('none')})])
    doc.bind('description.text','description','text',{'state':'skill_description'})
    doc.bind('turbo-value.text','turbo-value','text',{'op':'select','args':[{'state':'turbo'},'#str_41120','#str_41121']})
    titles=[]
    for i,key in enumerate(('#str_42000','#str_230038')):
        titles.append(breadcrumb(ui,doc,'crumb'+str(i),key,39+97*i))
    titles.append(ui.page_title('title','#str_230039',233,19))
    titles[-1]['properties']['opacity']=ui.number(1)
    doc.bind('title.text','title','text',{'state':'campaign_title'})
    title_group=ui.group('titles',bounds,titles)
    doc.timelines.add('arrive',150,[ui.track('content','opacity',[(0,ui.number(0)),(150,ui.number(1))])])
    doc.events['onActivate'].append({'op':'playTimeline','timeline':'arrive'})
    doc.timelines.add('detailLeave',300,[
        ui.track('title','opacity',[(0,ui.number(0)),(300,ui.number(0))]),
        ui.track('content','opacity',[(0,ui.number(1)),(250,ui.number(0)),(300,ui.number(0))]),
        ui.track('content','transform',[(0,ui.transform()),(300,ui.transform(tx=-U*640))]),
        ui.track('band-top','transform',[(0,top_at(ui,4),ui.ACCEL),(300,top_at(ui,3))]),
        ui.track('crumb1-clip','width',[(0,ui.length(U*ui.CRUMB_SPAN)),(150,ui.length(U*ui.CRUMB_SPAN)),(151,ui.length(U*ui.PAGE_TITLE_W)),(300,ui.length(U*ui.PAGE_TITLE_W))]),
        ui.track('crumb1','color',[(0,ui.colour([1,1,1,.4])),(150,ui.colour([1,1,1,.4])),(300,ui.colour([1,1,1,.5]))])])
    fits=ui.crumb_fits('#str_230038')
    for suffix,scale in [('',ui.CRUMB_SCALE)]+[('-'+lang,fit) for lang,fit in fits.items()]:
        doc.timelines.add('detailCrumbReturn'+suffix,300,[ui.track('crumb1','transform',[(0,ui.crumb_transform(scale)),(150,ui.crumb_transform(scale)),(300,ui.transform())])])
    doc.events['detailLeave']=[{'op':'playTimeline','timeline':'detailLeave'},*ui.by_language(doc,'detailCrumbReturn',fits)]
    doc.timelines.add('rest',1,[ui.track('content','transform',[(0,ui.transform()),(1,ui.transform())]),
        ui.track('title','opacity',[(0,ui.number(1)),(1,ui.number(1))]),ui.track('band-top','transform',[(0,top_at(ui,4)),(1,top_at(ui,4))]),
        ui.track('crumb1','color',[(0,ui.colour([1,1,1,.4])),(1,ui.colour([1,1,1,.4]))]),
        ui.track('crumb1-clip','width',[(0,ui.length(U*ui.CRUMB_SPAN)),(1,ui.length(U*ui.CRUMB_SPAN))])])
    doc.events['onDeactivate']=[{'op':'playTimeline','timeline':'rest'}]
    result=doc.build(fit_page(ui,ui.group('screen',{**ui.FULL,'background-color':ui.colour([0,0,0,1])},[
        *ui.lit_field('field',.7),*bands,content,title_group,
        ui.prompt_bar(doc,[('#str_107019','#str_200747'),('#str_107020','#str_230045')])])))
    result['canvas']['minWidth']=960
    return result
