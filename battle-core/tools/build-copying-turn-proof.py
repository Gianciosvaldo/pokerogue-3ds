#!/usr/bin/env python3
"""Add copying-move owning turns to the unchanged Transform turn transport."""
from pathlib import Path
import subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as temp:
 p=Path(temp)/'base'
 subprocess.run([sys.executable,str(root/'tools/build-transform-turn-proof.py'),sys.argv[1],str(p)],check=True)
 s=p.read_text()
def replace(a,b):
 global s
 if s.count(a)!=1:raise SystemExit('Copying proof anchor changed: '+a[:80])
 s=s.replace(a,b,1)
if sys.argv[1]=='producer':
 s=s.replace('TRANSFORMTURN','COPYINGTURN')
 replace('        const slot = rnd(p.moveset.length);\n        p.moveset[slot] = new PokemonMove(move);', '        const slot = p.getMoveset().findIndex((m:any)=>m?.moveId===move);\n        if(slot<0)throw Error("selected move missing from effective moveset");')
 start=s.index('        holder.species=speciesDataRegistry.getSpecies(SpeciesId.DITTO);')
 end=s.index('        let before: string[];',start)
 s=s[:start]+'''        const kind=state%6;
        holder.species=speciesDataRegistry.getSpecies(kind===3||kind===5?SpeciesId.DITTO:SpeciesId.SMEARGLE);
        other.species=speciesDataRegistry.getSpecies(SpeciesId.MAGIKARP);
        for(const p of [holder,other]){p.level=30+rnd(21);p.calculateStats();p.hp=p.getMaxHp();p.generateName();}
        holder.moveset=kind===3||kind===5?[new PokemonMove(MoveId.TRANSFORM)]:[new PokemonMove(kind===1||kind===4?MoveId.MIMIC:MoveId.SKETCH),new PokemonMove(kind===2?MoveId.SPLASH:MoveId.TACKLE)];
        other.moveset=[new PokemonMove(MoveId.SPLASH),new PokemonMove(MoveId.TACKLE),new PokemonMove(kind===5?MoveId.MIMIC:MoveId.SKETCH),new PokemonMove(MoveId.GROWL)];
        holder.setStatStage(Stat.SPD,-6);
        commandMove(holder,kind===3||kind===5?MoveId.TRANSFORM:kind===1||kind===4?MoveId.MIMIC:MoveId.SKETCH);
        commandMove(other,MoveId.SPLASH);
''' +s[end:]
 start=s.index('            if(state===12||state===23){')
 end=s.index('            before = dumpState(true);',start)
 s=s[:start]+'''            if(kind===3||kind===4||kind===5){
                await runTurn();
                battle.turn++;battle.turnCommands={};battle.preTurnCommands={};
                for(const p of [holder,other])p.resetTurnData();
                if(kind===3||kind===5){
                    if(!holder.isTransformed())throw Error("missing real initial Transform");
                    other.setMove(0,MoveId.TAIL_WHIP);
                    holder.setStatStage(Stat.SPD,-6);
                    commandMove(holder,kind===3?MoveId.SKETCH:MoveId.MIMIC);
                    commandMove(other,MoveId.TAIL_WHIP);
                }else{
                    if(!holder.summonData.moveset)throw Error("missing initial Mimic");
                    commandMove(holder,MoveId.SPLASH);commandMove(other,MoveId.SPLASH);
                }
                pm.clearAllPhases();pm.phaseQueue.deferredActive=false;pm.dynamicQueueManager.clearLastTurnOrder();
            }
            if((state>=6&&state<12||state>=18)&&kind!==2){
                await runTurn();battle.turn++;battle.turnCommands={};battle.preTurnCommands={};
                for(const p of [holder,other])p.resetTurnData();
                commandMove(holder,kind===3||kind===5?MoveId.TAIL_WHIP:MoveId.SPLASH);
                commandMove(other,kind===3||kind===5?MoveId.TAIL_WHIP:MoveId.SPLASH);
                pm.clearAllPhases();pm.phaseQueue.deferredActive=false;pm.dynamicQueueManager.clearLastTurnOrder();
            }
''' +s[end:]
 replace('            `MOVES b=${b} list=${L(p.moveset.map((m: any) => `${m.moveId}:${m.ppUsed}:${m.ppUp}:${num(m.maxPpOverride)}`))}`,','''            `MOVES b=${b} list=${L(Array.from({length:p.moveset.length},(_,i)=>p.moveset[i]?`${p.moveset[i].moveId}:${p.moveset[i].ppUsed}:${p.moveset[i].ppUp}:${num(p.moveset[i].maxPpOverride)}`:"0:0:0:u"))}`,''')
 needle=''' : ""),
            ].join(" "),'''
 idx=s.index('                    + (p.isTransformed() ?')
 end=s.index(needle,idx)
 s=s[:end]+s[end:].replace(needle,''' : "")
                    + (sd.moveset ? ` sm=1 malias=${sd.moveset.reduce((mask:number,m:any,i:number)=>mask|(m===p.moveset[i]?1<<i:0),0)} smmoves=${L(sd.moveset.map((m:any)=>`${m.moveId}:${m.ppUsed}:${m.ppUp}:${num(m.maxPpOverride)}`))}` : ""),
            ].join(" "),''',1)
