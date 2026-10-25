/* Actual BattleEndPhase queue plan across terminal waves, modes and mixed nested duplicates. */
import { BattleScene } from "#app/battle-scene";
import { getGameMode } from "#app/game-mode";
import { modifierTypes } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { BattleEndPhase } from "#phases/battle-end-phase";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { expect,it,vi } from "vitest";
it('native BattleEndPhase terminal queue',async()=>{
 const N=2560,seed=Number(process.env.ENDLESS_TERMINAL_SEED??777),native=BattleScene.prototype.randBattleSeedInt;
 expect(process.versions.node.split('.')[0]).toBe('22');
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));game.override.battleStyle('single').ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
 await game.classicMode.startBattle(SpeciesId.BULBASAUR);BattleScene.prototype.randBattleSeedInt=native;
 const scene:any=game.scene,battle:any=scene.currentBattle,pm:any=scene.phaseManager;
 vi.spyOn(BattleEndPhase.prototype,'end').mockImplementation(()=>{});for(const p of scene.getEnemyParty())p.destroy=()=>{};
 const fd=fs.openSync(process.env.ENDLESS_TERMINAL_OUT??'/tmp/endless-terminal.txt','w'),write=(s:string)=>fs.writeSync(fd,s+'\n'),rng=(s:string)=>s.replace(/^!rnd,/,"").replaceAll(',', ' ');
 const prefix=!!process.env.ENDLESS_TERMINAL_PREFIX;write(`ENDLESS_TERMINAL ${prefix?2:1} ${N} ${seed}`);let cleared=0,rewards=0,promoted=0;
 for(let i=0;i<N;i++){
  let j=i;const mode=j%5;j=Math.floor(j/5);const wave=[200,5849,5850,5851][j%4];j=Math.floor(j/4);const victory=j%2;j=Math.floor(j/2);const success=j%2;j=Math.floor(j/2);const faints=j%2;j=Math.floor(j/2);const dup=j%4;j=Math.floor(j/4);const preselect=j%2;j=Math.floor(j/2);const depth=j%2;
  scene.gameMode=getGameMode(mode);battle.gameMode=scene.gameMode;battle.waveIndex=wave;battle.moneyScattered=0;battle.successfulRun=!!success;battle.enemyFaints=faints;scene.modifiers.length=0;scene.enemyModifiers.length=0;
  pm.clearAllPhases();pm.pushNew('NewBattlePhase');pm.pushNew('TrainerVictoryPhase');pm.pushNew('ModifierRewardPhase',modifierTypes.EXP_CHARM);
  if(dup)pm.pushNew('BattleEndPhase',false);
  pm.unshiftNew('SelectBiomePhase');if(dup===2)pm.unshiftNew('BattleEndPhase',true);if(dup===3){pm.phaseQueue.currentLevel=1;pm.unshiftNew('BattleEndPhase',true);pm.phaseQueue.currentLevel=0;}
  if(preselect)pm.pushNew('SelectModifierPhase');pm.phaseQueue.currentLevel=depth;
  const phase=new BattleEndPhase(!!victory);pm.currentPhase=phase;
  let observer:(()=>void)|undefined;const allowed=scene.getPokemonAllowedInBattle;
  scene.getPokemonAllowedInBattle=function(){const value=allowed.call(this);if(observer){const fn=observer;observer=undefined;fn();}return value;};
  battle.battleSeed=`endless-terminal-${seed}-${i}`;battle.battleSeedState=null;Phaser.Math.RND.sow([battle.battleSeed]);battle.randSeedInt(2);
  write(`C ${i} ${mode} ${wave} ${victory} ${success} ${faints} ${dup} ${preselect} ${depth}`);write(`R ${rng(Phaser.Math.RND.state())}`);write(`B ${rng(battle.battleSeedState)}`);
  if(prefix)observer=()=>{const rows:string[]=[];pm.phaseQueue.levels.forEach((lv:any[],l:number)=>lv.forEach((p:any,k:number)=>{const flag=p.phaseName==='GameOverPhase'?+p.isVictory:p.phaseName==='SelectModifierPhase'?+(p.customModifierSettings?.rerollMultiplier===-1):0;rows.push(`AP ${l} ${k} ${p.phaseName} ${flag}`);}));write(`A ${+phase.isVictory} ${pm.phaseQueue.levels.length} ${pm.phaseQueue.currentLevel} ${rows.length}`);for(const row of rows)write(row);};
  phase.start();if(observer)throw Error('missing owning callback boundary');scene.getPokemonAllowedInBattle=allowed;const rows:string[]=[];
  pm.phaseQueue.levels.forEach((lv:any[],l:number)=>lv.forEach((p:any,k:number)=>{const flag=p.phaseName==='GameOverPhase'?+p.isVictory:p.phaseName==='SelectModifierPhase'?+(p.customModifierSettings?.rerollMultiplier===-1):0;rows.push(`P ${l} ${k} ${p.phaseName} ${flag}`);rewards+=+(p.phaseName==='SelectModifierPhase');}));
  write(`O ${+phase.isVictory} ${pm.phaseQueue.levels.length} ${pm.phaseQueue.currentLevel} ${rows.length}`);for(const row of rows)write(row);
  write(`RO ${rng(Phaser.Math.RND.state())}`);write(`BO ${rng(battle.battleSeedState)}`);write('E');cleared+=+(!!scene.gameMode.isEndless&&wave>=5850);promoted+=+(!victory&&phase.isVictory);
 }
 expect(cleared).toBe(512);expect(promoted).toBeGreaterThan(100);expect(rewards).toBeGreaterThan(100);
 write(`ENDLESS_TERMINAL_COMPLETE ${N}`);fs.closeSync(fd);console.log(`ENDLESS_TERMINAL cleared${cleared} promoted${promoted} rewards${rewards}`);
},240000);
