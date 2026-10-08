/* Real Soothe Bell acquisition + singular raw booster oracle; Node22/pin8555c08.
 * Does not call/claim whole Pokemon.addFriendship or Scene acquisition lifecycle. */
import { modifierTypes } from "#data/data-lists";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { PokemonFriendshipBoosterModifier } from "#modifiers/modifier";
import { PokemonData } from "#system/pokemon-data";
import { GameManager } from "#test/framework/game-manager";
import { NumberHolder } from "#utils/common";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it } from "vitest";

it("bounded Soothe Bell primitives producer", async () => {
  const N=Number(process.env.SOOTHE_N ?? 2000), SEED=Number(process.env.SOOTHE_SEED ?? 777);
  expect(process.versions.node.split(".")[0]).toBe("22"); expect(Number.isInteger(N)&&N>=120).toBe(true);
  let seed=SEED; const rnd=(n:number)=>{seed=(Math.imul(seed,1103515245)+12345)>>>0;return(seed>>>8)%n;};
  const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
  game.override.battleStyle("single").startingWave(5).ability(AbilityId.NONE).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene=game.scene as any,party=scene.getPlayerParty(),pool=Array.from({length:6},()=>scene.addPlayerPokemon(party[0].species,5));
  const lines=[`S 1 ${N}`],counts=[0,0,0,0];let wildcard=0,duplicate=0,zero=0,fractional=0,firstOnly=0;
  const rng=()=>Phaser.Math.RND.state().replace(/^!rnd,/,"").split(",").map(Number);
  const create=(name:string)=>{const f=(modifierTypes as any)[name];return f().withIdFromFunc(f);};
  const items=()=>scene.modifiers.map((m:any)=>`${m.type.id} ${m.stackCount} ${m.pokemonId==null||m.pokemonId===-1?-1:party.findIndex((p:any)=>p.id===m.pokemonId)} 0 0 0`);
  const emit=()=>{const a=items();lines.push(`I ${a.length}`,...a.map((s:string)=>`H ${s}`));};
  const gain=(base:number,target:number)=>{const v=new NumberHolder(base);const matched=scene.applyModifier(PokemonFriendshipBoosterModifier,true,party[target],v);return[v.value,scene.modifiers.indexOf(matched)];};
  for(let i=0;i<N;i++){
    party.length=0;scene.modifiers.length=0;const nParty=2+rnd(5),target=rnd(nParty),mode=i%5;
    for(let j=0;j<nParty;j++)party.push(pool[j]);
    const append=(name:string,owner=-1,stack=1)=>{const t=create(name);const m=name==="SOOTHE_BELL"&&owner<0?new PokemonFriendshipBoosterModifier(t,-1,stack):owner<0?t.newModifier():t.newModifier(party[owner]);m.stackCount=stack;scene.modifiers.push(m);};
    append("HEALING_CHARM");append("LEFTOVERS",(target+1)%nParty);
    if(i%3===0)append("SOOTHE_BELL",-1,1+rnd(3));
    if(mode===1||mode===3)append("SOOTHE_BELL",target,3);
    else if(mode===2||mode===4)append("SOOTHE_BELL",target,1+rnd(2));
    if(mode===3||mode===4){append("SOOTHE_BELL",target,1+rnd(2));duplicate++;}
    append("SOOTHE_BELL",(target+1)%nParty,1+rnd(3));
    const base=[0,1,2.5,6,13.75,1000000][Math.floor(i/5)%6];zero+=+(base===0);fractional+=+(base!==Math.floor(base));
    Phaser.Math.RND.sow([`soothe-${SEED}-${i}`]);const r=rng(),battleRng=scene.currentBattle.rngState;
    scene.money=9876;const beforeParty=JSON.stringify(party.map((p:any)=>new PokemonData(p)));
    lines.push(`C ${i} ${nParty} ${target} ${base}`,`P ${party.map((p:any)=>p.id).join(" ")}`,`R ${r.join(" ")}`);emit();
    const before=gain(base,target);lines.push(`G ${before.join(" ")}`);const beforeCount=scene.modifiers.length;
    const added=create("SOOTHE_BELL").newModifier(party[target]).add(scene.modifiers,false);
    const status=!added?1:scene.modifiers.length>beforeCount?2:3;counts[status]++;
    const after=gain(base,target);lines.push(`A ${status} ${after.join(" ")}`);emit();
    wildcard+=+(after[1]>=0&&scene.modifiers[after[1]].pokemonId===-1);
    const all=new NumberHolder(base);scene.applyModifiers(PokemonFriendshipBoosterModifier,true,party[target],all);firstOnly+=+(all.value!==after[0]);
    expect(JSON.stringify(party.map((p:any)=>new PokemonData(p)))).toBe(beforeParty);expect(scene.money).toBe(9876);
    expect(rng()).toEqual(r);expect(scene.currentBattle.rngState).toBe(battleRng);lines.push(`E ${i} ${rng().join(" ")} ${+!!(all.value!==after[0])}`);
  }
  for(const n of [...counts.slice(1),wildcard,duplicate,zero,fractional,firstOnly])expect(n).toBeGreaterThan(0);
  lines.push(`Z ${N} 0 0 ${counts.slice(1).join(" ")} ${wildcard} ${duplicate} ${zero} ${fractional} ${firstOnly}`);
  fs.writeFileSync(process.env.SOOTHE_OUT??"/tmp/soothe-bell.txt",lines.join("\n")+"\n");
},240000);
