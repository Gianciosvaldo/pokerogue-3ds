#!/usr/bin/env python3
"""Actual stock-mode cleanup -> Return -> NewBiomeEncounter callback reachability."""
from pathlib import Path
import sys
r=Path(__file__).resolve().parents[1]
s=(r/'tests/keldeo-forms.producer.test.ts').read_text().split('// Only scene assets')[0]
s+='''
it("stock biome cleanup recalls all players before biome callbacks",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).enemySpecies(SpeciesId.RATTATA).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.BULBASAUR,SpeciesId.CHARMANDER);BattleScene.prototype.randBattleSeedInt=native;
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);
 const scene:any=g.scene,pm:any=scene.phaseManager,battle:any=scene.currentBattle,players:any[]=scene.getPlayerParty();
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});
 vi.spyOn(audioManager,"playSound").mockImplementation(()=>{});vi.spyOn(audioManager,"playBgm").mockImplementation(()=>{});
 vi.spyOn(scene,"updateFieldScale").mockImplementation(async()=>{});
 vi.spyOn(scene.ui,"showText").mockImplementation((_t:any,_d:any,callback:any)=>{callback?.();});
 const timers:(()=>void)[]=[];vi.spyOn(scene.time,"delayedCall").mockImplementation((_d:any,callback:any)=>{timers.push(callback);return{} as any;});
 for(const p of players){vi.spyOn(p,"updateInfo").mockImplementation(()=>Promise.resolve());vi.spyOn(p,"hideInfo").mockImplementation(()=>Promise.resolve());vi.spyOn(p,"playAnim").mockImplementation(()=>{});vi.spyOn(p,"faintCry").mockImplementation((cb:any)=>cb());}
 const attr=allAbilities[AbilityId.ELECTRIC_SURGE].getAttrs("PostBiomeChangeAbAttr")[0],calls=vi.spyOn(attr,"apply");
 let cases=0,returned=0,pending=0,deferTween=false;
 scene.tweens.add=(c:any)=>{if(deferTween)pending++;else c.onComplete?.();return{};};
 for(const mode of [GameModes.CLASSIC,GameModes.CHALLENGE,GameModes.ENDLESS,GameModes.SPLICED_ENDLESS,GameModes.DAILY])for(const double of [false,true]){
  scene.gameMode=getGameMode(mode);if(scene.gameMode.hasRandomBiomes)throw Error("stock config unexpectedly random-biome");
  battle.waveIndex=11;battle.double=double;battle.enemyParty=[];scene.arena.weather=null;scene.arena.terrain=null;scene.modifiers=[];scene.enemyModifiers=[];scene.trainer.setVisible(true);
  players.forEach((p,k)=>{p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies(SpeciesId.BULBASAUR);p.formIndex=0;p.fusionSpecies=null;p.customPokemonData.ability=k===0?AbilityId.ELECTRIC_SURGE:AbilityId.BALL_FETCH;p.customPokemonData.passive=AbilityId.NONE;p.passive=false;p.pokerus=false;p.switchOutStatus=false;p.isTerastallized=false;p.calculateStats();p.hp=p.getMaxHp();if(k<(double?2:1)){if(!p.isOnField())scene.field.add(p);}else scene.field.remove(p);});
  pm.clearAllPhases();pm.phaseQueue.deferredActive=false;timers.length=0;deferTween=false;
  scene.doPostBattleCleanup({waveIndex:10,double},scene.getMaxExpLevel());
  let nReturn=0,encounter=false;
  for(let guard=0;guard<32;guard++){
   let ph=pm.phaseQueue.getNextPhase();if(ph?.phaseName==="DynamicPhaseMarker")ph=pm.dynamicQueueManager.popNextPhase(ph.phaseType);if(!ph)break;pm.currentPhase=ph;
   if(ph.phaseName==="NewBiomeEncounterPhase"){
    if(players.some(p=>p.isOnField()))throw Error(`unrecalled source player mode${mode} double${double}`);
    if(players[0].getAbility().id!==AbilityId.ELECTRIC_SURGE||players[0].hp<=0)throw Error("inactive proof lost its eligible Surge user");
    const before=calls.mock.calls.length,oldPending=pending;deferTween=true;ph.doEncounter();if(calls.mock.calls.length!==before||pending!==oldPending+1)throw Error("inactive stock biome callback or missing visual continuation");encounter=true;break;
   }
   if(!["ReturnPhase","ShowTrainerPhase"].includes(ph.phaseName))throw Error(`unexpected stock cleanup phase ${ph.phaseName}`);
   nReturn+=+(ph.phaseName==="ReturnPhase");await ph.start();for(let tick=0;tick<8;tick++){while(timers.length)timers.shift()!();await Promise.resolve();}
  }
  if(!encounter||nReturn!==(double?2:1))throw Error(`stock sequence mode${mode} double${double} returns${nReturn}`);
  returned+=nReturn;cases++;
 }
 if(cases!==10||returned!==15||pending!==10||calls.mock.calls.length)throw Error(`coverage ${cases}/${returned}/${pending}/${calls.mock.calls.length}`);
 console.log(`BIOME_STOCK_REACHABILITY modes=5 cases=${cases} nativeReturns=${returned} prefixes=${pending} callbackApplies=${calls.mock.calls.length}`);
},600000);
'''
Path(sys.argv[1]).write_text(s)
