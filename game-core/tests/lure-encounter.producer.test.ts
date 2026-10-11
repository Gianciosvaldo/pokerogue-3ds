/* Actual chance + newBattle decision, stopped before Battle constructor. */
import { getGameMode } from "#app/game-mode";
import { activeOverrides } from "#app/overrides";
import { modifierTypes } from "#data/data-lists";
import { GameModes } from "#enums/game-modes";
import { BattleType } from "#enums/battle-type";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { DoubleBattleChanceBoosterModifier } from "#modifiers/modifier";
import { BattleEndPhase } from "#phases/battle-end-phase";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import {expect,it,vi} from "vitest";
it("Lure real encounter input",async()=>{
 const N=Number(process.env.LUREENC_N??2000),SEED=Number(process.env.LUREENC_SEED??777);
 expect(process.versions.node.split(".")[0]).toBe("22");expect(Number.isSafeInteger(N)&&N>=120&&N<=100000).toBe(true);expect(Number.isSafeInteger(SEED)&&SEED>=0&&SEED<=0xffffffff).toBe(true);
 let seed=SEED;const rnd=(n:number)=>{seed=(Math.imul(seed,1103515245)+12345)>>>0;return(seed>>>8)%n;};
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));game.override.battleStyle("single").startingWave(5).ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
 await game.classicMode.startBattle(SpeciesId.BULBASAUR);
 for(const key of ["FIELD_SIZE_OVERRIDE","BATTLE_TYPE_OVERRIDE","RANDOM_TRAINER_OVERRIDE","MYSTERY_ENCOUNTER_RATE_OVERRIDE"])vi.spyOn(activeOverrides,key as any,"get").mockReturnValue(null);
 vi.spyOn(activeOverrides,"DISABLE_STANDARD_TRAINERS_OVERRIDE","get").mockReturnValue(false);
 const scene=game.scene as any,holder=scene.getPlayerParty()[0];holder.passive=false;holder.pauseEvolutions=false;scene.gameMode=getGameMode(GameModes.ENDLESS);scene.currentBattle.gameMode=scene.gameMode;
 vi.spyOn(scene.modifierBar,"updateModifiers").mockImplementation(()=>{});vi.spyOn(scene.phaseManager,"shiftPhase").mockImplementation(()=>{});
 for(const p of scene.getEnemyParty())vi.spyOn(p,"destroy").mockImplementation(()=>{});
 const keys=["LURE","SUPER_LURE","MAX_LURE"],maxes=[10,15,30],waves=[9,10,11,19,20,21,49,50,51],chars="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
 const type=(key:string)=>{const f=(modifierTypes as any)[key];return f().withIdFromFunc(f);};
 const stock=()=>scene.modifiers.map((m:any)=>[m.type.id,m.stackCount,m.getBattleCount?.()??0]);
 const rng=()=>Phaser.Math.RND.state().replace(/^!rnd,/,"").split(",").map(Number);
 const stop={},check=scene.checkIsDouble;let observed:any;
 scene.checkIsDouble=function(props:any){const before=rng();let draws=0,draw:any[]=[],raw=Phaser.Math.RND.rnd;Phaser.Math.RND.rnd=function(){draws++;const v=raw.call(this);draw.push(v);return v;};let decision;try{decision=check.call(this,props);}finally{Phaser.Math.RND.rnd=raw;}observed={before,after:rng(),draws,draw,decision,battleType:props.battleType};throw stop;};
  const mon = (p: any) => [p.id, p.species.speciesId, p.formIndex, p.fusionSpecies?.speciesId ?? -1,
    p.fusionFormIndex ?? 0, p.level, p.exp, p.hp, p.status?.effect ?? 0, p.getNature(),
    p.nature, p.customPokemonData.nature, p.getAbility(true).id, p.getAbility().id,
    p.hasPassive() ? p.getPassiveAbility().id : 0, p.friendship, p.gender, p.abilityIndex,
    p.fusionAbilityIndex ?? -1, +p.shiny, +p.passive, p.variant, p.luck, +p.pauseEvolutions,
    p.teraType, p.getMoveset().length,
    ...Array.from({length:4}, (_,j) => {const m=p.getMoveset()[j];return m?[m.moveId,m.ppUsed,m.ppUp,m.getMovePp()]:[0,0,0,0];}).flat(),
    ...p.ivs, ...p.stats];


 const lines=[`LURE_ENCOUNTER 1 ${N}`];let doubles=0,singles=0,expired=0,gates=0,drawsTotal=0;const counts=[0,0,0,0];
 for(let ci=0;ci<N;ci++){
  scene.phaseManager.clearAllPhases();scene.modifiers.length=0;scene.enemyModifiers.length=0;scene.currentBattle.playerParticipantIds.clear();scene.currentBattle.moneyScattered=0;scene.currentBattle.battleType=BattleType.WILD;scene.currentBattle.successfulRun=false;
  const wave=waves[Math.floor(ci/8)%waves.length],prior=Math.floor(ci/4)%2,n=ci%4,runSeed=Array.from({length:24},()=>chars[rnd(chars.length)]).join("");scene.seed=runSeed;scene.currentBattle.waveIndex=wave-1;scene.currentBattle.double=false;scene.arena.biomeId=0;scene.money=1000.25+ci;
  scene.modifiers.push(type("HEALING_CHARM").newModifier());
  for(let j=0;j<n;j++){const kind=(ci+j)%3,m=type(keys[kind]).newModifier();m.stackCount=1+Math.floor(ci/8)%2;m.battleCount=prior&&ci%3===0&&j===0?1:2+rnd(maxes[kind]-1);scene.modifiers.push(m);}
  const monBefore=mon(holder),before=stock(),wallet=scene.money,hp=holder.hp,friendship=holder.friendship;Phaser.Math.RND.sow([`encounter-control-${SEED}-${ci}`]);const initial=rng();
  if(prior){const phase=new BattleEndPhase(false);vi.spyOn(phase,"end").mockImplementation(()=>{});phase.start();expect(rng()).toEqual(initial);scene.phaseManager.clearAllPhases();}
  const after=stock(),active=scene.modifiers.filter((m:any)=>m instanceof DoubleBattleChanceBoosterModifier).length,denominator=scene.getDoubleBattleChance(wave);expect(rng()).toEqual(initial);counts[active]++;expired+=before.length-after.length;
  observed=undefined;try{scene.newBattle();throw Error("missing stop");}catch(e){if(e!==stop)throw e;}
  expect(observed.battleType).toBe(BattleType.WILD);expect(stock()).toEqual(after);expect(mon(holder)).toEqual(monBefore);expect(scene.money).toBe(wallet);expect(holder.hp).toBe(hp);expect(holder.friendship).toBe(friendship);
  const gated=wave%50===0;expect(observed.draws).toBe(gated||denominator<=1?0:2);if(gated){expect(observed.decision).toBe(false);gates++;}doubles+=+observed.decision;singles+=+!observed.decision;drawsTotal+=observed.draws;
  lines.push(`C ${ci} ${runSeed} ${wave} ${prior} ${scene.money}`,`R ${initial.join(" ")}`,`P ${monBefore.join(" ")}`,`I ${before.length}`);before.forEach(v=>lines.push(`H ${v.join(" ")}`));lines.push(`J ${after.length}`);after.forEach(v=>lines.push(`K ${v.join(" ")}`));lines.push(`A ${active} ${denominator} ${+observed.decision} ${observed.draws}`,`B ${observed.before.join(" ")}`,`D ${observed.draw.length} ${observed.draw.join(" ")}`.trim(),`S ${observed.after.join(" ")}`,`T ${mon(holder).join(" ")}`,`E ${ci}`);
 }
 scene.checkIsDouble=check;lines.push(`Z ${N}`);fs.writeFileSync(process.env.LUREENC_OUT??"/tmp/lure-encounter.txt",lines.join("\n")+"\n");console.log(`LURE_ENCOUNTER cases=${N} counts=${counts.join("/")} doubles=${doubles} singles=${singles} expired=${expired} gates=${gates} rawDraws=${drawsTotal}`);expect(doubles&&singles&&expired&&gates&&drawsTotal).toBeGreaterThan(0);expect(counts.every(x=>x>0)).toBe(true);
},360000);
