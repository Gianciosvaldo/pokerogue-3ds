/* Actual pinned name function, including native form prefixes and suffixes. */
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { getFusedSpeciesName } from "#utils/pokemon-utils";
import { AbilityId } from "#enums/ability-id";
import { SpeciesId } from "#enums/species-id";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { it } from "vitest";
it("actual native fusion names",async()=>{
 const N=Number(process.env.FUSENAME_N??8192),SEED=Number(process.env.FUSENAME_SEED??777);if(!Number.isSafeInteger(N)||N<1024||!process.versions.node.startsWith("22."))throw Error("Node22,N>=1024");
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));g.override.ability(AbilityId.BALL_FETCH).enemyAbility(AbilityId.BALL_FETCH).enemySpecies(SpeciesId.MAGIKARP);await g.classicMode.startBattle(SpeciesId.BULBASAUR);
 const names=Object.keys(speciesDataRegistry.data).map(Number).flatMap(id=>{const p=speciesDataRegistry.getSpecies(id);return Array.from({length:Math.max(1,p.forms.length)},(_,k)=>p.getName(k));});
 const edges=["Rapidash","King","Slaking","Ho-Oh","Mr. Mime","Mime Jr.","Type: Null","Nidoran♀","Nidoran♂","Flabébé","Farfetch’d","Sirfetch’d","Mega Charizard X","Mega Charizard Y","Iron Valiant","Great Tusk","Alolan Vulpix","Galarian Mr. Mime"];
 let seed=SEED;const rnd=(n:number)=>{seed=(Math.imul(seed,1103515245)+12345)>>>0;return(seed>>>8)%n;},hex=(s:string)=>Buffer.from(s).toString("hex");const rows=[`FUSENAME 1 ${N} ${SEED}`];
 const before=Phaser.Math.RND.state();
 for(let i=0;i<N;i++){const a=i<edges.length*edges.length?edges[Math.floor(i/edges.length)]:names[rnd(names.length)],b=i<edges.length*edges.length?edges[i%edges.length]:names[rnd(names.length)];rows.push(`C ${i} ${hex(a)} ${hex(b)} ${hex(getFusedSpeciesName(a,b))}`);}
 if(before!==Phaser.Math.RND.state())throw Error("name changed RNG");rows.push(`FUSENAME_COMPLETE ${N}`);fs.writeFileSync(process.env.FUSENAME_OUT??"/tmp/fusion-names.txt",rows.join("\n")+"\n");
},600000);
