/* Actual source selection methods and CommandPhase caller, with controlled histories. */
import { AbilityId } from "#enums/ability-id";
import { Command } from "#enums/command";
import { MoveId } from "#enums/move-id";
import { MoveUseMode } from "#enums/move-use-mode";
import { MoveResult } from "#enums/move-result";
import { SpeciesId } from "#enums/species-id";
import { PokemonMove } from "#moves/pokemon-move";
import { CommandPhase } from "#phases/command-phase";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { it } from "vitest";

it("consecutive move selection and actual fight caller", async()=>{
    if(!process.versions.node.startsWith("22.")||!process.env.HISTORY_OUT)throw new Error("Node22/HISTORY_OUT required");
    const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
    game.override.ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).enemySpecies(SpeciesId.RATTATA);
    await game.classicMode.startBattle(SpeciesId.RATTATA);
    const scene=game.scene as any,p=game.field.getPlayerPokemon() as any,battle=scene.currentBattle;
    const hex=(v:string)=>Buffer.from(v).toString("hex")||"-";
    const rows:string[]=[],counts=Array(14).fill(0);
    for(const move of [MoveId.GIGATON_HAMMER,MoveId.BLOOD_MOON])for(let state=0;state<14;state++)for(let mode=1;mode<=6;mode++)for(let control=0;control<4;control++){
        scene.phaseManager.clearAllPhases();battle.turnCommands={};battle.preTurnCommands={};
        p.resetSummonData();p.summonData.ability=AbilityId.BALL_FETCH;
        const entry=(m:number,result=MoveResult.SUCCESS,useMode=MoveUseMode.NORMAL)=>({move:m,result,useMode,targets:[2]});
        const selected=entry(move,state===5?MoveResult.FAIL:MoveResult.SUCCESS,state===6?mode:MoveUseMode.NORMAL);
        let hist:any[]=[];
        if([1,5,6].includes(state))hist=[selected];
        if(state===2)hist=[entry(MoveId.TACKLE)];
        if(state===3)hist=[selected,entry(MoveId.TACKLE)];
        if(state===4)hist=[entry(MoveId.TACKLE),selected];
        if(state===7)hist=[selected,entry(MoveId.NONE,MoveResult.OTHER)];
        if(state===8)hist=[selected,entry(MoveId.STRUGGLE)];
        if(state===9)hist=[selected,entry(MoveId.TACKLE,MoveResult.FAIL)];
        if(state===10)hist=[entry(move,MoveResult.FAIL),entry(MoveId.NONE,MoveResult.OTHER)];
        if(state===11||state===12){hist=Array.from({length:33},(_,i)=>entry(i%2?MoveId.TACKLE:move));if(state===12)hist[32]=entry(MoveId.TACKLE);}
        if(state===13)hist=[entry(move===MoveId.BLOOD_MOON?MoveId.GIGATON_HAMMER:MoveId.BLOOD_MOON)];
        p.summonData.moveHistory=hist;
        p.moveset=[new PokemonMove(move,control===1||control===3?99:0)];
        if(control!==2)p.moveset.push(new PokemonMove(MoveId.TACKLE,control===3?99:0));
        const ignore=control===3;
        const before=JSON.stringify({history:hist,moves:p.moveset});
        const rngBefore=JSON.stringify([Phaser.Math.RND.state(),battle.battleSeedState]);
        const selectable=p.isMoveSelectable(move),usable=p.trySelectMove(0,ignore),runtime=p.moveset[0].isUsable(p,ignore,false);
        let message="";scene.ui.setMode=()=>{};scene.ui.showText=(text:string)=>{message=text;};
        const phase=new CommandPhase(0) as any;
        const accepted=phase.handleFightCommand(Command.FIGHT,0,ignore?MoveUseMode.IGNORE_PP:MoveUseMode.NORMAL);
        const fight=accepted?battle.turnCommands[0].move.move:-1;
        if(before!==JSON.stringify({history:p.summonData.moveHistory,moves:p.moveset}))throw new Error("selection mutated history/PP");
        if(rngBefore!==JSON.stringify([Phaser.Math.RND.state(),battle.battleSeedState]))throw new Error("selection advanced RNG");
        const history=hist.map(h=>`${h.move},${h.useMode},${h.result}`).join(";")||"-";
        rows.push(`C ${rows.length} ${move} ${control} ${hist.length} ${history} ${+selectable[0]} ${hex(selectable[1])} ${+usable[0]} ${hex(usable[1])} ${+runtime[0]} ${hex(runtime[1])} ${fight} ${hex(message)}`);counts[state]++;
    }
    if(counts.some(n=>!n)||rows.length!==672)throw new Error("incomplete source coverage");
    fs.writeFileSync(process.env.HISTORY_OUT,`HISTORY_CONTRACT 1 ${rows.length}\n`+rows.join("\n")+`\nHISTORY_COMPLETE ${rows.length}\n`);
    console.log(`HISTORY cases=${rows.length} states=${counts.join("/")} actual CommandPhase/trySelectMove/isUsable; immutable history/PP`);
});
