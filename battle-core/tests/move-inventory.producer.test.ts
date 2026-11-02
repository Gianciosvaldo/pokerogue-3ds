import { BattleScene } from "#app/battle-scene";
import { allMoves } from "#data/data-lists";
import { SpeciesId } from "#enums/species-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { it,expect,vi } from "vitest";
it('capture registered move attribute source inventory',async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;expect(vi.isMockFunction(native)).toBe(false);
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));game.override.enemySpecies(SpeciesId.MAGIKARP);await game.classicMode.startBattle(SpeciesId.BULBASAUR);BattleScene.prototype.randBattleSeedInt=native;
 const capture=(attr:any)=>{const chain:string[]=[];for(let p=Object.getPrototypeOf(attr);p&&p!==Object.prototype;p=Object.getPrototypeOf(p))chain.push(p.constructor.name);expect(chain.length).toBeGreaterThan(0);return chain;};
 const moves=allMoves.map((m:any)=>({id:m.id,unimplemented:m.isUnimplemented,attrs:m.attrs.map(capture),chargeAttrs:(m.chargeAttrs??[]).map(capture)}));
 expect(moves.length).toBe(921);expect(moves.every(m=>typeof m.unimplemented==="boolean")).toBe(true);expect(moves.some(m=>!m.unimplemented)).toBe(true);expect(moves.some(m=>m.unimplemented)).toBe(true);fs.writeFileSync(process.env.MOVE_INVENTORY_OUT!,JSON.stringify({version:1,sourcePin:'8555c08c823b856cbec4eb99ca84ea52a955836d',moves},null,2)+'\n');
},120000);
