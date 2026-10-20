#!/usr/bin/env python3
"""Actual MoneyAbAttr dispatch and raw Pay Day override, complete frames/queues/RNG."""
from pathlib import Path
import subprocess,sys,tempfile
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'base.ts';subprocess.run([sys.executable,r/'tools/build-double-chance-proof.py',p],check=True);s=p.read_text()
s=s.replace('const N=16384,seed=Number(process.env.DOUBLE_CHANCE_SEED??777)','const N=32768+5850,seed=Number(process.env.HONEY_MONEY_SEED??777)').replace('DOUBLE_CHANCE','HONEY_MONEY').replace('double-chance','honey-money')
a=s.index(' const abilities=');b=s.index(' const queues=',a)
s=s[:a]+''' const calls=vi.spyOn(allAbilities[AbilityId.HONEY_GATHER].getAttrs("MoneyAbAttr")[0],"apply");
 const pay=allMoves[MoveId.PAY_DAY].getAttrs("MoneyAttr")[0];
'''+s[b:]
a=s.index(' write([`HONEY_MONEY');b=s.index('  pm.clearAllPhases();',a)
s=s[:a]+''' write([`HONEY_MONEY 1 ${N} ${seed}`]);let changed=0,offFieldApplications=0,wide=0;
 for(let i=0;i<N;i++){
  const payday=i>=32768,u=i%8,simulated=!!(Math.floor(i/8)%2),victory=!!(Math.floor(i/16)%2),shape=Math.floor(i/32)%8;
  const wave=payday?i-32768+1:[1,10,199,200,256,1000,2000,3000,4000,5000,5849,5850,250,100,20,5][Math.floor(i/256)%16];
  const input=[0,1234,2147483647,9007199254740991,1e18+128,1e30,Number.MAX_VALUE,0][Math.floor(i/4096)%8];
  battle.double=true;battle.waveIndex=wave;battle.moneyScattered=input;scene.gameMode=getGameMode(GameModes.ENDLESS);battle.gameMode=scene.gameMode;scene.arena.weather=null;scene.arena.terrain=null;scene.arena.ignoreAbilities=false;scene.arena.ignoringEffectSource=null;scene.modifiers.length=0;
  for(let k=0;k<8;k++){
   const p=actors[k];if(k<4&&scene.field.getIndex(p)<0)scene.field.add(p);
   p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies([SpeciesId.BULBASAUR,SpeciesId.PIDGEY,SpeciesId.DRATINI,SpeciesId.CHARMANDER][k%4]);p.formIndex=0;p.fusionSpecies=null;p.customPokemonData.ability=k===u&&![1,6].includes(shape)?AbilityId.HONEY_GATHER:AbilityId.BALL_FETCH;p.passive=k===u&&[1,2,6].includes(shape);p.customPokemonData.passive=p.passive?AbilityId.HONEY_GATHER:AbilityId.NONE;p.abilityIndex=0;p.level=[1,50,256,1000000][k%4];p.gender=-1;p.isTerastallized=false;p.calculateStats();p.hp=k===u&&shape===4?0:p.getMaxHp();p.status=null;p.moveset=[new PokemonMove(MoveId.SPLASH)];p.visible=false;p.switchOutStatus=k===u&&shape===5;p.summonData.abilitySuppressed=k===u&&shape===3;p.summonData.addedType=null;p.generateName();
  }
  if(shape===7)scene.field.remove(actors[u]);
'''+s[b:]
a=s.index('  const chance=');b=s.index('\n }\n if(',a)
s=s[:a]+'''  const beforeCalls=calls.mock.calls.length;
  write([`C ${i} ${u} ${+simulated} ${+victory} ${+payday} ${wave} ${input}`,`W ${scene.waveSeed}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(battle.battleSeedState)}`,...actors.map((p,k)=>`I ${k} ${snapshot(p).join(" ")}`)]);
  if(payday)pay.apply(actors[u],actors[(u+1)%8],allMoves[MoveId.PAY_DAY]);else applyAbAttrs("MoneyAbAttr",{pokemon:actors[u],victory,simulated});
  changed+=+(battle.moneyScattered!==input);wide+=+(battle.moneyScattered>2147483647);offFieldApplications+=+(shape===7&&calls.mock.calls.length>beforeCalls);
  write([`V ${battle.moneyScattered} ${scene.getWaveMoneyAmount(.2)}`,...actors.map((p,k)=>`O ${k} ${snapshot(p).join(" ")}`),...queues(),`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(battle.battleSeedState)}`,"E"]);
'''+s[b:]
a=s.index(' if(calls.some');b=s.index('\n},600000);',a)
s=s[:a]+''' if(calls.mock.calls.length<1000||changed<1000||offFieldApplications<100||wide<1000)throw Error(`coverage ${calls.mock.calls.length}/${changed}/${offFieldApplications}/${wide}`);
 write([`HONEY_MONEY_COMPLETE ${N}`]);fs.closeSync(fd);console.log(`HONEY_MONEY calls=${calls.mock.calls.length} changed=${changed} offField=${offFieldApplications} wide=${wide}`);
'''+s[b:]
s=s.replace('native double encounter chance dispatcher and scene getter','native Honey Gather dispatcher and Pay Day override')
Path(sys.argv[1]).write_text(s)
