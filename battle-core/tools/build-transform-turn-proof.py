#!/usr/bin/env python3
"""Add a Transform owning projection to unchanged Speed Boost scaffolding.
The source battle/turn methods and established comparator are retained. Every
overlay has one exact anchor; changes in that scaffolding require review.
"""
from pathlib import Path
import sys

def overlay(text, old, new):
    if text.count(old) != 1:
        raise SystemExit("Transform proof anchor changed; review required: " + old[:90])
    return text.replace(old, new, 1)

root = Path(__file__).resolve().parents[1]
if sys.argv[1] == "producer":
    s = (root / "tests/speed-boost.producer.test.ts").read_text()
    s = s.replace("SPEEDBOOST", "TRANSFORMTURN").replace("bounded Speed Boost move turn producer", "actual Transform full move turns")
    s = overlay(s, '    await game.classicMode.startBattle(SpeciesId.BULBASAUR);', '''    await game.classicMode.startBattle(SpeciesId.BULBASAUR);
    game.override.moveset([]).enemyMoveset([]);
    (Pokemon.prototype as any).loadAssets = async function () {};
    (Pokemon.prototype as any).playAnim = function () {};''')
    start = s.index('    const originalCan=SpeedBoostAbAttr.prototype.canApply;')
    end = s.index('    const out:string[]=[];', start)
    s = s[:start] + s[end:]
    start = s.index('        holder.summonData.ability=AbilityId.SPEED_BOOST;')
    end = s.index('        let before: string[];', start)
    s = s[:start] + '''        holder.species=speciesDataRegistry.getSpecies(SpeciesId.DITTO);
        holder.gender=-1; holder.level=30+rnd(21); holder.calculateStats(); holder.hp=holder.getMaxHp(); holder.generateName();
        other.species=speciesDataRegistry.getSpecies(state%2?SpeciesId.CHARIZARD:SpeciesId.PIKACHU);
        other.formIndex=state===18?1:state===19?2:0;other.calculateStats();other.hp=other.getMaxHp();other.generateName();
        for(const p of [holder,other]){
            p.moveset=[new PokemonMove(MoveId.TRANSFORM),new PokemonMove(MoveId.TACKLE,2,2),new PokemonMove(MoveId.GROWL),new PokemonMove(MoveId.SKETCH)];
            for(const stat of [Stat.ATK,Stat.DEF,Stat.SPATK,Stat.SPDEF,Stat.SPD,Stat.ACC,Stat.EVA])p.setStatStage(stat,rnd(13)-6);
        }
        if(state===1)other.moveset[1]=new PokemonMove(MoveId.TACKLE,prPp=allMoves[MoveId.TACKLE].pp,3,7);
        if(state===2)holder.addTag(BattlerTagType.POWER_TRICK,0,MoveId.POWER_TRICK,holder.id);
        if(state===3)for(let stat=1;stat<6;stat++)other.setStat(stat,other.stats[stat]*1.5,false);
        if(state===4)holder.summonData.addedType=7;
        if(state===5){other.summonData.types=[3,9];other.summonData.addedType=7;}
        if(state===6)other.addTag(BattlerTagType.SUBSTITUTE,0,MoveId.SUBSTITUTE,other.id);
        if(state===7)holder.summonData.ability=AbilityId.IMPOSTER;
        if(state===8)other.summonData.ability=AbilityId.IMPOSTER;
        if(state===9)other.summonData.ability=AbilityId.INTREPID_SWORD;
        if(state===10)other.summonData.ability=AbilityId.SPEED_BOOST;
        if(state===11){holder.passive=true;holder.customPokemonData.passive=AbilityId.SIMPLE;}
        if(state===14){holder.hp=1;holder.setStat(Stat.SPD,1,false);other.setStat(Stat.SPD,1000,false);}
        if(state===16){holder.isTerastallized=true;holder.teraType=4;}
        if(state===17){other.isTerastallized=true;other.teraType=4;}
        if(state===20)holder.moveset=[new PokemonMove(MoveId.TRANSFORM)];
        if(state===21)other.moveset=[new PokemonMove(MoveId.SPLASH)];
        if(state===22)other.moveset=[new PokemonMove(MoveId.SKETCH)];
        commandMove(holder,MoveId.TRANSFORM);
        commandMove(other,state===13?MoveId.TRANSFORM:state===14?MoveId.TACKLE:state===15?MoveId.PROTECT:MoveId.SPLASH);
''' + s[end:]
    s = s.replace('prPp=allMoves[MoveId.TACKLE].pp', 'allMoves[MoveId.TACKLE].pp')
    s = overlay(s, 'state=Math.floor(i/2)%30;', 'state=Math.floor(i/2)%24;')
    s = overlay(s, '            before = dumpState(true);', '''            // Fixture setup may queue tag presentation; the proof starts at TurnStart.
            pm.clearAllPhases();pm.phaseQueue.deferredActive=false;pm.dynamicQueueManager.clearLastTurnOrder();
            if(state===12||state===23){
                // Run the actual first turn, then prove use of a copied PokemonMove
                // on the next complete turn, including its independent PP counter.
                await runTurn();
                if(!holder.isTransformed())throw new Error("missing initial Transform");
                battle.turn++;battle.turnCommands={};battle.preTurnCommands={};
                for(const p of [holder,other]){
                    const moves=p.getMoveset(),slot=moves.findIndex((m:any)=>m.moveId===MoveId.SPLASH);
                    if(slot<0)throw new Error("missing copied Splash");
                    const move={move:MoveId.SPLASH,targets:getMoveTargets(p,MoveId.SPLASH).targets,useMode:MoveUseMode.NORMAL};
                    battle.turnCommands[p.getBattlerIndex()]=p.isPlayer()?{command:Command.FIGHT,cursor:slot,move,args:[MoveUseMode.NORMAL,undefined]}:{command:Command.FIGHT,move,skip:false};
                }
                pm.clearAllPhases();pm.phaseQueue.deferredActive=false;pm.dynamicQueueManager.clearLastTurnOrder();
            }
            before = dumpState(true);''')
    s = overlay(s, '    const stateCounts=Array(30).fill(0), sideCounts=[0,0], applies=Array(30).fill(0),refusals=Array(30).fill(0);',
        '    const stateCounts=Array(24).fill(0), sideCounts=[0,0];')
    s = overlay(s, '                    + (sd.stats.some((v: number) => v) ? ` ss=${L(sd.stats)}` : ""),', '''                    + (sd.stats.some((v: number) => v) ? ` ss=${L(sd.stats)}` : "")
                    + (p.isTransformed() ? ` tfsp=${p.getSpeciesForm().speciesId} tfform=${Math.max(0,speciesDataRegistry.getSpecies(p.getSpeciesForm().speciesId).forms.indexOf(p.getSpeciesForm()))} tfgender=${p.getGender()} tfmoves=${L(p.getMoveset().map((m:any)=>`${m.moveId}:${m.ppUsed}:${m.ppUp}:${num(m.maxPpOverride)}`))}` : ""),''')
    start = s.index('    console.log(`TRANSFORMTURN cases=')
    end = s.index('    fs.writeFileSync(OUT', start)
    s = s[:start] + '    console.log(`TRANSFORMTURN cases=${N} states=${stateCounts.join("/")} sides=${sideCounts.join("/")}`);\n' + s[end:]
    s = '/* Generated additive Transform source scaffold; see build-transform-turn-proof.py. */\n' + s
