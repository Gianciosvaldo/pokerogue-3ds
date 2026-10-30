import { BattleScene } from "#app/battle-scene";
import { speciesDataRegistry } from "#app/global-species-data-registry";
import { TerastallizeModifierType } from "#modifiers/modifier-type";
import { SpeciesId } from "#enums/species-id";
import { MoveId } from "#enums/move-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { it,expect,vi } from "vitest";
it("native Tera shard filter and consumable owner",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;expect(vi.isMockFunction(native)).toBe(false);
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));game.override.enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
 await game.classicMode.startBattle(SpeciesId.MEW,SpeciesId.MEW);BattleScene.prototype.randBattleSeedInt=native;
 const s:any=game.scene,p:any=s.party[1],other:any=s.party[0];expect(vi.isMockFunction(s.addModifier)).toBe(false);
 for(const mon of s.party)vi.spyOn(mon,"updateInfo").mockResolvedValue(undefined);
 const rng=Phaser.Math.RND.state(),br=s.currentBattle.battleSeedState,rows=["TERA_SHARDS 1 1520"];
 let i=0;
 for(const primary of [SpeciesId.MEW,SpeciesId.SHEDINJA,SpeciesId.OGERPON,SpeciesId.TERAPAGOS])for(const fusion of [0,SpeciesId.MEW,SpeciesId.SHEDINJA,SpeciesId.OGERPON,SpeciesId.TERAPAGOS])for(const faint of [0,1])for(let type=0;type<=18;type++)for(const same of [0,1]){
  p.species=speciesDataRegistry.getSpecies(primary);p.formIndex=0;p.fusionSpecies=fusion?speciesDataRegistry.getSpecies(fusion):null;p.fusionFormIndex=0;p.hp=faint?0:10;p.teraType=same?type:(type+1)%19;other.teraType=2;s.modifiers=[];
  const before=p.teraType,t=new TerastallizeModifierType(type as any),mod:any=t.newModifier(p);
  expect(vi.isMockFunction(mod.shouldApply)).toBe(false);expect(vi.isMockFunction(mod.apply)).toBe(false);
  const filter=t.selectFilter!(p),allowed=mod.shouldApply(p),result=s.addModifier(mod,true,false,false,true);
  rows.push(`C ${i++} ${primary} ${fusion} ${faint} ${type} ${same} ${before} ${+!filter} ${+allowed} ${+result} ${p.teraType} ${other.teraType} ${s.modifiers.length}`);
  expect(Phaser.Math.RND.state()).toBe(rng);expect(s.currentBattle.battleSeedState).toBe(br);
 }
 expect(i).toBe(1520);rows.push("TERA_SHARDS_COMPLETE 1520");fs.writeFileSync(process.env.TERA_SHARDS_OUT!,rows.join("\n")+"\n");
},120000);
