#!/usr/bin/env python3
"""Native PostBiome dispatcher and distinct ability-gain weather/terrain paths."""
from pathlib import Path
import subprocess,sys,tempfile
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'base.ts';subprocess.run([sys.executable,r/'tools/build-poison-puppeteer-proof.py',p],check=True);s=p.read_text()
def put(a,b):
 global s
 assert s.count(a)==1,a[:100]
 s=s.replace(a,b,1)
put('import { Terrain } from "#data/terrain";','import { Weather } from "#data/weather";\nimport { Terrain } from "#data/terrain";')
put('const N=65536,seed=Number(process.env.PUPPETEER_SEED??777)','const N=26880,seed=Number(process.env.BIOME_CALLBACK_SEED??777)')
s=s.replace('process.env.PUPPETEER_OUT??"/tmp/puppeteer.txt"','process.env.BIOME_CALLBACK_OUT??"/tmp/biome-callback.txt"').replace('PUPPETEER 1','BIOME_CALLBACK 1').replace('PUPPETEER_COMPLETE','BIOME_CALLBACK_COMPLETE')
a=s.index(' const applied=');b=s.index(' const queues=',a)
s=s[:a]+''' const abilities=[AbilityId.DRIZZLE,AbilityId.SAND_STREAM,AbilityId.DROUGHT,AbilityId.SNOW_WARNING,AbilityId.PRIMORDIAL_SEA,AbilityId.DESOLATE_LAND,AbilityId.DELTA_STREAM,AbilityId.ORICHALCUM_PULSE,AbilityId.ELECTRIC_SURGE,AbilityId.PSYCHIC_SURGE,AbilityId.MISTY_SURGE,AbilityId.GRASSY_SURGE,AbilityId.HADRON_ENGINE];
 const variants=abilities.flatMap((ab,index)=>Array.from({length:index<8?10:5},(_,state)=>({ab,w:index<8?state:(state*2)%10,t:index<8?state%5:state})));
 if(variants.length!==105)throw Error("variant inventory");
 const calls=abilities.map(id=>vi.spyOn(allAbilities[id].getAttrs("PostBiomeChangeAbAttr")[0],"apply"));
 const env=()=>{const w=scene.arena.weather,t=scene.arena.terrain;return [w?.weatherType??0,w?.turnsLeft??0,w?.maxDuration??0,t?.terrainType??0,t?.turnsLeft??0,t?.maxDuration??0].join(" ");};
'''+s[b:]
a=s.index(' const tags=');b=s.index(' for(let i=',a)
s=s[:a]+''' write([`BIOME_CALLBACK 1 ${N} ${seed}`]);let changed=0;
'''+s[b:]
a=s.index('  const u=');b=s.index('  pm.clearAllPhases();',a)
s=s[:a]+'''  const u=i%8,simulated=!!(Math.floor(i/8)%2),shape=Math.floor(i/16)%8,gain=!!(Math.floor(i/128)%2),v=variants[Math.floor(i/256)];
  battle.double=true;scene.gameMode=getGameMode(GameModes.CLASSIC);scene.arena.weather=v.w?new Weather(v.w,7,11):null;scene.arena.terrain=v.t?new Terrain(v.t,3,9):null;scene.arena.ignoreAbilities=false;scene.arena.ignoringEffectSource=null;
  for(let k=0;k<8;k++){
   const p=actors[k];p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies([SpeciesId.BULBASAUR,SpeciesId.PIDGEY,SpeciesId.DRATINI,SpeciesId.CHARMANDER][k%4]);p.formIndex=0;p.fusionSpecies=null;p.customPokemonData.ability=k===u&&![1,6].includes(shape)?v.ab:AbilityId.BALL_FETCH;p.passive=k===u&&[1,2,6].includes(shape);p.customPokemonData.passive=p.passive?v.ab:AbilityId.NONE;p.abilityIndex=0;p.level=[1,50,256,1000000][k%4];p.gender=-1;p.isTerastallized=false;p.calculateStats();p.hp=k===u&&shape===4?0:p.getMaxHp();p.status=null;p.moveset=[new PokemonMove(MoveId.SPLASH)];p.visible=false;p.switchOutStatus=k===u&&shape===5;p.summonData.abilitySuppressed=k===u&&shape===3;p.summonData.addedType=null;p.generateName();
  }
  if(shape===6)actors[u].customPokemonData.ability=AbilityId.NONE;
  if(shape===7){const other=actors[(u+1)%4];other.customPokemonData.ability=AbilityId.MIMICRY;}
'''+s[b:]
put('pm.currentPhase=context?pm.create("MoveEffectPhase",0,[2],allMoves[MoveId.TOXIC],MoveUseMode.NORMAL):pm.create("MovePhase",actors[0],[2],new PokemonMove(MoveId.SPLASH),MoveUseMode.NORMAL);','pm.currentPhase=pm.create("MovePhase",actors[0],[2],new PokemonMove(MoveId.SPLASH),MoveUseMode.NORMAL);')
s=s.replace('`puppeteer-${seed}-${i}`','`biome-callback-${seed}-${i}`')
a=s.index('  const before=target.getTag');b=s.index('\n }\n const durations=',a)
s=s[:a]+'''  const before=env();
  write([`C ${i} ${u} ${+simulated} ${+gain}`,`ENV ${before}`,`W ${scene.waveSeed}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(battle.battleSeedState)}`,...actors.map((p,k)=>`I ${k} ${snapshot(p).join(" ")}`)]);
  if(gain)applyOnGainAbAttrs({pokemon:actors[u],simulated});else applyAbAttrs("PostBiomeChangeAbAttr",{pokemon:actors[u],simulated});
  changed+=+(before!==env());
  write([...actors.map((p,k)=>`O ${k} ${snapshot(p).join(" ")}`),`ENVO ${env()}`,...queues(),`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(battle.battleSeedState)}`,"E"]);
'''+s[b:]
a=s.index(' const durations=');b=s.index('\n},600000);',a)
s=s[:a]+''' if(calls.some(c=>c.mock.calls.length<32)||changed<1000)throw Error(`coverage ${calls.map(c=>c.mock.calls.length)}/${changed}`);
 write([`BIOME_CALLBACK_COMPLETE ${N}`]);fs.closeSync(fd);console.log(`BIOME_CALLBACK calls=${calls.map(c=>c.mock.calls.length)} changed=${changed}`);
'''+s[b:]
put('it("native Poison Puppeteer gates and spent Misty Terrain RNG"','it("native biome callbacks preserve immutable weather rejection and terrain replay"')
Path(sys.argv[1]).write_text(s)
