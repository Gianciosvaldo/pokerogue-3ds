import { WishAttr } from "#data/moves/move";
import i18next from "i18next";
/* Actual generateName/getNameToRender/message affixes, with owning Transform phase. */
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { getPokemonNameWithAffix } from "#app/messages";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it,vi } from "vitest";
it("actual native fusion message identity",async()=>{
 const N=Number(process.env.FUSIONMSG_N??8192),seed=Number(process.env.FUSIONMSG_SEED??777);if(!Number.isSafeInteger(N)||N<1024||!process.versions.node.startsWith("22."))throw Error("Node22,N>=1024");
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).enemySpecies(SpeciesId.MAGIKARP);await g.classicMode.startBattle(SpeciesId.BULBASAUR);
 g.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset([]).enemyMoveset([]);
 const s:any=g.scene,pm:any=s.phaseManager,ps:any[]=[g.field.getPlayerPokemon(),g.field.getEnemyPokemon()];vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});vi.spyOn(s,"updateFieldScale").mockImplementation(async()=>{});
 for(const p of ps){vi.spyOn(p,"loadAssets").mockImplementation(async()=>{});vi.spyOn(p,"updateInfo").mockImplementation(async()=>{});vi.spyOn(p,"playAnim").mockImplementation(()=>{});vi.spyOn(p,"updateSpritePipelineData").mockImplementation(()=>{});}
 const wishes=[`FUSIONWISH 1 ${N} ${seed}`];
 const all=Object.keys(speciesDataRegistry.data).map(Number).flatMap(id=>Array.from({length:Math.max(1,speciesDataRegistry.getSpecies(id).forms.length)},(_,k)=>[id,k]));
 let r=seed;const rnd=(n:number)=>{r=(Math.imul(r,1103515245)+12345)>>>0;return(r>>>8)%n;},hex=(v:string)=>Buffer.from(v).toString("hex"),rows=[`FUSIONMSG 1 ${N} ${seed}`];
 for(let i=0;i<N;i++){
  const p=ps[i%2],other=ps[1-i%2],mode=Math.floor(i/2)%4;
  for(const q of ps){q.resetSummonData();q.resetTurnData();q.resetBattleAndWaveData();const [id,form]=all[rnd(all.length)],[fid,ff]=all[rnd(all.length)];q.species=speciesDataRegistry.getSpecies(id);q.formIndex=form;q.fusionSpecies=speciesDataRegistry.getSpecies(fid);q.fusionFormIndex=ff;q.fusionAbilityIndex=0;q.abilityIndex=0;q.customPokemonData.ability=AbilityId.NONE;q.fusionCustomPokemonData=null;q.nickname=null;q.passive=false;q.customPokemonData.passive=AbilityId.NONE;q.moveset=[];q.calculateStats();q.hp=q.getMaxHp();q.generateName();}
  if(mode===0)p.fusionSpecies=null;p.generateName();s.currentBattle.waveIndex=Math.floor(i/8)%2?200:1;ps[1].trainerSlot=Math.floor(i/16)%2?1:0;
  if(mode===2)p.setIllusion(other);
  if(mode===3){pm.clearAllPhases();pm.phaseQueue.deferredActive=false;const ph=pm.create("PokemonTransformPhase",p.getBattlerIndex(),other.getBattlerIndex(),false);pm.currentPhase=ph;ph.start();await Promise.resolve();await Promise.resolve();await Promise.resolve();if(!p.isTransformed())throw Error("Transform refused");}
  const ill=p.summonData.illusion;
  pm.clearAllPhases();pm.phaseQueue.deferredActive=false;s.arena.positionalTagManager.tags=[];
  const initialRng=Phaser.Math.RND.state(),maxHp=p.getMaxHp();new WishAttr().apply(p,p);
  const wish=s.arena.positionalTagManager.tags[0];if(!wish)throw Error("Wish missing");const frozen=wish.pokemonName;
  p.name="Magikarp";if(ill)ill.name="Magikarp";
  for(let k=0;k<2;k++){const ph=pm.create("PositionalTagPhase");pm.currentPhase=ph;ph.start();}
  const heals=pm.phaseQueue.levels.flat().filter((ph:any)=>ph.phaseName==="PokemonHealPhase");if(heals.length!==1)throw Error("Wish did not queue");
  if(initialRng!==Phaser.Math.RND.state())throw Error("Wish changed RNG");
  wishes.push(`W ${i} ${p.species.speciesId} ${p.formIndex} ${p.fusionSpecies?.speciesId??0} ${p.fusionFormIndex} ${+p.isPlayer()} ${+p.hasTrainer()} ${s.currentBattle.waveIndex} ${+!!ill} ${ill?.species??0} ${ill?.formIndex??0} ${ill?.fusionSpecies?.speciesId??0} ${ill?.fusionFormIndex??0} ${+p.isTransformed()} ${maxHp} ${heals[0].hpHealed} ${hex(frozen)} ${hex(heals[0].message)}`);
  p.generateName();if(ill)ill.name=getPokemonNameWithAffix(other);


 }
 wishes.push(`FUSIONWISH_COMPLETE ${N}`);fs.writeFileSync(process.env.FUSIONWISH_OUT??"/tmp/fusion-wish.txt",wishes.join("\n")+"\n");
},600000);
