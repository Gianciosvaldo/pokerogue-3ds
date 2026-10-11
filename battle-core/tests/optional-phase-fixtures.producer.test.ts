/* Actual pending phases; genuine -1 comes from an off-field berry holder. */
import { modifierTypes } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { SwitchType } from "#enums/switch-type";
import { BerryType } from "#enums/berry-type";
import { GameManager } from "#test/framework/game-manager";
import { BerryPhase } from "#phases/berry-phase";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { ModifierTypeGenerator } from "#modifiers/modifier-type";
import Phaser from "phaser";
import * as fs from "node:fs";
import {expect,it,vi} from "vitest";
it("optional phase deterministic objects",async()=>{
 expect(process.versions.node.split('.')[0]).toBe('22');const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));game.override.battleStyle("single").ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);await game.classicMode.startBattle(SpeciesId.BULBASAUR);
 const scene=game.scene as any,pm=scene.phaseManager as any,battle=scene.currentBattle,bench=scene.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.BULBASAUR),5);scene.getPlayerParty().push(bench);bench.passive=false;bench.pauseEvolutions=false;
 vi.spyOn(scene.modifierBar,"updateModifiers").mockImplementation(()=>{});vi.spyOn(pm,"shiftPhase").mockImplementation(()=>{});
 const token=(p:any)=>{if(!("battlerIndex" in p))return 'A';const v=p.battlerIndex;if(v===undefined)return 'U';if(v===null)return 'N';if(!Number.isSafeInteger(v))throw Error('fractional/invalid phase metadata');return `I${v}`;};
 const state=()=>Phaser.Math.RND.state().replace(/^!rnd,/,"").split(',').join(' '),lines=['OPTIONAL_FIXTURES 1 6'];
 const queue=()=>{const out:string[]=[];pm.phaseQueue.levels.forEach((lv:any[],i:number)=>lv.forEach((p:any,j:number)=>out.push(`Q L ${i} ${j} ${p.phaseName==='DynamicPhaseMarker'?`Marker:${p.phaseType}`:p.phaseName} ${token(p)}`)));const groups=[...pm.dynamicQueueManager.dynamicPhaseMap].filter(([,q]:any)=>q.queue.length).sort(([a]:any,[b]:any)=>a<b?-1:a>b?1:0);groups.forEach(([,q]:any,i:number)=>q.queue.forEach((p:any,j:number)=>out.push(`Q D ${i} ${j} ${p.phaseName} ${token(p)}`)));return out;};
 let absent=0,indexed=0,negative=0;
 for(let ci=0;ci<6;ci++){
  pm.clearAllPhases();pm.phaseQueue.deferredActive=false;scene.modifiers.length=0;bench.resetSummonData();bench.resetBattleAndWaveData();bench.calculateStats();bench.hp=Math.floor(bench.getMaxHp()/4);bench.battleData.hasEatenBerry=false;scene.money=1000.25+ci;Phaser.Math.RND.sow([`optional-phase-${ci}`]);const rng=state(),hp=bench.hp,stats=bench.stats.slice(),max=bench.getMaxHp(),lead=scene.getPlayerParty()[0].hp,enemy=scene.getEnemyParty()[0].hp,br=(battle as any).rngState;
  lines.push(`C ${ci}`,`R ${rng}`,`P ${bench.id} ${hp} ${bench.stats.join(" ")} ${scene.money}`);
  if(ci===0||ci===4||ci===5)pm.pushNew('SwitchSummonPhase',SwitchType.SWITCH,0,-1,false,false);
  if(ci===1||ci===4)pm.pushNew('PostSummonPhase',0);
  if(ci===2||ci===4)pm.pushNew('PostSummonPhase',2);
  if(ci===3||ci===5){const g=modifierTypes.BERRY() as ModifierTypeGenerator;const type=g.generateType(scene.getPlayerParty(),[BerryType.SITRUS])!;type.id='BERRY';const berry=type.newModifier(bench);scene.modifiers.push(berry);expect(bench.getBattlerIndex()).toBe(-1);expect(berry.shouldApply(bench)).toBe(true);new BerryPhase().eatBerries(bench);expect(bench.battleData.hasEatenBerry).toBe(true);}
  const q=queue();q.forEach(x=>{absent+=+x.endsWith(' A');indexed+=+(/ I[02]$/.test(x));negative+=+x.endsWith(' I-1');});expect(bench.hp).toBe(hp);expect(bench.stats).toEqual(stats);expect(scene.getPlayerParty()[0].hp).toBe(lead);expect(scene.getEnemyParty()[0].hp).toBe(enemy);expect(state()).toBe(rng);expect((battle as any).rngState).toBe(br);
  lines.push(`A ${bench.hp} ${+bench.battleData.hasEatenBerry} ${scene.modifiers.length} ${scene.money}`,`QCOUNT ${q.length} ${pm.phaseQueue.levels.length} ${+pm.phaseQueue.deferredActive}`,...q,`S ${state()}`,`E ${ci}`);
 }
 lines.push('Z 6');fs.writeFileSync(process.env.OPTIONAL_OUT??'/tmp/optional-fixtures.txt',lines.join('\n')+'\n');console.log(`OPTIONAL_FIXTURES complete6 absent${absent} indexed${indexed} genuineMinusOne${negative}`);expect(absent&&indexed&&negative).toBeGreaterThan(0);
},120000);
