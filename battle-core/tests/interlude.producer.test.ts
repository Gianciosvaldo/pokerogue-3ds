/* Actual CheckInterludePhase -> PhaseManager.onInterlude / PhaseTree. */
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it,vi } from "vitest";
it("actual interlude queue selection",async()=>{
 const N=Number(process.env.INTERLUDE_N??8192),seed=Number(process.env.INTERLUDE_SEED??777);
 if(!Number.isSafeInteger(N)||N<512||!process.versions.node.startsWith("22."))throw Error("Node22,N>=512");
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).enemySpecies(SpeciesId.MAGIKARP);await g.classicMode.startBattle(SpeciesId.BULBASAUR);
 const s:any=g.scene,pm:any=s.phaseManager;vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});
 const enemies=Array.from({length:6},()=>s.addEnemyPokemon(SpeciesId.MAGIKARP,20));
 const kinds=["WeatherEffectPhase","BerryPhase","CheckStatusEffectPhase","PositionalTagPhase","TurnEndPhase","PostTurnStatusEffectPhase","HideAbilityPhase","MessagePhase"];
 const waves=[1,9,10,11,20,199,200,201,1000,0];const rows=[`INTERLUDE 1 ${N} ${seed}`];let rng=seed;
 const rnd=(n:number)=>{rng=(Math.imul(rng,1103515245)+12345)>>>0;return(rng>>>8)%n;};
 const rs=(r:string)=>r.replace(/^!rnd,/,"").replaceAll(","," ");
 for(let i=0;i<N;i++){
  const count=[0,1,2,6][i%4],wave=waves[Math.floor(i/4)%waves.length],levels=1+Math.floor(i/40)%4,alive=Math.floor(i/160)%8;
  const party=enemies.slice(0,count);party.forEach((p:any,k:number)=>p.hp=alive===7||alive===k?5:0);s.currentBattle.enemyParty=party;s.currentBattle.waveIndex=wave;
  pm.clearAllPhases();pm.phaseQueue.levels=Array.from({length:levels},()=>[]);let id=0;
  for(let l=0;l<levels;l++)for(let k=0;k<4+rnd(9);k++){
   const kind=rnd(kinds.length),name=kinds[kind],ph=pm.create(name as any,...(name==="MessagePhase"?[""]:name==="WeatherEffectPhase"||name==="PostTurnStatusEffectPhase"?[0]:[]));
   ph.transportId=id++;ph.transportKind=kind;if(name==="TurnEndPhase")ph.upcomingInterlude=!!(i%7===0);pm.phaseQueue.levels[l].push(ph);
  }
  const dump=(tag:string)=>{rows.push(`${tag} ${pm.phaseQueue.levels.length}`);pm.phaseQueue.levels.forEach((level:any[],l:number)=>{rows.push(`L ${l} ${level.length}`);for(const p of level)rows.push(`P ${p.transportId} ${p.transportKind} ${+!!p.upcomingInterlude}`);});};
  Phaser.Math.RND.sow([`interlude-${seed}-${i}`]);s.currentBattle.battleSeedState=null;s.currentBattle.randSeedInt(2);
  rows.push(`C ${i} ${wave} ${count}`,`H ${party.map((p:any)=>p.hp).join(" ")||"-"}`,`R ${rs(Phaser.Math.RND.state())}`,`B ${rs(s.currentBattle.battleSeedState)}`);dump("I");
  const ph=pm.create("CheckInterludePhase");pm.currentPhase=ph;ph.start();dump("O");rows.push(`RO ${rs(Phaser.Math.RND.state())}`,`BO ${rs(s.currentBattle.battleSeedState)}`,"E");
 }
 rows.push(`INTERLUDE_COMPLETE ${N}`);fs.writeFileSync(process.env.INTERLUDE_OUT??"/tmp/interlude.txt",rows.join("\n")+"\n");
},600000);
