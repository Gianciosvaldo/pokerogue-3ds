#!/usr/bin/env python3
"""Pinned actual DoubleBattleChance dispatcher and scene getter; complete frames/queues/RNG."""
from pathlib import Path
import subprocess,sys,tempfile
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'base.ts';subprocess.run([sys.executable,r/'tools/build-biome-callback-proof.py',p],check=True);s=p.read_text()
s=s.replace('import { Weather } from "#data/weather";','import { NumberHolder } from "#utils/common";\nimport { modifierTypes } from "#data/data-lists";\nimport { generateModifierType } from "#mystery-encounters/encounter-phase-utils";\nimport { Weather } from "#data/weather";')
s=s.replace('const N=26880,seed=Number(process.env.BIOME_CALLBACK_SEED??777)','const N=16384,seed=Number(process.env.DOUBLE_CHANCE_SEED??777)')
s=s.replace('process.env.BIOME_CALLBACK_OUT??"/tmp/biome-callback.txt"','process.env.DOUBLE_CHANCE_OUT??"/tmp/double-chance.txt"').replace('BIOME_CALLBACK','DOUBLE_CHANCE')
a=s.index(' const abilities=');b=s.index(' const queues=',a)
s=s[:a]+''' const abilities=[AbilityId.ILLUMINATE,AbilityId.ARENA_TRAP,AbilityId.NO_GUARD,AbilityId.COMMANDER];
 const calls=abilities.map(id=>vi.spyOn(allAbilities[id].getAttrs("DoubleBattleChanceAbAttr")[0],"apply"));
'''+s[b:]
a=s.index(' write([`DOUBLE_CHANCE');b=s.index('  pm.clearAllPhases();',a)
s=s[:a]+''' write([`DOUBLE_CHANCE 1 ${N} ${seed}`]);let changed=0,offFieldApplications=0;
 for(let i=0;i<N;i++){
  const owner=i>=8192,j=i%8192,u=j%8,kind=Math.floor(j/8)%4,simulated=!owner&&!!(Math.floor(j/32)%2),shape=Math.floor(j/(owner?32:64))%8,single=!!(Math.floor(j/(owner?256:512))%2),nLures=owner?Math.floor(j/512)%4:0,wave=owner?[1,10,256,1000000][Math.floor(j/2048)]:1,input=owner?0:[0,.5,1,2,8,32,256,1e12][Math.floor(j/1024)];
  battle.double=!single;scene.gameMode=getGameMode(GameModes.CLASSIC);scene.arena.weather=null;scene.arena.terrain=null;scene.arena.ignoreAbilities=false;scene.arena.ignoringEffectSource=null;
  scene.modifiers.length=0;
  for(let k=0;k<nLures;k++){const m=(generateModifierType((modifierTypes as any)[["LURE","SUPER_LURE","MAX_LURE"][k]]) as any).newModifier();m.stackCount=1+(i%2);scene.modifiers.push(m);}
  for(let k=0;k<8;k++){
   const p=actors[k];if(k<4&&scene.field.getIndex(p)<0)scene.field.add(p);
   p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies([SpeciesId.BULBASAUR,SpeciesId.PIDGEY,SpeciesId.DRATINI,SpeciesId.CHARMANDER][k%4]);p.formIndex=0;p.fusionSpecies=null;p.customPokemonData.ability=k===u&&shape!==1?abilities[kind]:AbilityId.BALL_FETCH;p.passive=k===u&&[1,2,5].includes(shape);p.customPokemonData.passive=p.passive?abilities[(kind+(shape===5?1:0))%4]:AbilityId.NONE;p.abilityIndex=0;p.level=[1,50,256,1000000][k%4];p.gender=-1;p.isTerastallized=false;p.calculateStats();p.hp=k===u&&shape===4?0:p.getMaxHp();p.status=null;p.moveset=[new PokemonMove(MoveId.SPLASH)];p.visible=false;p.switchOutStatus=k===u&&shape===6;p.summonData.abilitySuppressed=k===u&&shape===3;p.summonData.addedType=null;p.generateName();
  }
  if(shape===7)scene.field.remove(actors[u]);
'''+s[b:]
s=s.replace('`biome-callback-${seed}-${i}`','`double-chance-${seed}-${i}`')
a=s.index('  const before=env();');b=s.index('\n }\n if(',a)
s=s[:a]+'''  const chance=new NumberHolder(input),beforeCalls=calls.reduce((n,c)=>n+c.mock.calls.length,0);
  write([`C ${i} ${u} ${+simulated} ${+owner} ${+single} ${wave} ${nLures} ${input}`,`W ${scene.waveSeed}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(battle.battleSeedState)}`,...actors.map((p,k)=>`I ${k} ${snapshot(p).join(" ")}`)]);
  if(owner)chance.value=scene.getDoubleBattleChance(wave);else applyAbAttrs("DoubleBattleChanceAbAttr",{pokemon:actors[u],chance,simulated});
  changed+=+(chance.value!==input);offFieldApplications+=+(shape===7&&calls.reduce((n,c)=>n+c.mock.calls.length,0)>beforeCalls);
  write([`V ${chance.value}`,...actors.map((p,k)=>`O ${k} ${snapshot(p).join(" ")}`),...queues(),`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(battle.battleSeedState)}`,"E"]);
'''+s[b:]
a=s.index(' if(calls.some');b=s.index('\n},600000);',a)
s=s[:a]+''' if(calls.some(c=>c.mock.calls.length<100)||changed<1000||offFieldApplications<100)throw Error(`coverage ${calls.map(c=>c.mock.calls.length)}/${changed}/${offFieldApplications}`);
 write([`DOUBLE_CHANCE_COMPLETE ${N}`]);fs.closeSync(fd);console.log(`DOUBLE_CHANCE calls=${calls.map(c=>c.mock.calls.length)} changed=${changed} offFieldApplications=${offFieldApplications}`);
'''+s[b:]
s=s.replace('native biome callbacks preserve immutable weather rejection and terrain replay','native double encounter chance dispatcher and scene getter')
Path(sys.argv[1]).write_text(s)
