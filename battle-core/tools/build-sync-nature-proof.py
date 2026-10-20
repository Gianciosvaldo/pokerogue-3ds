#!/usr/bin/env python3
"""Actual Synchronize dispatch with independent raw/effective/custom nature state."""
from pathlib import Path
import subprocess,sys,tempfile
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'base.ts';subprocess.run([sys.executable,r/'tools/build-double-chance-proof.py',p],check=True);s=p.read_text()
s=s.replace('const N=16384,seed=Number(process.env.DOUBLE_CHANCE_SEED??777)','const N=102400,seed=Number(process.env.SYNC_NATURE_SEED??777)')
s=s.replace('process.env.DOUBLE_CHANCE_OUT??"/tmp/double-chance.txt"','process.env.SYNC_NATURE_OUT??"/tmp/sync-nature.txt"').replace('DOUBLE_CHANCE','SYNC_NATURE')
s=s.replace('p.level, p.nature, p.hp','p.level, p.getNature(), p.hp')
a=s.index(' const abilities=');b=s.index(' const queues=',a)
s=s[:a]+''' const calls=vi.spyOn(allAbilities[AbilityId.SYNCHRONIZE].getAttrs("SyncEncounterNatureAbAttr")[0],"apply");
 const metadata=(key:string)=>actors.map((p,k)=>`${key} ${k} ${p.nature} ${p.customPokemonData.nature} ${p.getNature()}`);
'''+s[b:]
a=s.index(' write([`SYNC_NATURE');b=s.index('  pm.clearAllPhases();',a)
s=s[:a]+''' write([`SYNC_NATURE 1 ${N} ${seed}`]);let changed=0,simulatedChanges=0,customChanges=0;
 for(let i=0;i<N;i++){
  const u=i%8,t=Math.floor(i/8)%8,simulated=!!(Math.floor(i/64)%2),shape=Math.floor(i/128)%8,nature=Math.floor(i/1024)%25,custom=Math.floor(i/25600)%4;
  battle.double=true;scene.gameMode=getGameMode(GameModes.CLASSIC);scene.arena.weather=null;scene.arena.terrain=null;scene.arena.ignoreAbilities=false;scene.arena.ignoringEffectSource=null;
  scene.modifiers.length=0;
  for(let k=0;k<8;k++){
   const p=actors[k];if(k<4&&scene.field.getIndex(p)<0)scene.field.add(p);
   p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies([SpeciesId.BULBASAUR,SpeciesId.PIDGEY,SpeciesId.DRATINI,SpeciesId.CHARMANDER][k%4]);p.formIndex=0;p.fusionSpecies=null;p.customPokemonData.ability=k===u&&![1,7].includes(shape)?AbilityId.SYNCHRONIZE:AbilityId.BALL_FETCH;p.passive=k===u&&[1,2,7].includes(shape);p.customPokemonData.passive=p.passive?AbilityId.SYNCHRONIZE:AbilityId.NONE;p.abilityIndex=0;p.level=[1,50,256,1000000][k%4];p.gender=-1;p.isTerastallized=false;
   p.nature=(nature+3*k)%25;p.customPokemonData.nature=-1;if(k===u&&(custom&1))p.customPokemonData.nature=(nature+7)%25;if(k===t&&(custom&2))p.customPokemonData.nature=(nature+11)%25;
   p.calculateStats();p.hp=k===u&&shape===4?0:p.getMaxHp();p.status=null;p.moveset=[new PokemonMove(MoveId.SPLASH)];p.visible=false;p.switchOutStatus=k===u&&shape===5;p.summonData.abilitySuppressed=k===u&&shape===3;p.summonData.addedType=null;p.generateName();
  }
  if(shape===7)actors[u].customPokemonData.ability=AbilityId.NONE;
  if(shape===6)scene.field.remove(actors[u]);
'''+s[b:]
s=s.replace('`double-chance-${seed}-${i}`','`sync-nature-${seed}-${i}`')
a=s.index('  const chance=new NumberHolder');b=s.index('\n }\n if(',a)
s=s[:a]+'''  const useSceneSetter=+(actors[t].customPokemonData.nature>=0||i%2),before=actors[t].nature;
  write([`C ${i} ${u} ${t} ${+simulated} ${useSceneSetter}`,`W ${scene.waveSeed}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(battle.battleSeedState)}`,...actors.map((p,k)=>`I ${k} ${snapshot(p).join(" ")}`),...metadata("NI")]);
  applyAbAttrs("SyncEncounterNatureAbAttr",{pokemon:actors[u],target:actors[t],simulated});
  const change=before!==actors[t].nature;changed+=+change;simulatedChanges+=+(simulated&&change);customChanges+=+(actors[t].customPokemonData.nature>=0&&change);
  write([...actors.map((p,k)=>`O ${k} ${snapshot(p).join(" ")}`),...metadata("NO"),...queues(),`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(battle.battleSeedState)}`,"E"]);
'''+s[b:]
a=s.index(' if(calls.some');b=s.index('\n},600000);',a)
s=s[:a]+''' if(calls.mock.calls.length<1000||changed<1000||simulatedChanges<1000||customChanges<1000)throw Error(`coverage ${calls.mock.calls.length}/${changed}/${simulatedChanges}/${customChanges}`);
 write([`SYNC_NATURE_COMPLETE ${N}`]);fs.closeSync(fd);console.log(`SYNC_NATURE calls=${calls.mock.calls.length} changed=${changed} simulatedChanges=${simulatedChanges} customChanges=${customChanges}`);
'''+s[b:]
s=s.replace('native double encounter chance dispatcher and scene getter','native Synchronize raw and custom nature setter semantics')
Path(sys.argv[1]).write_text(s)
