/* Canonical Wide Lens PersistentModifier.add oracle, pin8555c08/Node22.
 * Deliberately excludes Scene acquisition, effects and virtual additions. */
import { modifierTypes } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { PokemonMoveAccuracyBoosterModifier } from "#modifiers/modifier";
import { PokemonData } from "#system/pokemon-data";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it } from "vitest";

it("canonical Wide Lens acquisition producer", async () => {
  const N=Number(process.env.WIDE_HELD_N??2000),SEED=Number(process.env.WIDE_HELD_SEED??777);
  expect(process.versions.node.split(".")[0]).toBe("22");expect(Number.isInteger(N)&&N>=120).toBe(true);
  const FAMILY="WIDE_LENS",CAP=3;
  let seed=SEED;const rnd=(n:number)=>{seed=(Math.imul(seed,1103515245)+12345)>>>0;return(seed>>>8)%n;};
  const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
  game.override.battleStyle("single").startingWave(5).ability(AbilityId.NONE).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene=game.scene as any,party=scene.getPlayerParty(),pool=Array.from({length:6},()=>scene.addPlayerPokemon(party[0].species,5));
  const lines=[`S 1 ${N}`],counts=[0,0,0,0];let wild=0,duplicate=0,reordered=0,fainted=0;
  const rng=()=>Phaser.Math.RND.state().replace(/^!rnd,/,"").split(",").map(Number);
  const type=(name:string)=>{const f=(modifierTypes as any)[name];return f().withIdFromFunc(f);};
  const emit=()=>{lines.push(`I ${scene.modifiers.length}`);for(const m of scene.modifiers){
    const owner=m.pokemonId==null||m.pokemonId===-1?-1:party.findIndex((p:any)=>p.id===m.pokemonId);
    if(m.type.id===FAMILY){expect(m).toBeInstanceOf(PokemonMoveAccuracyBoosterModifier);expect(m.getArgs()[1]).toBe(5);expect(m.getMaxHeldItemCount(party[0])).toBe(CAP);}
    expect(m.virtualStackCount).toBe(0);lines.push(`H ${m.type.id} ${m.stackCount} ${owner} 0 0 ${+(m.active??false)}`);
  }};
  for(let i=0;i<N;i++){
    party.length=0;scene.modifiers.length=0;const nParty=2+rnd(5),target=rnd(nParty),mode=i%5;
    const rotated=i%2===0;for(let j=0;j<nParty;j++)party.push(pool[(j+(rotated?1:0))%6]);reordered+=+rotated;
    for(const p of party)p.hp=rnd(2)?p.getMaxHp():0;fainted+=+(party[target].hp===0);
    const append=(name:string,owner=-1,stack=1)=>{const t=type(name),m=owner<0&&name===FAMILY?t.newModifier(party[0]):owner<0?t.newModifier():t.newModifier(party[owner]);
      if(name===FAMILY&&owner<0)m.pokemonId=-1;m.stackCount=stack;m.active=!!rnd(2);scene.modifiers.push(m);};
    append("HEALING_CHARM");append("LEFTOVERS",(target+1)%nParty);
    if(i%3===0){append(FAMILY,-1,1+rnd(CAP));wild++;}
    if(mode===1||mode===3)append(FAMILY,target,CAP);
    else if(mode===2||mode===4)append(FAMILY,target,1+rnd(CAP));
    if(mode===3||mode===4){append(FAMILY,target,1);duplicate++;}
    append(FAMILY,(target+1)%nParty,1+rnd(CAP));
    Phaser.Math.RND.sow([`wide-held-${SEED}-${i}`]);const r=rng(),battleRng=scene.currentBattle.rngState;
    scene.money=9876;const beforeParty=JSON.stringify(party.map((p:any)=>new PokemonData(p)));
    lines.push(`C ${i} ${nParty} ${target} ${+rotated} ${+(party[target].hp===0)}`,`P ${party.map((p:any)=>p.id).join(" ")}`,`R ${r.join(" ")}`);emit();
    const beforeCount=scene.modifiers.length,incoming=type(FAMILY).newModifier(party[target]);
    expect(incoming.stackCount).toBe(1);expect(incoming.virtualStackCount).toBe(0);
    const added=incoming.add(scene.modifiers,false),status=!added?1:scene.modifiers.length>beforeCount?2:3;
    counts[status]++;lines.push(`A ${status}`);emit();
    expect(JSON.stringify(party.map((p:any)=>new PokemonData(p)))).toBe(beforeParty);expect(scene.money).toBe(9876);
    expect(rng()).toEqual(r);expect(scene.currentBattle.rngState).toBe(battleRng);lines.push(`E ${i} ${rng().join(" ")}`);
  }
  expect(counts[1]).toBeGreaterThan(0);expect(counts[2]).toBeGreaterThan(0);expect(counts[3]).toBeGreaterThan(0);
  for(const n of [wild,duplicate,reordered,fainted])expect(n).toBeGreaterThan(0);
  lines.push(`Z ${N} 0 0 ${counts.slice(1).join(" ")} ${wild} ${duplicate} ${reordered} ${fainted}`);
  fs.writeFileSync(process.env.WIDE_HELD_OUT??"/tmp/wide-held.txt",lines.join("\n")+"\n");
},240000);
