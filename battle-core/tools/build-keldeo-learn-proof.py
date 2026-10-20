#!/usr/bin/env python3
"""Real LearnMovePhase.start through its text callback and queued nonquiet form."""
from pathlib import Path
import sys
r=Path(__file__).resolve().parents[1];s=(r/'tests/keldeo-forms.producer.test.ts').read_text()
s=s.replace('import { TrainerSlot }','import * as battleAnims from "#data/battle-anims";\nimport { TrainerSlot }')
s=s.replace('process.env.KELDEO_SEED','process.env.KELDEO_LEARN_SEED').replace('process.env.KELDEO_OUT??"/tmp/keldeo.txt"','process.env.KELDEO_LEARN_OUT??"/tmp/keldeo-learn.txt"').replace('const N=33792','const N=32768')
a=' if(actors.length!==8)throw Error("eight natural actors required");'
b=a+'''
 for(const p of [actors[4],actors[5]])scene.getPlayerParty().push(p);
 for(const p of [actors[6],actors[7]])scene.getEnemyParty().push(p);
 if(scene.getPlayerParty().length!==4||scene.getEnemyParty().length!==4)throw Error("actual party benches required");
 vi.spyOn(battleAnims,"initMoveAnim").mockImplementation(async()=>{});
 vi.spyOn(battleAnims,"loadMoveAnimAssets").mockImplementation(()=>{});
 vi.spyOn(scene.ui,"setMode").mockImplementation(()=>Promise.resolve());
'''
assert s.count(a)==1;s=s.replace(a,b)
a=s.index(' write([`KELDEO 5 ');b=s.index('\n},600000);',a)
s=s[:a]+'''
 write([`KELDEO_LEARN 2 ${N} ${seed}`]);let learned=0,queued=0,executed=0;
 for(let i=0;i<N;i++){
  const u=[0,1,4,5][i%4],form=Math.floor(i/4)%2,move=Math.floor(i/8)%2?MoveId.SECRET_SWORD:MoveId.SPLASH,mode=Math.floor(i/16)%2,tagged=!!(Math.floor(i/32)%2),tera=!!(Math.floor(i/64)%2),fainted=!!(Math.floor(i/128)%2),gm=[GameModes.CLASSIC,GameModes.DAILY,GameModes.ENDLESS,GameModes.CHALLENGE][Math.floor(i/256)%4];scene.gameMode=getGameMode(gm);
  for(let k=0;k<8;k++){
   const p=actors[k];p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies(k===u?SpeciesId.KELDEO:SpeciesId.BULBASAUR);p.formIndex=k===u?form:0;p.fusionSpecies=null;p.customPokemonData.ability=k===u?[AbilityId.BALL_FETCH,AbilityId.DISGUISE,AbilityId.MIMICRY,AbilityId.DRIZZLE][Math.floor(i/4096)%4]:AbilityId.BALL_FETCH;p.passive=false;p.customPokemonData.passive=AbilityId.NONE;p.level=[1,50,256,1000000][Math.floor(i/1024)%4];p.gender=-1;p.abilityIndex=0;p.calculateStats();p.hp=k===u&&fainted?0:p.getMaxHp();p.status=null;p.moveset=[];p.visible=false;p.switchOutStatus=false;p.isTerastallized=k===u&&tera;p.teraType=18;p.stellarTypesBoosted=p.isTerastallized?[1,2]:[];p.summonData.addedType=11;p.generateName();
   if(k===u&&tagged){const t:any=getBattlerTag(BattlerTagType.AUTOTOMIZED);t.autotomizeCount=2;p.summonData.tags.push(t);}
  }
  pm.clearAllPhases();timers.length=0;texts.length=0;
  battle.battleSeed=`keldeo-learn-${seed}-${i}`;battle.battleSeedState=null;Phaser.Math.RND.sow([`keldeo-learn-${seed}-${i}`]);battle.randSeedInt(2);
  const dex=scene.gameData.dexData[SpeciesId.KELDEO];
  write([`C ${i} ${u} ${mode} ${gm} ${move} ${+(i>=16384)}`,`W ${scene.waveSeed}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(battle.battleSeedState)}`,...actors.map((p,k)=>`I ${k} ${snapshot(p).join(" ")}`),...actors.map((p,k)=>`M ${k} ${p.abilityIndex} ${p.customPokemonData.ability??-1} ${p.customPokemonData.passive??-1}`),`IT ${actors.map(p=>p.getTag(BattlerTagType.AUTOTOMIZED)?.autotomizeCount??0).join(" ")}`,`D ${dex.seenAttr??0n} ${dex.caughtAttr??0n}`]);
  if(i>=16384)pm.pushNew("LearnMovePhase",scene.getPlayerParty().indexOf(actors[u]),MoveId.GROWL);
  const p=actors[u],phase=pm.create("LearnMovePhase",scene.getPlayerParty().indexOf(p),move);pm.currentPhase=phase;shifted.mockClear();phase.start();
  for(let turn=0;turn<64&&!shifted.mock.calls.length;turn++){while(timers.length)timers.shift()!();await Promise.resolve();}
  if(!shifted.mock.calls.length)throw Error(`learn callback not ended ${i}`);learned++;
  const forms=pm.phaseQueue.levels.flat().filter((ph:any)=>ph.phaseName==="FormChangePhase");
  if(forms.length>1||pm.phaseQueue.levels.flat().some((ph:any)=>ph.phaseName!=="FormChangePhase"&&ph.phaseName!=="LearnMovePhase"))throw Error("unexpected learned queue");
  queued+=forms.length;write([`F ${forms.length}`,...forms.map((ph:any)=>`FORM ${actor(ph.pokemon)} ${ph.pokemon.species.forms.findIndex((f:any)=>f.formKey===ph.formChange.formKey)} ${pm.phaseQueue.levels.findIndex((lv:any[])=>lv.includes(ph))} ${pm.phaseQueue.levels.flat().indexOf(ph)}`)]);
  if(mode)for(const ph of forms){pm.currentPhase=ph;shifted.mockClear();await ph.start();for(let turn=0;turn<64&&!shifted.mock.calls.length;turn++){while(timers.length)timers.shift()!();await Promise.resolve();}if(!shifted.mock.calls.length)throw Error(`form not ended ${i}`);executed++;}
  write([...actors.map((p,k)=>`O ${k} ${snapshot(p).join(" ")}`),`OT ${actors.map(p=>p.getTag(BattlerTagType.AUTOTOMIZED)?.autotomizeCount??0).join(" ")}`,`DO ${dex.seenAttr??0n} ${dex.caughtAttr??0n}`,`T ${texts.length}`,...texts.map(t=>`TEXT ${hex(t)}`),`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(battle.battleSeedState)}`,"E"]);
 }
 if(learned!==N||queued<1000||executed<1000)throw Error(`coverage ${learned}/${queued}/${executed}`);write([`KELDEO_LEARN_COMPLETE ${N}`]);fs.closeSync(fd);console.log(`KELDEO_LEARN learned=${learned} queued=${queued} executed=${executed}`);
'''+s[b:]
s=s.replace('it("native Keldeo learned-move form queues and phase execution"','it("actual LearnMove callback queues cached Keldeo form and updates profile"')
Path(sys.argv[1]).write_text(s)
