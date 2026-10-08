/* Additive game-core money oracle: real factories, MoneyRewardModifier,
 * Scene.addModifier/addMoney and SelectModifierPhase.applyModifier.
 * UI/achievement/phase-end boundaries are intercepted; balance logic is real.
 * GC_MONEY_N=2000 GC_MONEY_SEED=777 GC_MONEY_OUT=/abs/money.txt, Node22. */
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { modifierTypes } from "#data/data-lists";
import { Status } from "#data/status-effect";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { StatusEffect } from "#enums/status-effect";
import { PokemonMove } from "#moves/pokemon-move";
import { MoneyAchv, ModifierAchv } from "#system/achv";
import { SelectModifierPhase } from "#phases/select-modifier-phase";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it } from "vitest";

it("game money application producer", async () => {
  const N = Number(process.env.GC_MONEY_N ?? 2000), SEED = Number(process.env.GC_MONEY_SEED ?? 777);
  expect(process.versions.node.split(".")[0]).toBe("22");
  expect(Number.isInteger(N) && N >= 100).toBe(true);
  let seed = SEED;
  const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
  const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
  game.override.battleStyle("single").startingWave(5).ability(AbilityId.NONE)
    .enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any, party = scene.getPlayerParty();
  const pool = Array.from({ length: 6 }, () => scene.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.BULBASAUR), 5));
  const species = [SpeciesId.BULBASAUR, SpeciesId.IVYSAUR, SpeciesId.PIKACHU, SpeciesId.SHEDINJA, SpeciesId.DEOXYS, SpeciesId.CHARIZARD];
  const mon = (p: any) => [p.id, p.species.speciesId, p.formIndex, p.fusionSpecies?.speciesId ?? -1,
    p.fusionFormIndex ?? 0, p.level, p.exp, p.hp, p.status?.effect ?? 0, p.getNature(),
    p.nature, p.customPokemonData.nature, p.getAbility(true).id, p.getAbility().id,
    p.hasPassive() ? p.getPassiveAbility().id : 0, p.friendship, p.gender, p.abilityIndex,
    p.fusionAbilityIndex ?? -1, +p.shiny, +p.passive, p.variant, p.luck, +p.pauseEvolutions,
    p.teraType, p.getMoveset().length,
    ...Array.from({length:4}, (_,j) => {const m=p.getMoveset()[j];return m?[m.moveId,m.ppUsed,m.ppUp,m.getMovePp()]:[0,0,0,0];}).flat(),
    ...p.ivs, ...p.stats];
  const items = () => scene.modifiers.map((m: any) => [m.type.id, m.stackCount,
    m.pokemonId == null ? -1 : party.findIndex((p: any) => p.id === m.pokemonId),
    m.type.id === "BASE_STAT_BOOSTER" ? m.getArgs()[1] : 0, 0, 1].join(" "));
  const rng = () => Phaser.Math.RND.state().replace(/^!rnd,/, "").split(",").map(Number).join(" ");
  const lines: string[] = [`V 1 ${N}`], names = ["NUGGET", "BIG_NUGGET", "RELIC_GOLD"];
  let capped=0, paid=0, free=0, fractional=0;
  const oldUI=scene.updateMoneyText, oldAnimate=scene.animateMoneyChanged, oldAchv=scene.validateAchvs;
  const oldHandler=scene.ui.getHandler;scene.ui.getHandler=function(...args:any[]){const h=oldHandler.apply(this,args);h.updateCostText=()=>{};return h;};
  scene.updateMoneyText=()=>{}; scene.animateMoneyChanged=()=>{};
  scene.validateAchvs=(type:any)=>{expect([MoneyAchv,ModifierAchv]).toContain(type);};
  for (let i = 0; i < N; i++) {
    party.length = 0; scene.modifiers.length = 0;
    const count = 1 + rnd(6), which = i % 3, wave = i%2 ? 1+rnd(10000) : [1,10,11,100,9999,10000][Math.floor(i/3)%6];
    const cost = Math.floor(i / 3) % 3 === 0 ? -1 : i % 7 === 0 ? 0 : 50 + rnd(500);
    const money = [10000+rnd(10000), 10000+rnd(10000)+0.25, Number.MAX_SAFE_INTEGER, Number.MAX_SAFE_INTEGER-1, Number.MAX_SAFE_INTEGER-100000][Math.floor(i/3)%5];
    for (let j = 0; j < count; j++) {
      const p = pool[j]; p.resetSummonData(); p.resetBattleAndWaveData();
      p.species = speciesDataRegistry.getSpecies(species[rnd(species.length)]);
      p.formIndex = rnd(Math.max(p.species.forms.length, 1));
      p.fusionSpecies = rnd(3) === 0 ? speciesDataRegistry.getSpecies(species[rnd(species.length)]) : null;
      p.fusionFormIndex = p.fusionSpecies ? rnd(Math.max(p.fusionSpecies.forms.length, 1)) : 0;
      p.abilityIndex = 0; p.fusionAbilityIndex = 0; p.passive = rnd(2) === 0;
      p.shiny = rnd(2) === 0; p.fusionShiny = rnd(2) === 0; p.variant = 0; p.fusionVariant = 0;
      p.luck = rnd(4); p.fusionLuck = rnd(4); p.gender = rnd(3) - 1; p.fusionGender = rnd(3) - 1;
      p.level = i % 7 === 0 ? 255 : 1 + rnd(100); p.exp = rnd(100000); p.ivs = Array.from({ length: 6 }, () => rnd(32));
      p.nature = rnd(25); p.customPokemonData.nature = rnd(2) === 0 ? rnd(25) : -1;
      p.teraType = rnd(19); p.friendship = rnd(256); p.pauseEvolutions = rnd(2) === 0;
      p.moveset = [new PokemonMove(MoveId.TACKLE, rnd(4), rnd(4)), new PokemonMove(MoveId.GROWL, rnd(4), rnd(4))];
      party.push(p);
    }
    const add = (name:string, owner=-1, stack=1) => {
      const f=(modifierTypes as any)[name],t=f().withIdFromFunc(f);
      const m=owner<0?t.newModifier():t.newModifier(party[owner]);m.stackCount=stack;scene.modifiers.push(m);
    };
    add("HEALING_CHARM");add("LEFTOVERS",rnd(count));
    for(let j=0;j<Math.floor(i/3)%6;j++)add("AMULET_COIN",-1,1+rnd(5));
    party.forEach((q:any)=>{q.calculateStats();q.hp=rnd(q.getMaxHp()+1);q.status=rnd(3)===0?new Status(StatusEffect.POISON):null;});
    scene.currentBattle.waveIndex=wave;scene.money=money;Phaser.Math.RND.sow([`money-${SEED}-${i}`]);
    const battleRng=scene.currentBattle.rngState;
    lines.push(`C ${i} ${count} ${which} ${wave} ${cost} ${money}`,`R ${rng()}`);
    party.forEach((q:any)=>{const v=mon(q);expect(v).toHaveLength(54);v.forEach(n=>expect(Number.isFinite(n)).toBe(true));lines.push(`P ${v.join(" ")}`);});
    lines.push(`I ${items().length}`,...items().map((v:string)=>`H ${v}`));
    let result=false,error=0,ended=0,amount=-1,credit=-1;
    const originalAdd=scene.addModifier,originalError=scene.ui.playError,originalShift=scene.phaseManager.shiftPhase,originalQueue=scene.phaseManager.unshiftNew,oldMoney=scene.addMoney;
    scene.addModifier=function(...args:any[]){result=originalAdd.apply(this,args);return result;};
    scene.addMoney=function(n:number){const before=this.money;amount=n;oldMoney.call(this,n);credit=this.money-before;};
    scene.ui.playError=()=>{error++;};scene.phaseManager.shiftPhase=()=>{ended++;};
    scene.phaseManager.unshiftNew=(name:string)=>{throw new Error(`unexpected money queue ${name}`);};
    try {const f=(modifierTypes as any)[names[which]],t=f().withIdFromFunc(f);(new SelectModifierPhase() as any).applyModifier(t.newModifier(),cost,false);}
    finally {scene.addModifier=originalAdd;scene.ui.playError=originalError;scene.phaseManager.shiftPhase=originalShift;scene.phaseManager.unshiftNew=originalQueue;scene.addMoney=oldMoney;}
    expect(result).toBe(true);expect(error).toBe(0);expect(ended).toBe(cost<0?1:0);
    lines.push(`A ${+result} ${amount} ${credit} ${error} ${ended} ${scene.money}`);
    party.forEach((q:any)=>lines.push(`T ${mon(q).join(" ")}`));
    lines.push(`J ${items().length}`,...items().map((v:string)=>`K ${v}`),`S ${rng()}`,"E");
    expect(scene.currentBattle.rngState).toBe(battleRng);
    capped+=+(money+credit===Number.MAX_SAFE_INTEGER);paid+=+(cost>=0);free+=+(cost<0);fractional+=+(money!==Math.floor(money));
  }
  scene.updateMoneyText=oldUI;scene.animateMoneyChanged=oldAnimate;scene.validateAchvs=oldAchv;scene.ui.getHandler=oldHandler;
  for(const n of [capped,paid,free,fractional])expect(n).toBeGreaterThan(0);
  lines.push(`Z ${N}`);fs.writeFileSync(process.env.GC_MONEY_OUT??"/tmp/game-money.txt",lines.join("\n")+"\n");
  console.log(`MONEY ${SEED}: ${N} cases capped=${capped} paid=${paid} free=${free} fractional=${fractional}, thrown=0 skipped=0`);
},240000);
