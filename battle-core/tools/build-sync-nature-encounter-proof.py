#!/usr/bin/env python3
"""Actual EncounterPhase.start; observe Sync's post-constructor prefix with passthrough calls."""
from pathlib import Path
import subprocess,sys,tempfile
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'base.ts';subprocess.run([sys.executable,r/'tools/build-sync-nature-proof.py',p],check=True);s=p.read_text()
s=s.replace('import { applyAbAttrs, applyOnGainAbAttrs }','import * as abilityDispatch from "#abilities/apply-ab-attrs";\nimport { applyAbAttrs, applyOnGainAbAttrs }')
s=s.replace('const N=102400','const N=1536').replace('process.env.SYNC_NATURE_OUT??"/tmp/sync-nature.txt"','process.env.SYNC_ENCOUNTER_OUT??"/tmp/sync-encounter.txt"').replace('SYNC_NATURE 1','SYNC_NATURE 2')
a=s.index(' write([`SYNC_NATURE');s=s[:a]+'''
 const dispatch=abilityDispatch.applyAbAttrs;
 vi.spyOn(Pokemon.prototype,"loadAssets").mockImplementation(()=>new Promise<void>(()=>{}));
 vi.spyOn(Pokemon.prototype,"updateInfo").mockImplementation(()=>Promise.resolve());
 let groups=0,inside=0,custom=0,single=false;
 const watch=vi.spyOn(abilityDispatch,"applyAbAttrs").mockImplementation((type:any,params:any,config:any)=>{
  if(type!=="SyncEncounterNatureAbAttr")return dispatch(type,params,config);
  const t=battle.enemyParty.indexOf(params.target)+2;if(t<2||t>3)throw Error("unexpected native sync target");
  if(!inside){
   actors[t]=params.target;if(custom&2)actors[t].setCustomNature((groups+11)%25);
   write([`C ${groups} -1 ${t} 0 1 ${+single}`,`W ${scene.waveSeed}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(battle.battleSeedState)}`,...actors.map((p,k)=>`I ${k} ${snapshot(p).join(" ")}`),...metadata("NI")]);
  }
  dispatch(type,params,config);inside++;
  if(inside===(single?1:2)){
   write([...actors.map((p,k)=>`O ${k} ${snapshot(p).join(" ")}`),...metadata("NO"),...queues(),`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(battle.battleSeedState)}`,"E"]);groups++;inside=0;
  }
 });
 write([`SYNC_NATURE 2 ${N} ${seed}`]);
 for(let i=0;i<1024;i++){
  const shape=i%8,nature=Math.floor(i/32)%4;custom=Math.floor(i/8)%4;single=!!(Math.floor(i/128)%2);
  scene.currentBattle=battle;battle.double=!single;battle.waveIndex=1;battle.enemyLevels=Array(single?1:2).fill([1,50,256,1000000][nature]);scene.gameMode=getGameMode(GameModes.CLASSIC);
  g.override.enemySpecies([SpeciesId.MAGIKARP,SpeciesId.BULBASAUR,SpeciesId.ABRA,SpeciesId.DITTO][Math.floor(i/256)]);
  scene.arena.weather=null;scene.arena.terrain=null;scene.arena.ignoreAbilities=false;scene.arena.ignoringEffectSource=null;scene.modifiers.length=0;
  for(let k=0;k<8;k++){
   const p=actors[k];if(k<4&&scene.field.getIndex(p)<0)scene.field.add(p);p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();
   p.customPokemonData.ability=k===1||(k===0&&![1,2].includes(shape))?AbilityId.SYNCHRONIZE:AbilityId.BALL_FETCH;p.passive=k===0&&[2,3].includes(shape);p.customPokemonData.passive=p.passive?AbilityId.SYNCHRONIZE:AbilityId.NONE;p.nature=(5*nature+3*k)%25;p.customPokemonData.nature=k<2&&(custom&1)?(5*nature+7+k)%25:-1;
   p.calculateStats();p.hp=k===0&&shape===4?0:p.getMaxHp();p.status=null;p.visible=false;p.switchOutStatus=k===0&&shape===6;p.summonData.abilitySuppressed=k===0&&shape===5;p.generateName();
  }
  if(shape===7)scene.field.remove(actors[0]);
  pm.clearAllPhases();pm.pushNew("MessagePhase","anchor");const phase=pm.create("EncounterPhase",false);pm.currentPhase=phase;
  battle.battleSeed=`sync-encounter-${seed}-${i}`;battle.battleSeedState=null;Phaser.Math.RND.sow([`sync-encounter-${seed}-${i}`]);battle.randSeedInt(2);
  const before=groups;phase.start();if(inside||groups-before!==(single?1:2))throw Error(`missing actual native sync owner ${i}/${groups-before}/${inside}`);
 }
 watch.mockRestore();if(groups!==N||calls.mock.calls.length<1000)throw Error(`coverage ${groups}/${calls.mock.calls.length}`);
 write([`SYNC_NATURE_COMPLETE ${N}`]);fs.closeSync(fd);console.log(`SYNC_ENCOUNTER groups=${groups} actualApplyCalls=${calls.mock.calls.length}`);
},600000);
'''
s=s.replace(' const dispatch=abilityDispatch.applyAbAttrs;',' const challenge=process.env.SYNC_CHALLENGE==="1";\n const dispatch=abilityDispatch.applyAbAttrs;')
s=s.replace('SYNC_NATURE 2 ${N}', 'SYNC_NATURE ${challenge?3:2} ${N}')
s=s.replace('${+single}`,`W ${scene.waveSeed}', '${+single}${challenge?" "+scene.gameMode.modeId:""}`,`W ${scene.waveSeed}')
s=s.replace('scene.gameMode=getGameMode(GameModes.CLASSIC);', 'scene.gameMode=getGameMode(challenge?GameModes.CHALLENGE:GameModes.CLASSIC);battle.gameMode=scene.gameMode;if(challenge&&scene.gameMode.challenges.some((c:any)=>c.value!==0))throw Error("unexpected selected challenge");')
s=s.replace('native Synchronize raw and custom nature setter semantics','native EncounterPhase Synchronize prefix after actual wild constructors')
Path(sys.argv[1]).write_text(s)
