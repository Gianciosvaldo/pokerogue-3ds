#!/usr/bin/env python3
"""Native Toxic turns own the status -> Poison Puppeteer callback; retain wide controls."""
from pathlib import Path
import subprocess,sys
r=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]);subprocess.run([sys.executable,r/'tools/build-wide-combat-proof.py',out],check=True);s=out.read_text()
def put(a,b):
 global s
 assert s.count(a)==1,a[:100]
 s=s.replace(a,b,1)
put('import { it,vi } from "vitest";','import { it,vi } from "vitest";\nimport { Terrain } from "#data/terrain";\nimport { TerrainType } from "#enums/terrain-type";\nimport { PokemonType } from "#enums/pokemon-type";')
put('  const rawDamage=vi.spyOn(Pokemon.prototype,"getAttackDamage");','''  const rawDamage=vi.spyOn(Pokemon.prototype,"getAttackDamage");
  const puppetCounts=[0,0];
  const puppet=allAbilities[AbilityId.POISON_PUPPETEER].getAttrs("ConfusionOnStatusEffectAbAttr")[0],nativeApply=puppet.apply;
  vi.spyOn(puppet,"apply").mockImplementation(function(this:any,params:any){puppetCounts[+!!params.passive]++;return nativeApply.call(this,params);});''')
put('    picks = [0, 1, 2].map(', '    game.override.ability(i<4096?AbilityId.NONE:AbilityId.BALL_FETCH).enemyAbility(i<4096?AbilityId.NONE:AbilityId.BALL_FETCH);\n    picks = [0, 1, 2].map(')
put('    battle.playerParticipantIds.add(A0.id);','''    if(i<4096){
      const source=i%4,target=(source+1+Math.floor(i/4)%3)%4,passive=!!(Math.floor(i/16)%2),shape=Math.floor(i/32)%8;
      battle.waveIndex=201;
      scene.arena.terrain=shape>=4?new Terrain(TerrainType.MISTY):null;
      for(let k=0;k<4;k++){
        const p=actors[k];p.species=speciesDataRegistry.getSpecies(k===source?SpeciesId.PECHARUNT:SpeciesId.CHARMANDER);p.formIndex=0;p.fusionSpecies=null;p.level=[50,256,1000000][Math.floor(i/256)%3];p.summonData.tags=[];p.status=null;p.switchOutStatus=false;p.summonData.abilitySuppressed=false;p.customPokemonData.ability=k===source&&!passive?AbilityId.POISON_PUPPETEER:AbilityId.BALL_FETCH;p.summonData.ability=p.customPokemonData.ability;p.passive=k===source&&passive;p.customPokemonData.passive=p.passive?AbilityId.POISON_PUPPETEER:AbilityId.NONE;p.isTerastallized=k===target&&shape>=6;p.teraType=shape===6?PokemonType.FLYING:PokemonType.GROUND;
        if(k===target&&shape===1)p.customPokemonData.ability=p.summonData.ability=AbilityId.OWN_TEMPO;
        if(k===target&&(shape===2||shape===5))p.customPokemonData.ability=p.summonData.ability=AbilityId.LEVITATE;
        if(k===target&&shape===3){p.passive=true;p.customPokemonData.passive=AbilityId.OWN_TEMPO;}
        p.summonData.statStages.fill(0);p.summonData.statStages[Stat.SPD-1]=k===source?6:-6;p.moveset=[new PokemonMove(k===source?MoveId.TOXIC:MoveId.SPLASH)];p.calculateStats();p.generateName();p.hp=p.getMaxHp();
      }
      const user=actors[source];if((!passive&&user.getAbility().id!==AbilityId.POISON_PUPPETEER)||(passive&&(!user.hasPassive()||user.getPassiveAbility().id!==AbilityId.POISON_PUPPETEER)))throw Error("puppeteer fixture override");
      battle.puppetSource=source;battle.puppetTarget=target;
    }
    battle.playerParticipantIds.add(A0.id);''')
put('    let before: string[];','''    if(i<4096){const source=battle.puppetSource,target=battle.puppetTarget;battle.turnCommands[source].move.targets=[target];}
    let before: string[];''')
put('  const out: string[] = [`# DOUBLES_CONTRACT 1 ${N}`];','''  const fd=fs.openSync(OUT,"w");
  const out={push:(...rows:string[])=>fs.writeSync(fd,rows.join("\\n")+"\\n")};
  out.push(`# DOUBLES_CONTRACT 1 ${N}`);''')
put('  fs.writeFileSync(OUT, out.join("\\n") + "\\n");','  fs.closeSync(fd);')
put('  out.push(`# DOUBLES_COMPLETE ${written} ${threwCount}`);','''  if(puppetCounts.some(n=>n<700))throw Error(`Puppeteer ownership coverage ${puppetCounts}`);
  console.log(`PUPPETEER_TURNS primaryPassive=${puppetCounts}`);
  out.push(`# DOUBLES_COMPLETE ${written} ${threwCount}`);''')
put('it("wide native combat doubles turns"','it("native Toxic owns Poison Puppeteer confusion"')
out.write_text(s)