elif sys.argv[1] == "replay":
    s = (root / "tests/speed-boost-replay.inc").read_text()
    s = overlay(s, '            p->turn_move_effectiveness = -1;', '''            if(field(l,"tfsp")){
                p->transformed=true;p->transform_species=(uint16_t)inum(l,"tfsp");
                p->transform_form=(uint8_t)inum(l,"tfform");p->transform_gender=(int8_t)inum(l,"tfgender");
                char moves[4][64];p->transform_n_moves=(uint8_t)split(need(l,"tfmoves"),',',moves,4);
                for(int j=0;j<p->transform_n_moves;j++){
                    char parts[4][64];if(split(moves[j],':',parts,4)!=4){fprintf(stderr,"bad temporary move\\n");exit(2);}
                    p->transform_moves[j]=(uint16_t)atoi(parts[0]);p->transform_pp_used[j]=(uint8_t)atoi(parts[1]);
                    p->transform_pp_up[j]=(uint8_t)atoi(parts[2]);p->transform_max_pp_override[j]=!strcmp(parts[3],"u")?-1:(int16_t)atoi(parts[3]);
                }
            }
            p->turn_move_effectiveness = -1;''')
    s = overlay(s, '    add_line(L, "%s", s.s);\n\n    s.n = 0;\n    sb(&s, "TURN b=%s acted=%d order=", b, p->acted);', '''    if (p->transformed) {
        sb(&s," tfsp=%d tfform=%d tfgender=%d tfmoves=",p->transform_species,p->transform_form,pr_get_gender(p));
        for(int i=0;i<PR_N_MOVES(p);i++){
            if(i)sb(&s,",");
            sb(&s,"%d:%d:%d:",PR_MOVES(p)[i],PR_PP_USED(p)[i],PR_PP_UP(p)[i]);num_or_u(&s,PR_MAX_PP(p)[i],-1);
        }
        if(!PR_N_MOVES(p))sb(&s,"-");
    }
    add_line(L, "%s", s.s);

    s.n = 0;
    sb(&s, "TURN b=%s acted=%d order=", b, p->acted);''')
else:
    raise SystemExit("producer or replay required")
Path(sys.argv[2]).write_text(s)