elif sys.argv[1]=='replay':
 s='#include <errno.h>\n#include <limits.h>\n'+s
 marker='static long copying_number(const char *s,long min,long max)\n{\n    char *end;errno=0;long n=strtol(s,&end,10);\n    if(errno||end==s||*end||n<min||n>max){fprintf(stderr,"bad copying move number\\n");exit(2);}\n    return n;\n}\n'
 s=s.replace('#include <stdarg.h>', '#include <stdarg.h>\n'+marker,1)
 replace('            p->turn_move_effectiveness = -1;','''            if(field(l,"sm")){
                p->summon_moves_active=true;
                if(copying_number(need(l,"sm"),1,1)!=1)exit(2);
                p->summon_moves_alias_mask=(uint8_t)copying_number(need(l,"malias"),0,15);
                char moves[4][64];p->transform_n_moves=(uint8_t)split(need(l,"smmoves"),',',moves,4);
                for(int j=0;j<p->transform_n_moves;j++){
                    char parts[4][64];if(split(moves[j],':',parts,4)!=4){fprintf(stderr,"bad summon moves\\n");exit(2);}
                    p->transform_moves[j]=(uint16_t)copying_number(parts[0],0,PR_MOVE_COUNT-1);p->transform_pp_used[j]=(uint8_t)copying_number(parts[1],0,255);
                    p->transform_pp_up[j]=(uint8_t)copying_number(parts[2],0,3);p->transform_max_pp_override[j]=!strcmp(parts[3],"u")?-1:(int16_t)copying_number(parts[3],-1,32767);
                }
            }
            p->turn_move_effectiveness = -1;''')
 needle='''    add_line(L, "%s", s.s);

    s.n = 0;
    sb(&s, "TURN b=%s acted=%d order=", b, p->acted);'''
 replace(needle,'''    if(PR_HAS_SUMMON_MOVES(p)){
        sb(&s," sm=1 malias=%d smmoves=",p->summon_moves_alias_mask);
        for(int i=0;i<PR_N_MOVES(p);i++){
            if(i)sb(&s,",");
            sb(&s,"%d:%d:%d:",PR_MOVES(p)[i],PR_PP_USED(p)[i],PR_PP_UP(p)[i]);num_or_u(&s,PR_MAX_PP(p)[i],-1);
        }
        if(!PR_N_MOVES(p))sb(&s,"-");
    }
    add_line(L, "%s", s.s);

    s.n = 0;
    sb(&s, "TURN b=%s acted=%d order=", b, p->acted);''')
else:raise SystemExit('producer or replay required')
if sys.argv[1]=='producer' and len(sys.argv)>3 and sys.argv[3]=='transition':
 s=s.replace('const kind=state%6;','const kind=6;')
 s=s.replace('kind===1||kind===4?MoveId.MIMIC:MoveId.SKETCH','kind===1||kind===4||kind===6?MoveId.MIMIC:MoveId.SKETCH')
 s=s.replace('new PokemonMove(kind===2?MoveId.SPLASH:MoveId.TACKLE)','new PokemonMove(kind===6?MoveId.TRANSFORM:kind===2?MoveId.SPLASH:MoveId.TACKLE)')
 s=s.replace('if(kind===3||kind===4||kind===5){','if(kind===3||kind===4||kind===5||kind===6){')
 s=s.replace('if(kind===3||kind===5){\n                    if(!holder.isTransformed())', 'if(kind===6){\n                    if(!holder.summonData.moveset)throw Error("missing initial Mimic");\n                    commandMove(holder,MoveId.TRANSFORM);commandMove(other,MoveId.SPLASH);\n                }else if(kind===3||kind===5){\n                    if(!holder.isTransformed())')
Path(sys.argv[2]).write_text(s)
