#!/usr/bin/env python3
"""Actual prior-turn Dive and defined Magic Guard bench retaliation, plus wide controls."""
from pathlib import Path
import subprocess,sys
r=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]);subprocess.run([sys.executable,r/'tools/build-gulp-missile-turn-proof.py',out],check=True);s=out.read_text()
def put(a,b):
 global s
 assert s.count(a)==1,(a[:90],s.count(a))
 s=s.replace(a,b,1)
put('import { BattleScene }','import { PositionalTagType } from "#enums/positional-tag-type";\nimport { BattleScene }')
put('allMoves[MoveId.SURF].getAttrs("GulpMissileTagAttr")','(allMoves[MoveId.DIVE] as any).getChargeAttrs("GulpMissileTagAttr")')
put('const holder=Math.floor(i/8)%4,opponent=holder^2,loaded=i>=2048;','const holder=i>=2048?2+i%2:Math.floor(i/8)%4,opponent=holder^2,loaded=i>=2048;')
put('p.formIndex=k===holder&&loaded?1+(i%2):0','p.formIndex=k===holder&&loaded?1:0')
put('k===holder?(loaded?MoveId.SPLASH:i%4<2?MoveId.SURF:MoveId.DIVE):k===opponent?MoveId.TACKLE:MoveId.SPLASH','k===holder&&!loaded?MoveId.DIVE:MoveId.SPLASH')
put('getBattlerTag(i%2?BattlerTagType.GULP_MISSILE_PIKACHU:BattlerTagType.GULP_MISSILE_ARROKUDA,0,MoveId.SURF)','getBattlerTag(BattlerTagType.GULP_MISSILE_ARROKUDA,0,MoveId.SURF)')
put('    let before: string[];','''    if(i>=2048&&i<4096){
      const caster=playerBench[0],target=2+i%2;
      if(!party.includes(caster))party.push(caster);
      caster.resetSummonData();caster.resetTurnData();caster.resetBattleAndWaveData();caster.species=speciesDataRegistry.getSpecies(SpeciesId.BULBASAUR);caster.formIndex=0;caster.fusionSpecies=null;caster.level=50;caster.nature=0;caster.ivs=Array(6).fill(15);caster.customPokemonData.ability=process.env.GULP_UNPROTECTED_BENCH==="1"?AbilityId.NONE:AbilityId.MAGIC_GUARD;caster.summonData.ability=caster.customPokemonData.ability;caster.customPokemonData.passive=AbilityId.NONE;caster.passive=false;caster.calculateStats();caster.generateName();caster.hp=caster.getMaxHp();caster.status=null;caster.moveset=[new PokemonMove(MoveId.FUTURE_SIGHT)];
      scene.arena.positionalTagManager.addTag({tagType:PositionalTagType.DELAYED_ATTACK,sourceId:caster.id,targetIndex:target,sourceMove:MoveId.FUTURE_SIGHT,turnCount:1});
    }
    if(i<2048){
      const prior=await runTurn();if(prior.stop!=="TurnInitPhase")throw Error(`Dive preparation stopped at ${prior.stop}`);
      const holder=actors[Math.floor(i/8)%4];if(!holder.getTag(BattlerTagType.UNDERWATER)||holder.summonData.moveQueue.length!==1)throw Error(`native Dive preparation missing ${i}`);
      pm.clearAllPhases();pm.phaseQueue.deferredActive=false;pm.dynamicQueueManager.clearLastTurnOrder();battle.turnCommands={};battle.preTurnCommands={};
      for(const p of actors){p.resetTurnData();command(p);}
    }
    let before: string[];''')
put('native Gulp Missile owning doubles turns','native Gulp Missile Dive continuation and Magic Guard bench delayed turns')
out.write_text(s)
