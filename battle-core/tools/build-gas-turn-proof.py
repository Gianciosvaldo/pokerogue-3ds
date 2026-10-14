#!/usr/bin/env python3
"""Actual singles Gas turns, including a carrier KO; unchanged turn comparator."""
from pathlib import Path
import subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
    base=Path(tmp)/'base'
    subprocess.run([sys.executable,str(root/'tools/build-transform-turn-proof.py'),sys.argv[1],str(base)],check=True)
    s=base.read_text()
if sys.argv[1]=='producer':
    s=s.replace('TRANSFORMTURN','GASTURN')
    s='import { SuppressAbilitiesTag } from "#data/arena-tag";\n'+s
    s=s.replace('    game.override.moveset([]).enemyMoveset([]);','    game.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);')
    a=s.index('        holder.species=speciesDataRegistry.getSpecies(SpeciesId.DITTO);')
    b=s.index('        let before: string[];',a)
    s=s[:a]+'''        holder.species=speciesDataRegistry.getSpecies(SpeciesId.RATTATA);holder.formIndex=0;
        other.species=speciesDataRegistry.getSpecies(SpeciesId.MAGIKARP);other.formIndex=0;
        for(const p of [holder,other]){p.summonData.ability=undefined;p.customPokemonData.ability=AbilityId.BALL_FETCH;p.level=30+rnd(21);p.moveset=[new PokemonMove(MoveId.SPLASH),new PokemonMove(MoveId.TACKLE)];p.calculateStats();p.hp=p.getMaxHp();p.generateName();}
        holder.customPokemonData.ability=AbilityId.NEUTRALIZING_GAS;
        if(state%4===1){holder.customPokemonData.ability=AbilityId.BALL_FETCH;holder.passive=true;holder.customPokemonData.passive=AbilityId.NEUTRALIZING_GAS;}
        other.customPokemonData.ability=state%3===0?AbilityId.INTREPID_SWORD:AbilityId.SPEED_BOOST;
        if(state%4===2){other.passive=true;other.customPokemonData.passive=AbilityId.INTIMIDATE;}
        const gas:any=new SuppressAbilitiesTag(holder.id);scene.arena.tags=[gas];
        if(state>=12){holder.hp=1;holder.setStat(Stat.SPD,1,false);other.setStat(Stat.SPD,1000,false);}
        commandMove(holder,MoveId.SPLASH);commandMove(other,state>=12?MoveId.TACKLE:MoveId.SPLASH);
'''+s[b:]
    a=s.index('            if(state===12||state===23){')
    b=s.index('            before = dumpState(true);',a)
    s=s[:a]+s[b:]
    s=s.replace('+ (at instanceof EntryHazardTag ? ` layers=${at.layers}` : ""));','+ (at instanceof EntryHazardTag ? ` layers=${at.layers}` : "")\n                + (at instanceof SuppressAbilitiesTag ? ` gascount=${at.sourceCount} removing=${+at.beingRemoved}` : ""));')
    s=s.replace('actual Transform full move turns','actual Gas full move turns')
elif sys.argv[1]=='replay':
    s=s.replace('t->layers = field(l, "layers") ? (int8_t)inum(l, "layers") : 1;', 't->source_count=field(l,"gascount")?(int8_t)inum(l,"gascount"):0;t->being_removed=field(l,"removing")?inum(l,"removing"):false;\n            t->layers = field(l, "layers") ? (int8_t)inum(l, "layers") : 1;')
    old='t->turn_count, t->max_duration, t->source_move, (long long)t->source_id, layers);'
    assert s.count(old)==1
    s=s.replace(old,old+'\n        if(t->type==ARENA_TAG_TYPE_NEUTRALIZING_GAS){char *last=L->s[L->n-1];size_t len=strlen(last);snprintf(last+len,LINE_LEN-len," gascount=%d removing=%d",t->source_count,t->being_removed);}')
else:raise SystemExit('producer or replay required')
Path(sys.argv[2]).write_text(s)
