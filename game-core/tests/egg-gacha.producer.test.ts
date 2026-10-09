/* Actual public voucher admission/debit, pullEggs, Egg and EggData; UI delivery bounded. */
import { speciesDataRegistry } from "#app/global-species-data-registry";
import { Egg,getLegendaryGachaSpeciesForTimestamp } from "#data/egg";
import { EggData } from "#system/egg-data";
import { EggGachaUiHandler } from "#ui/handlers/egg-gacha-ui-handler";
import { variantData } from "#sprites/variant";
import { PokemonData } from "#system/pokemon-data";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import i18next from "i18next";
import * as fs from "node:fs";
import { createHash } from "node:crypto";
import { expect,it,vi } from "vitest";
it("actual bounded egg purchase and constructor",async()=>{
 const N=Number(process.env.GC_EGG_N??2000),SEED=Number(process.env.GC_EGG_SEED??777);expect(process.versions.node.split(".")[0]).toBe("22");expect(Number.isInteger(N)&&N>=120&&N<=100000).toBe(true);expect(Number.isInteger(SEED)&&SEED>=0&&SEED<=0xffffffff).toBe(true);
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));await game.classicMode.startBattle(1);const scene=game.scene as any,g=scene.gameData;
 const asset=fs.readFileSync("assets/images/pokemon/variant/_masterlist.json");expect(createHash("sha256").update(asset).digest("hex")).toBe("c776d4ad0eb7fe63f00b76bc1cfc0dde4bf60404195a2a1634ef3b2e3dcf9b50");Object.assign(variantData,JSON.parse(asset.toString()));
 const pool:any[]=[];for(const tier of [0,1,2,3])for(const s of speciesDataRegistry.getSpeciesForEggTier(tier))pool.push([s.speciesId,speciesDataRegistry.getStarterCost(s.speciesId),tier,+s.hasVariants()]);
 let draws=0;let seed=SEED;const rnd=(n:number)=>{seed=(Math.imul(seed,1664525)+1013904223)>>>0;return seed%n;};
 const state=(r:any)=>r.state().replace(/^!rnd,/,"").split(",").map(Number);
 const statNames=["eggsPulled","rareEggsPulled","epicEggsPulled","legendaryEggsPulled","manaphyEggsPulled"];
 const profile=()=>[...Object.values(g.voucherCounts),...g.eggPity,...g.unlockPity,...statNames.map(k=>g.gameStats[k])];
 const fields=(e:any)=>{const d=new EggData(e);expect(JSON.stringify(d)).toBe(JSON.stringify(new EggData(d)));return[d.id,d.hatchWaves,d.tier,d.species,d.sourceType,d.variantTier,+d.isShiny,d.eggMoveIndex,+d.overrideHiddenAbility,d.timestamp];};
 const opaque=()=>JSON.stringify({stats:{...g.gameStats,eggsPulled:0,rareEggsPulled:0,epicEggsPulled:0,legendaryEggsPulled:0,manaphyEggsPulled:0},dex:g.dexData,starter:g.starterData,unlocks:g.unlocks,achv:g.achvUnlocks,voucher:g.voucherUnlocks},(k,v)=>typeof v==="bigint"?v.toString():v);
 const translation=vi.spyOn(i18next,"t");
 const handler:any=Object.create(EggGachaUiHandler.prototype);handler.voucherCountLabels=[];handler.getUi=()=>({revertMode:()=>{}});let error="",display:any[]=[];handler.showError=(key:string)=>{error=key;};handler.pull=async(count:number)=>{display=handler.pullEggs(count);};
 const out=[`GACHA 1 ${N} ${pool.length}`,...pool.map(p=>`M ${p.join(" ")}`)];let success=0,capacity=0,cost=0,eggs=0,shiny=0,variants=0,manaphy=0,legendary=0,featured=0;const machines=[0,0,0],options=[0,0,0,0,0];
 for(let ci=0;ci<N;ci++){
  const machine=ci%3,cursor=Math.floor(ci/3)%5,timestamp=Date.UTC(2026,9,9)+rnd(366)*86400000,beforeN=ci%13===0?98:ci%17===0?99:rnd(7);handler.gachaCursor=machine;error="";display=[];
  vi.spyOn(Date,"now").mockReturnValue(timestamp);
  const entropy=new Phaser.Math.RandomDataGenerator([`egg-entropy-${SEED}-${ci}`]);vi.spyOn(Math,"random").mockImplementation(()=>{draws++;return entropy.frac();});Phaser.Math.RND.sow([`egg-scene-${SEED}-${ci}`]);
  g.eggs=[];for(let j=0;j<beforeN;j++)g.eggs.push(new Egg({id:100+j,tier:0,species:pool[j%pool.length][0],sourceType:0,timestamp,variantTier:0,isShiny:false,eggMoveIndex:0,overrideHiddenAbility:false,hatchWaves:10}));
  for(let j=0;j<4;j++){g.voucherCounts[j]=ci%19===0?0:20+rnd(10);g.eggPity[j]=j===0||ci%4!==0?0:([0,8,58,411][j]+rnd(3));g.unlockPity[j]=ci%5===0?9:rnd(11);}
  const caught=pool.map(p=>{const yes=ci%3===0?1:ci%3===1?0:rnd(2);g.dexData[p[0]].caughtAttr=BigInt(yes);return yes;}).join("");for(const k of statNames)g.gameStats[k]=rnd(30);
  const party=scene.getPlayerParty().map((p:any)=>JSON.stringify(new PokemonData(p))),money=scene.money,other=opaque();const before=g.eggs.map(fields),v=profile(),r=state(Phaser.Math.RND),e=state(entropy),focus=getLegendaryGachaSpeciesForTimestamp(timestamp);expect(state(Phaser.Math.RND)).toEqual(r);
  draws=0;translation.mockClear();const accepted=handler.handleVoucherSelectAction(cursor);expect(typeof accepted).toBe("boolean");const keys=translation.mock.calls.map(args=>args[0]);const result=accepted?0:keys.includes("egg:tooManyEggs")?1:2;if(!accepted)expect(keys.includes(result===1?"egg:tooManyEggs":"egg:notEnoughVouchers")).toBe(true);
  if(!accepted){expect(g.eggs.map(fields)).toEqual(before);expect(profile()).toEqual(v);expect(state(entropy)).toEqual(e);expect(state(Phaser.Math.RND)).toEqual(r);expect(error.length).toBeGreaterThan(0);}
  expect(opaque()).toBe(other);expect(scene.money).toBe(money);expect(scene.getPlayerParty().map((p:any)=>JSON.stringify(new PokemonData(p)))).toEqual(party);
  out.push(`C ${ci} ${machine} ${cursor} ${timestamp} ${result} ${beforeN} ${focus}`,`R ${r.join(" ")}`,`E ${e.join(" ")}`,`V ${v.join(" ")}`,`K ${caught}`,`B ${before.length}`,...before.map(p=>`I ${p.join(" ")}`),`A ${g.eggs.length}`, ...g.eggs.map((p:any)=>`O ${fields(p).join(" ")}`),`W ${profile().join(" ")}`,`S ${state(Phaser.Math.RND).join(" ")}`,`T ${state(entropy).join(" ")}`,`U ${draws}`,`D ${display.length} ${display.map(x=>g.eggs.indexOf(x)-beforeN).join(" ")}`.trimEnd(),`X ${ci}`);
  if(accepted){success++;machines[machine]++;options[cursor]++;for(const egg of display){eggs++;shiny+=+egg.isShiny;variants+=+(egg.variantTier>0);manaphy+=+egg.isManaphyEgg();legendary+=+(egg.tier===3);featured+=+(machine===1&&egg.tier===3&&egg.species===focus);}}else if(result===1)capacity++;else cost++;
 }
 out.push(`Z ${N} ${success} ${capacity} ${cost} ${eggs} ${shiny} ${variants} ${manaphy} ${legendary} ${featured} ${machines.join(" ")} ${options.join(" ")}`);
 if(N>=2000)expect(success&&capacity&&cost&&shiny&&variants&&manaphy&&legendary&&featured).toBeGreaterThan(0);expect(machines.every(n=>n>0)&&options.every(n=>n>0)).toBe(true);fs.writeFileSync(process.env.GC_EGG_OUT!,out.join("\n")+"\n");vi.restoreAllMocks();
},240000);
