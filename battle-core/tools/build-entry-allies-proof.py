#!/usr/bin/env python3
"""Actual Hospitality / Curious Medicine dispatch, including ability gain."""
from pathlib import Path
import sys
r=Path(__file__).resolve().parents[1]
s=(r/'tests/keldeo-forms.producer.test.ts').read_text().split('// Only scene assets')[0]
s+='''
import { applyAbAttrs, applyOnGainAbAttrs } from "#abilities/apply-ab-attrs";
it("native ally entry callbacks and gain filtering",async()=>{
 const N=32768,seed=Number(process.env.ENTRY_ALLIES_SEED??777),native=BattleScene.prototype.randBattleSeedInt;
 if(!process.versions.node.startsWith("22."))throw Error("Node22 required");
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.battleStyle("double").ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).enemySpecies(SpeciesId.RATTATA).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).disableTrainerWaves().mysteryEncounterChance(0);
 await g.classicMode.startBattle(SpeciesId.BULBASAUR,SpeciesId.CHARMANDER);BattleScene.prototype.randBattleSeedInt=native;
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);
 const scene:any=g.scene,battle:any=scene.currentBattle,pm:any=scene.phaseManager;
 const actors:any[]=[...scene.getPlayerField(),...scene.getEnemyField(),...Array.from({length:2},()=>scene.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.PIDGEY),50)),...Array.from({length:2},()=>scene.addEnemyPokemon(speciesDataRegistry.getSpecies(SpeciesId.PIDGEY),50,TrainerSlot.NONE))];
 for(const p of actors.slice(4,6))scene.getPlayerParty().push(p);
 for(const p of actors.slice(6))scene.getEnemyParty().push(p);
 const fd=fs.openSync(process.env.ENTRY_ALLIES_OUT??"/tmp/entry-allies.txt","w"),write=(rows:string[])=>fs.writeSync(fd,rows.join("\\n")+"\\n");
 const calls=[AbilityId.HOSPITALITY,AbilityId.CURIOUS_MEDICINE].map(id=>vi.spyOn(allAbilities[id].getAttrs("PostSummonAbAttr")[0],"apply"));
 const queues=()=>{const rows:string[]=[];pm.phaseQueue.levels.forEach((lv:any[],l:number)=>lv.forEach((p:any,j:number)=>{
  const name=p.phaseName;
  if(!["PokemonHealPhase","MessagePhase","ShowAbilityPhase","HideAbilityPhase"].includes(name))throw Error(`unexpected queue ${name}`);
  const heal=name==="PokemonHealPhase",show=name==="ShowAbilityPhase";
  const flags=heal?(+(p.message!==undefined)|(+!p.showFullHpMessage<<1)|(+p.skipAnim<<2)|(+p.revive<<3)|(+p.healStatus<<4)|(+p.preventFullHeal<<5)|(+p.fullRestorePP<<6)):0;
  const text=heal?hex(p.message??""):name==="MessagePhase"?hex(p.text):show?hex(p.pokemonName??"")+"."+hex(p.abilityName??""):"-";
  rows.push(`P ${l} ${j} ${name} ${heal||show?p.battlerIndex:-1} ${show?+p.passive:-1} ${name==="MessagePhase"?+!!p.prompt:-1} ${text} ${heal?p.hpHealed:0} ${flags}`);
 }));return [`Q ${rows.length} ${pm.phaseQueue.levels.length} ${pm.phaseQueue.currentLevel} ${+pm.phaseQueue.deferredActive}`,...rows];};
 write([`ENTRY_ALLIES 1 ${N} ${seed}`]);let changed=0,heals=0;
 for(let i=0;i<N;i++){
  const u=i%8,kind=Math.floor(i/8)%2,simulated=!!(Math.floor(i/16)%2),shape=Math.floor(i/32)%8,gain=!!(Math.floor(i/256)%2),single=!!(Math.floor(i/512)%2);
  battle.double=!single;scene.gameMode=getGameMode(GameModes.CLASSIC);
  for(let k=0;k<8;k++){
   const p=actors[k];p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies([SpeciesId.BULBASAUR,SpeciesId.BLISSEY,SpeciesId.SHUCKLE,SpeciesId.CHARMANDER][k%4]);p.formIndex=0;p.fusionSpecies=null;p.customPokemonData.ability=k===u&&shape!==1?[AbilityId.HOSPITALITY,AbilityId.CURIOUS_MEDICINE][kind]:AbilityId.BALL_FETCH;p.passive=k===u&&[1,2].includes(shape);p.customPokemonData.passive=p.passive?[AbilityId.HOSPITALITY,AbilityId.CURIOUS_MEDICINE][kind]:AbilityId.NONE;p.abilityIndex=0;p.level=[1,50,256,1000000][Math.floor(i/1024)%4];p.gender=-1;p.isTerastallized=false;p.calculateStats();p.hp=p.getMaxHp();p.status=null;p.moveset=[new PokemonMove(MoveId.SPLASH)];p.visible=false;p.switchOutStatus=false;p.summonData.abilitySuppressed=k===u&&shape===3;p.summonData.addedType=null;
   if(k===u&&shape===4)p.hp=0;
   p.summonData.statStages=[1,-2,3,-4,5,-6,6].map((v,j)=>(i+k+j)%3? v:0);p.generateName();
  }
  const ally=actors[u].getAlly();if(ally){if(shape===5)ally.hp=0;else if(shape===6)ally.hp=Math.max(1,Math.floor(ally.hp/2));if(shape===7)ally.summonData.statStages.fill(0);}
  pm.clearAllPhases();pm.pushNew("MessagePhase","anchor");pm.currentPhase=pm.create("MovePhase",actors[0],[2],new PokemonMove(MoveId.SPLASH),MoveUseMode.NORMAL);
  battle.battleSeed=`entry-allies-${seed}-${i}`;battle.battleSeedState=null;Phaser.Math.RND.sow([`entry-allies-${seed}-${i}`]);battle.randSeedInt(2);
  const before=actors.map(p=>snapshot(p).join(" ")).join(";");
  write([`C ${i} ${u} ${+simulated} ${+gain} ${+single}`,`W ${scene.waveSeed}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(battle.battleSeedState)}`,...actors.map((p,k)=>`I ${k} ${snapshot(p).join(" ")}`)]);
  if(gain)applyOnGainAbAttrs({pokemon:actors[u],simulated});else applyAbAttrs("PostSummonAbAttr",{pokemon:actors[u],simulated});
  heals+=pm.phaseQueue.levels.flat().filter((p:any)=>p.phaseName==="PokemonHealPhase").length;changed+=+(before!==actors.map(p=>snapshot(p).join(" ")).join(";"));
  write([...actors.map((p,k)=>`O ${k} ${snapshot(p).join(" ")}`),...queues(),`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(battle.battleSeedState)}`,"E"]);
 }
 if(calls.some(c=>c.mock.calls.length<1000)||heals<1000||changed<1000)throw Error(`coverage ${calls.map(c=>c.mock.calls.length)}/${heals}/${changed}`);
 write([`ENTRY_ALLIES_COMPLETE ${N}`]);fs.closeSync(fd);console.log(`ENTRY_ALLIES calls=${calls.map(c=>c.mock.calls.length)} heals=${heals} changed=${changed}`);
},600000);
'''
Path(sys.argv[1]).write_text(s)
