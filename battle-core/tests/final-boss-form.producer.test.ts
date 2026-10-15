import { BattleScene } from "#app/battle-scene";
/* Actual controller -> QuietFormChangePhase.start -> PokemonMove.usePp. */
import { audioManager } from "#app/global-audio-manager";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it,vi } from "vitest";
it("actual Eternamax form and PP lifecycle",async()=>{
 const N=Number(process.env.FINAL_BOSS_N??2048),seed=Number(process.env.FINAL_BOSS_SEED??777);
 if(!process.versions.node.startsWith("22.")||!Number.isSafeInteger(N)||N<512)throw Error("Node22,N>=512");
 const realBattleRoll=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).enemySpecies(SpeciesId.ETERNATUS);await g.classicMode.startBattle(SpeciesId.BULBASAUR);BattleScene.prototype.randBattleSeedInt=realBattleRoll;
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE);
 const s:any=g.scene,pm:any=s.phaseManager,p:any=g.field.getEnemyPokemon();
 vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});vi.spyOn(audioManager,"fadeOutBgm").mockImplementation(()=>{});vi.spyOn(s,"setFieldScale").mockImplementation(()=>{});vi.spyOn(s,"updateFieldScale").mockImplementation(async()=>{});
 vi.spyOn(p,"loadAssets").mockImplementation(async()=>{});vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});vi.spyOn(p,"updateSpritePipelineData").mockImplementation(()=>{});
 vi.spyOn(s.ui,"showDialogue").mockImplementation((_t:any,_n:any,_d:any,cb:any)=>cb?.());let texts:string[]=[];vi.spyOn(s.ui,"showText").mockImplementation((t:any,_d:any,cb:any)=>{texts.push(t);cb?.();});
 let rng=seed;const rnd=(n:number)=>{rng=(Math.imul(rng,1103515245)+12345)>>>0;return(rng>>>8)%n;};
 const rows=[`FINALBOSSFORM 1 ${N} ${seed}`],rs=(x:string)=>x.replace(/^!rnd,/,"").replaceAll(","," ");
 for(let i=0;i<N;i++){
  pm.clearAllPhases();s.modifiers=[];s.enemyModifiers=[];s.currentBattle.double=false;s.currentBattle.waveIndex=200;
  p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.formIndex=0;p.level=1+rnd(255);p.nature=rnd(25);p.customPokemonData.nature=-1;p.ivs=Array.from({length:6},()=>rnd(32));p.customPokemonData.ability=-1;p.abilityIndex=0;p.customPokemonData.passive=-1;p.passive=false;p.bossSegments=4;p.bossSegmentIndex=0;p.status=null;p.stats=Array(6).fill(0);p.hp=0;p.calculateStats();p.hp=1+rnd(p.getMaxHp());p.visible=false;p.generateAndPopulateMoveset();p.generateName();
  Phaser.Math.RND.sow([`final-boss-form-${seed}-${i}`]);s.currentBattle.battleSeedState=null;const before=Phaser.Math.RND.state();texts=[];
  rows.push(`C ${i} ${p.level} ${p.nature} ${p.hp}`,`IV ${p.ivs.join(" ")}`,`I ${p.stats.join(" ")}`,`R ${rs(before)}`);
  s.initFinalBossPhaseTwo(p);const forms=pm.phaseQueue.levels.flat().filter((ph:any)=>ph.phaseName==="QuietFormChangePhase");if(forms.length!==1)throw Error("form not queued");pm.currentPhase=forms[0];await forms[0].start();
  rows.push(`O ${p.formIndex} ${p.hp} ${p.abilityIndex} ${p.getAbility().id} ${p.getPassiveAbility().id} ${+p.hasPassive()} ${p.stats.join(" ")}`,`F ${[...p.turnData.formChangeAbilitiesApplied].join(" ")}`,`T ${texts.length===1?Buffer.from(texts[0]).toString("hex"):"-"}`);
  if(texts.length!==1)throw Error("expected Eternamax message");
  for(const m of p.moveset){const count=1+rnd(10);m.usePp(count);rows.push(`M ${count} ${m.moveId} ${m.ppUp} ${m.ppUsed} ${m.getMovePp()}`);}
  rows.push(`RO ${rs(Phaser.Math.RND.state())}`,"E");
 }
 rows.push(`FINALBOSSFORM_COMPLETE ${N}`);fs.writeFileSync(process.env.FINAL_BOSS_OUT??"/tmp/final-boss-form.txt",rows.join("\n")+"\n");
},600000);
