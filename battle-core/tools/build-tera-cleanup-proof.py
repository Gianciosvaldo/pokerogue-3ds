#!/usr/bin/env python3
"""Observe the real owning cleanup's Tera counter, including fusion identity."""
from pathlib import Path
import sys
r=Path(__file__).resolve().parents[1];s=(r/'tests/reset-arena.producer.test.ts').read_text()
def put(a,b):
 global s
 assert s.count(a)==1,a[:80]
 s=s.replace(a,b,1)
put('import { audioManager }','import { BattleScene } from "#app/battle-scene";\nimport { audioManager }')
put(' const game=new GameManager',' const native=BattleScene.prototype.randBattleSeedInt;\n const game=new GameManager')
put(' const scene:any=game.scene,',' BattleScene.prototype.randBattleSeedInt=native;\n const counters=[`TERA_CLEANUP 1 ${N}`];\n const scene:any=game.scene,')
put('  timers.length=0;texts=[];','''  scene.arena.playerTerasUsed=[0,3,4294967296,9007199254740991][Math.floor(i/4)%4];
  const countBefore=scene.arena.playerTerasUsed,k=Math.floor(i/16)%4,identity=Math.floor(i/64)%3;
  if(identity===1){slots[k].species=speciesDataRegistry.getSpecies(SpeciesId.TERAPAGOS);slots[k].formIndex=0;slots[k].customPokemonData.ability=AbilityId.BALL_FETCH;slots[k].generateName();}
  if(identity===2){slots[k].fusionSpecies=speciesDataRegistry.getSpecies(SpeciesId.TERAPAGOS);slots[k].fusionFormIndex=0;slots[k].fusionCustomPokemonData={ability:AbilityId.BALL_FETCH};}
  const identityBefore=slots.map(p=>`${p.species.speciesId} ${p.fusionSpecies?.speciesId??0}`).join(" ");
  timers.length=0;texts=[];''')
put('  if(kind!==0&&scene.getPlayerParty()', '  counters.push(`C ${i} ${kind} ${identityBefore} ${countBefore} ${scene.arena.playerTerasUsed}`);\n  if(kind!==0&&scene.getPlayerParty()')
put(' rows.push(`RESETARENA_COMPLETE ${N}`);',' counters.push(`TERA_CLEANUP_COMPLETE ${N}`);fs.writeFileSync(process.env.TERA_CLEANUP_OUT!,counters.join("\\n")+"\\n");\n rows.push(`RESETARENA_COMPLETE ${N}`);')
Path(sys.argv[1]).write_text(s)
