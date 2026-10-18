#!/usr/bin/env python3
"""Natural trainer Revival Blessing turns plus retained wide/ally/spread/faint controls."""
from pathlib import Path
import subprocess,sys
r=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]);subprocess.run([sys.executable,r/'tools/build-wide-combat-proof.py',out],check=True);s=out.read_text()
def put(a,b):
 global s
 assert s.count(a)==1,a[:100]
 s=s.replace(a,b,1)
put('import { it,vi } from "vitest";','import { it,vi } from "vitest";\nimport { TrainerSlot } from "#enums/trainer-slot";\nimport { PartyUiMode } from "#enums/party-ui-mode";')
put('.battleType(BattleType.WILD)','.battleType(BattleType.TRAINER)')
put('  const rawDamage=vi.spyOn(Pokemon.prototype,"getAttackDamage");','''  const rawDamage=vi.spyOn(Pokemon.prototype,"getAttackDamage");
  const trainer=battle.trainer;if(!trainer)throw Error("actual trainer required");
  const epicks=vi.spyOn(trainer,"getNextSummonIndex");
  const revival=vi.spyOn(allMoves[MoveId.REVIVAL_BLESSING].getAttrs("RevivalBlessingAttr")[0],"apply");
  const enemyBench=Array.from({length:2},()=>scene.addEnemyPokemon(speciesDataRegistry.getSpecies(SpeciesId.PIDGEY),50,TrainerSlot.TRAINER));
  let trainerCases=0;''')
put('''    const eligible = party.map((_: any, k: number) => k).filter((k: number) => k >= first && !party[k].isFainted());''','''    const revive=args[0]===PartyUiMode.REVIVAL_BLESSING;
    const eligible = party.map((_: any, k: number) => k).filter((k: number) => revive?party[k].isFainted():k >= first && !party[k].isFainted());''')
put('    args[2](eligible.length ? eligible[r % eligible.length] : -1, PartyOption.SEND_OUT);','    args[2](eligible.length ? eligible[r % eligible.length] : -1, revive?PartyOption.REVIVE:PartyOption.SEND_OUT);')
put('''    battle.playerParticipantIds.add(A0.id);''','''    if(i<4096){
      trainerCases++;battle.battleType=BattleType.TRAINER;battle.trainer=trainer;
      party.splice(2,99,...playerBench.slice(0,2));eparty.splice(2,99,...enemyBench);
      for(const b of enemyBench){if(b.isOnField())scene.field.remove(b);b.resetSummonData();b.resetTurnData();b.resetBattleAndWaveData();}
      for(const side of [party,eparty])for(let k=0;k<side.length;k++){
        const p=side[k];p.species=speciesDataRegistry.getSpecies(SpeciesId.BULBASAUR);p.formIndex=0;p.fusionSpecies=null;p.customPokemonData.ability=AbilityId.BALL_FETCH;p.summonData.ability=AbilityId.BALL_FETCH;p.passive=false;p.customPokemonData.passive=AbilityId.NONE;p.level=50;p.calculateStats();p.generateName();p.hp=(k===1||k===2)?0:p.getMaxHp();p.status=p.hp?null:new Status(StatusEffect.FAINT);p.switchOutStatus=false;p.summonData.tags=[];p.moveset=[new PokemonMove(k===0?MoveId.REVIVAL_BLESSING:MoveId.SPLASH)];
        if(!p.isPlayer()){p.trainerSlot=TrainerSlot.TRAINER;p.bossSegments=0;p.bossSegmentIndex=0;}
      }
    }else{
      battle.battleType=BattleType.WILD;battle.trainer=null;for(const p of [D0,D1])p.trainerSlot=TrainerSlot.NONE;
    }
    battle.playerParticipantIds.add(A0.id);''')
put('    lines.push(`PICK list=${L(picks)}`);','''    lines.push(`PICK list=${L(picks)}`);
    if(battle.trainer){const tr=battle.trainer;lines.push(`TRAINER partner=${+tr.isPartner()} names=${JSON.stringify([tr.getName(TrainerSlot.TRAINER),tr.getName(TrainerSlot.TRAINER_PARTNER)])}`);}''')
put('  const out: string[] = [`# DOUBLES_CONTRACT 1 ${N}`];','''  const fd=fs.openSync(OUT,"w");
  const out={push:(...lines:string[])=>fs.writeSync(fd,lines.join("\\n")+"\\n")};
  out.push(`# DOUBLES_CONTRACT 1 ${N}`);''')
put('  fs.writeFileSync(OUT, out.join("\\n") + "\\n");','  fs.closeSync(fd);')
put('  out.push(`# DOUBLES_COMPLETE ${written} ${threwCount}`);','''  const successes=revival.mock.results.filter(x=>x.type==="return"&&x.value===true).length;
  if(successes<100||trainerCases<4096||epicks.mock.calls.length)throw Error(`Revival owning coverage ${successes}/${trainerCases}/unexpected picks ${epicks.mock.calls.length}`);
  console.log(`REVIVAL_TURNS successful=${successes} trainerCases=${trainerCases} calls=${revival.mock.calls.length}`);
  out.push(`# DOUBLES_COMPLETE ${written} ${threwCount}`);''')
put('it("wide native combat doubles turns"','it("native Revival Blessing trainer doubles turns"')
out.write_text(s)
