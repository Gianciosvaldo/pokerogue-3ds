import { BattleScene } from "#app/battle-scene";
import { speciesDataRegistry as registry } from "#app/globals/global-species-data-registry";
import { audioManager } from "#app/global-audio-manager";
import { allMoves } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { LearnMoveType } from "#enums/learn-move-type";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { UiMode } from "#enums/ui-mode";
import { PokemonMove } from "#moves/pokemon-move";
import { LearnMovePhase } from "#phases/learn-move-phase";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import {expect,it,vi} from "vitest";
vi.mock("#data/battle-anims",async importOriginal=>({...await importOriginal<any>(),initMoveAnim:async()=>{},loadMoveAnimAssets:()=>{}}));
it("native complete TM history at the actual learn callback",async()=>{
 expect(Number(process.versions.node.split(".")[0])).toBe(22);
 const native=BattleScene.prototype.randBattleSeedInt,learn=(LearnMovePhase.prototype as any).learnMove;expect(vi.isMockFunction(native)).toBe(false);expect(vi.isMockFunction(learn)).toBe(false);
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));game.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE).moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).enemySpecies(SpeciesId.MAGIKARP).disableTrainerWaves().mysteryEncounterChance(0);
 await game.classicMode.startBattle(SpeciesId.MEW);BattleScene.prototype.randBattleSeedInt=native;expect((LearnMovePhase.prototype as any).learnMove).toBe(learn);
 const s:any=game.scene,p:any=s.getPlayerParty()[0];vi.spyOn(s.ui,"setMode").mockResolvedValue(undefined);vi.spyOn(s.ui,"showText").mockImplementation(()=>{});vi.spyOn(audioManager,"playSound").mockImplementation(()=>{});
 p.usedTMs=[];p.moveset=[MoveId.SPLASH,MoveId.TACKLE,MoveId.POUND,MoveId.GROWL].map(m=>new PokemonMove(m));
 const available=p.getCompatibleTms(),implemented=available.map((m:number)=>!allMoves[m].isUnimplemented),selected=available.filter((_m:number,i:number)=>implemented[i]);expect(selected.length).toBeGreaterThan(64);
 const catalog=new Set<number>();for(const sp of registry.getAllSpecies()){for(const f of sp.forms.length?sp.forms:[sp])for(const move of sp.getTms(f.formKey))catalog.add(move);}
 const frame=()=>({used:[...p.usedTMs],moves:p.moveset.map((m:any)=>({move:m.moveId,used:m.ppUsed,up:m.ppUp,max:m.maxPpOverride??-1})),memory:p.getLearnableLevelMoves(),global:Phaser.Math.RND.state(),battle:s.currentBattle.battleSeedState});
 Phaser.Math.RND.sow(["tm-history-native"]);s.currentBattle.battleSeedState=null;s.currentBattle.randSeedInt(2);const initial=frame(),rows:any[]=[];
 for(const move of [...selected,selected[0]]){const before=frame();const phase:any=new LearnMovePhase(0,move,LearnMoveType.TM);phase.messageMode=UiMode.MESSAGE;await phase.learnMove(0,allMoves[move],p);rows.push({move,before,after:frame()});}
 fs.writeFileSync(process.env.TM_HISTORY_OUT!,JSON.stringify({version:1,scope:"native LearnMovePhase.learnMove TM callback before text acknowledgement; animation/audio/UI presentation bypassed",available,implemented,catalog:[...catalog].sort((a,b)=>a-b),level:p.level,species:p.species.speciesId,initial,rows})+"\n");
},240000);
