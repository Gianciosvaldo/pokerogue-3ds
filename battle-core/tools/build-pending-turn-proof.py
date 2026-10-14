#!/usr/bin/env python3
"""Actual Healing Wish/Lunar Dance move, faint, switch and entry turns."""
from pathlib import Path
import subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
 base=Path(tmp)/'base';subprocess.run([sys.executable,str(root/'tools/build-transform-turn-proof.py'),sys.argv[1],str(base)],check=True);s=base.read_text()
if sys.argv[1]=='producer':
 s=s.replace('TRANSFORMTURN','PENDINGTURN').replace('actual Transform full move turns','actual pending healing full move turns')
 s=s.replace('    game.override.moveset([]).enemyMoveset([]);','    game.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);')
 a=s.index('        holder.species=speciesDataRegistry.getSpecies(SpeciesId.DITTO);');b=s.index('        let before: string[];',a)
 s=s[:a]+'''        holder.species=speciesDataRegistry.getSpecies(SpeciesId.BULBASAUR);holder.formIndex=0;
        other.species=speciesDataRegistry.getSpecies(SpeciesId.MAGIKARP);other.formIndex=0;
        for(const p of [holder,other]){p.customPokemonData.ability=AbilityId.BALL_FETCH;p.summonData.ability=undefined;p.level=30+rnd(21);p.moveset=[new PokemonMove(MoveId.HEALING_WISH),new PokemonMove(MoveId.LUNAR_DANCE),new PokemonMove(MoveId.TACKLE),new PokemonMove(MoveId.SPLASH)];p.calculateStats();p.hp=p.getMaxHp();p.generateName();}
        for(const p of party.slice(1)){
            p.moveset=[new PokemonMove(MoveId.TACKLE,state%4===2?2:0),new PokemonMove(MoveId.SPLASH)];
            if(state%4===1)p.hp=Math.max(1,p.getMaxHp()-3);
            if(state%4===3)p.status=new Status(StatusEffect.TOXIC,2);
        }
        commandMove(holder,state%8>=4?MoveId.LUNAR_DANCE:MoveId.HEALING_WISH);commandMove(other,MoveId.SPLASH);
'''+s[b:]
 a=s.index('            if(state===12||state===23){');b=s.index('            before = dumpState(true);',a);s=s[:a]+s[b:]
 needle='            before = dumpState(true);'
 assert s.count(needle)==1;s=s.replace(needle,'''            if(process.env.PENDING_FOLLOW==="1"){
                await runTurn();battle.turn++;battle.turnCommands={};battle.preTurnCommands={};
                const active=scene.getPlayerField()[0],enemy=scene.getEnemyField()[0];
                commandMove(active,MoveId.TACKLE);commandMove(enemy,MoveId.SPLASH);
                pm.clearAllPhases();pm.phaseQueue.deferredActive=false;pm.dynamicQueueManager.clearLastTurnOrder();
            }
'''+needle)

 needle='        for (const pt of scene.arena.positionalTagManager.tags as any[]) {'
 assert s.count(needle)==1;s=s.replace(needle,'''        for(const at of scene.arena.tags){
            for(const [b,es] of Object.entries(at.pendingHeals??{}) as any){
                for(const h of es)lines.push(`PHEAL target=${b} sid=${h.sourceId} move=${h.moveId} pp=${+h.restorePP} text=${Buffer.from(h.healMessage).toString("hex")}`);
            }
        }
'''+needle)
elif sys.argv[1]=='replay':
 s='static const char *locale_get(void *ctx,const char *file,int fallback,size_t *len);\n'+s
 needle='        } else if (!strncmp(l, "POS ", 4)) {'
 assert s.count(needle)==1;s=s.replace(needle,'''        } else if(!strncmp(l,"PHEAL ",6)){
            pr_arena_tag *t=(pr_arena_tag *)pr_arena_get_tag_on_side(bt,ARENA_TAG_TYPE_PENDING_HEAL,ARENA_TAG_SIDE_BOTH);
            int b,move,pp;long long sid;char hex[4096],extra;
            if(sscanf(l,"PHEAL target=%d sid=%lld move=%d pp=%d text=%4095s %c",&b,&sid,&move,&pp,hex,&extra)!=5||strlen(hex)%2||strspn(hex,"0123456789abcdef")!=strlen(hex)){fprintf(stderr,"bad pending transport\\n");exit(2);}
            if(!t||t->n_pending_heals>=PR_MAX_PENDING_HEALS||b<0||b>3||(move!=MOVE_ID_HEALING_WISH&&move!=MOVE_ID_LUNAR_DANCE)||pp<0||pp>1){fprintf(stderr,"bad pending record\\n");exit(2);}
            for(int k=0;k<t->n_pending_heals;k++)if(t->pending_heals[k].target_index==b&&t->pending_heals[k].move_id==move){fprintf(stderr,"duplicate pending record\\n");exit(2);}
            t->pending_heals[t->n_pending_heals++]=(pr_pending_heal){.source_id=sid,.target_index=b,.move_id=move,.restore_pp=pp};
'''+needle)
 needle='\n/* ------------------------------------------------------------------ printing records */'
 assert s.count(needle)==1
 end=s.index(needle);close=s.rfind('\n}',0,end)
 s=s[:close]+'''
    /* This bounded following-turn fixture leaves the original native source's
     * name unchanged. Actual frozen text remains in the before-state comparison. */
    for(int a=0;a<bt->n_arena_tags;a++)for(int k=0;k<bt->arena_tags[a].n_pending_heals;k++){
        pr_pending_heal *h=&bt->arena_tags[a].pending_heals[k];const pr_pokemon *source=pr_pokemon_by_id(bt,h->source_id);
        if(!source){fprintf(stderr,"unrepresented frozen pending name\\n");exit(2);}h->pokemon_name=pr_arg_pokemon(bt,"",source).v;
    }
'''+s[close:]

 needle='    for (int i = 0; i < bt->n_pos_tags; i++) {\n        const pr_positional_tag *t = &bt->pos_tags[i];'
 assert s.count(needle)==1;s=s.replace(needle,'''    for(int a=0;a<bt->n_arena_tags;a++)for(int b=0;b<4;b++)for(int k=0;k<bt->arena_tags[a].n_pending_heals;k++){
        const pr_pending_heal *h=&bt->arena_tags[a].pending_heals[k];if(h->target_index!=b)continue;
        const char *key=h->move_id==MOVE_ID_LUNAR_DANCE?"moveTriggers:lunarDanceRestore":"moveTriggers:sacrificialFullRestore";
        pr_msg_arg name={"pokemonName",PR_ARG_POKEMON,h->pokemon_name,NULL};pr_msg m=pr_msg1(key,name);pr_text_source src={locale_get,NULL,"en"};char text[1024],hex[2048];size_t n=pr_msg_format(&m,&src,text,sizeof text);if(n>=sizeof text){*incomplete=true;continue;}
        for(size_t j=0;j<n;j++)snprintf(hex+j*2,3,"%02x",(unsigned char)text[j]);hex[n*2]=0;
        add_line(L,"PHEAL target=%d sid=%lld move=%d pp=%d text=%s",b,(long long)h->source_id,h->move_id,h->restore_pp,hex);
    }
'''+needle)
else:raise SystemExit('producer or replay required')
Path(sys.argv[2]).write_text(s)
