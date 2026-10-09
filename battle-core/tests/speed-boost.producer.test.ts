/* Additive Speed Boost proof: actual pinned TurnEndPhase/ability/stat phase.
 * Source methods are observed with delegation, never replaced by formulas. Node22.
 * SPEEDBOOST_N=2000 SPEEDBOOST_SEED=777 SPEEDBOOST_OUT=<trace>.
 */
import { BattleScene } from "#app/battle-scene";
import { settings } from "#app/global-settings-manager";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { allAbilities, allMoves, modifierTypes } from "#data/data-lists";
import { EntryHazardTag } from "#data/arena-tag";
import { Status } from "#data/status-effect";
import { AbilityId } from "#enums/ability-id";
import { BattleType } from "#enums/battle-type";
import { Command } from "#enums/command";
import { MoveCategory } from "#enums/move-category";
import { BattlerTagType } from "#enums/battler-tag-type";
import { MoveId } from "#enums/move-id";
import { MoveUseMode } from "#enums/move-use-mode";
import { SpeciesId } from "#enums/species-id";
import { Stat } from "#enums/stat";
import { StatusEffect } from "#enums/status-effect";
import { TrainerSlot } from "#enums/trainer-slot";
import { UiMode } from "#enums/ui-mode";
import { SpeedBoostAbAttr } from "#abilities/ab-attrs";
import { Weather } from "#data/weather";
import { WeatherType } from "#enums/weather-type";

import { getMoveTargets } from "#moves/move-utils";
import { Pokemon } from "#field/pokemon";
import { PokemonMove } from "#moves/pokemon-move";
import { GameManager } from "#test/framework/game-manager";
import { PartyOption } from "#ui/party-ui-handler";
import i18next from "i18next";
import * as fs from "node:fs";
import Phaser from "phaser";
import { it } from "vitest";
it("bounded Speed Boost move turn producer", async () => {
    for (const [key, value] of Object.entries(process.env))
        if (key.startsWith("TURN_") && value === "1")
            throw new Error(`unapproved mode ${key}`);
    if (!process.versions.node.startsWith("22.")) throw new Error("Node 22 required");
    const N = Number(process.env.SPEEDBOOST_N ?? 2000);
    if (!Number.isSafeInteger(N) || N < 20) throw new Error("SPEEDBOOST_N must be an integer >=20");
    const FUSION = false;
    const FORMS = false;
    const OUT = process.env.SPEEDBOOST_OUT ?? "/tmp/pr-speed-boost.txt";
    (Pokemon.prototype as any).hideInfo = async function () { };
    (Phaser.GameObjects.Sprite.prototype as any).setPosition = function () { return this; };
    const game = new GameManager(new Phaser.Game({ type: Phaser.HEADLESS }));
    game.override
        .battleStyle("single")
        .ability(AbilityId.BALL_FETCH)
        .enemyAbility(AbilityId.BALL_FETCH)
        .battleType(BattleType.WILD)
        .startingWave(3)
        .enemySpecies(SpeciesId.MAGIKARP);
    await game.classicMode.startBattle(SpeciesId.BULBASAUR);
    (BattleScene.prototype as any).randBattleSeedInt = function (this: any, range: number, min = 0) {
        return this.currentBattle?.randSeedInt(range, min);
    };
    const scene = game.scene as any;
    const battle = scene.currentBattle as any;
    const pm = scene.phaseManager as any;
    const A = game.field.getPlayerPokemon() as any;
    const D = game.field.getEnemyPokemon() as any;
    let seed = Number(process.env.SPEEDBOOST_SEED ?? 12345);
    const rnd = (n: number) => {
        seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
        return (seed >>> 8) % n;
    };
    const ids = Object.keys(speciesDataRegistry.data).map(Number);
    const usable = allMoves.filter((m: any) => m && m.id !== MoveId.NONE && !m.isUnimplemented);
    const attacks = usable.filter((m: any) => m.category !== MoveCategory.STATUS);
    const boostTags = [BattlerTagType.PROTOSYNTHESIS, BattlerTagType.QUARK_DRIVE];
    const healStatusAttrs: [
        number,
        any
    ][] = allAbilities
        .filter((a: any) => a)
        .sort((a: any, b: any) => a.id - b.id)
        .flatMap((a: any) => a.attrs.filter((x: any) => x.constructor.name === "PostSummonHealStatusAbAttr").map((x: any) => [a.id, x]));
    const benchPool = (player: boolean) => Array.from({ length: 5 }, () => player
        ? scene.addPlayerPokemon(speciesDataRegistry.getSpecies(SpeciesId.PIDGEY), 5)
        : scene.addEnemyPokemon(speciesDataRegistry.getSpecies(SpeciesId.RATTATA), 5, TrainerSlot.NONE));
    const playerBench = benchPool(true);
    let epicks: string[] = [];
    let picks: number[] = [];
    let pickAt = 0;
    const uiSetMode = scene.ui.setMode.bind(scene.ui);
    let checkYes = false;
    scene.ui.setMode = (mode: UiMode, ...args: any[]) => {
        if (mode !== UiMode.PARTY) {
            return uiSetMode(mode, ...args);
        }
        const party = scene.getPlayerParty();
        const first = battle.getBattlerCount();
        const eligible = party.map((_: any, k: number) => k).filter((k: number) => k >= first && !party[k].isFainted());
        const r = picks[pickAt++ % picks.length];
        args[2](eligible.length ? eligible[r % eligible.length] : -1, PartyOption.SEND_OUT);
        return Promise.resolve();
    };
    const L = (xs: any[]) => (xs.length ? xs.join(",") : "-");
    const num = (v: any) => (v === undefined || v === null ? "u" : String(v));
    const rngState = (st: string) => st.split(",").slice(1).join(",");
    const turnMove = (t: any) => `${t.move}:${L(t.targets).replace(/,/g, "/")}:${t.useMode}:${num(t.result)}`;
    const monLines = (p: any, b: string) => {
        const td = p.turnData;
        const sd = p.summonData;
        const st = p.status;
        if (st && st.effect === StatusEffect.NONE) {
            throw new Error("Status object with effect NONE");
        }
        return [
            [
                `MON b=${b} id=${p.id} sp=${p.species.speciesId} form=${p.formIndex}${p.fusionSpecies ? ` fsp=${p.fusionSpecies.speciesId} fform=${p.fusionFormIndex} fabil=${p.fusionAbilityIndex}` : ""} lv=${p.level} ivs=${L(p.ivs)} nat=${p.nature}`,
                `ab=${num(sd.ability)} abil=${p.getAbility().id} babil=${p.getAbility(true).id} passive=${p.passive ? 1 : 0} cpass=${p.customPokemonData.passive} gender=${p.gender}`,
                `friend=${p.friendship} hp=${p.hp} stats=${L(p.stats)} st=${st ? st.effect : 0} toxic=${st ? st.toxicTurnCount : 0}`,
                `sleep=${num(st?.sleepTurnsRemaining)} freeze=${num(st?.freezeTurnsRemaining)} stages=${L(sd.statStages)}`,
                `tera=${p.isTerastallized ? 1 : 0} teratype=${p.teraType} stellar=${L([...p.stellarTypesBoosted].sort((x, y) => x - y))}`,
                `boss=${p.isBoss() ? 1 : 0} shiny=${p.isShiny(true) ? 1 : 0} onfield=${p.isOnField() ? 1 : 0} out=${p.switchOutStatus ? 1 : 0}`,
                `shc=${sd.hitCount} types=${L(sd.types)} added=${sd.addedType ?? 0} abilsup=${sd.abilitySuppressed ? 1 : 0}`,
                `tsd=${p.tempSummonData.turnCount} tswd=${p.tempSummonData.waveTurnCount}`,
                `aas=${L([...sd.abilitiesApplied])} aaw=${L([...p.waveData.abilitiesApplied])}`
                    + (p.battleData.hasEatenBerry ? " eb=1" : "")
                    + (p.trainerSlot ? ` tslot=${p.trainerSlot}` : "")
                    + (sd.stats.some((v: number) => v) ? ` ss=${L(sd.stats)}` : ""),
            ].join(" "),
            [
                `TURN b=${b} acted=${td.acted ? 1 : 0} order=${num(td.order)} hc=${td.hitCount} hl=${td.hitsLeft}`,
                `dealt=${td.totalDamageDealt} single=${td.singleHitDamageDealt} taken=${td.damageTaken}`,
                `inc=${td.statStagesIncreased ? 1 : 0} dec=${td.statStagesDecreased ? 1 : 0} eff=${num(td.moveEffectiveness)}`,
                `failedrun=${td.failedRunAway ? 1 : 0} pending=${td.pendingStatus} switched=${td.switchedInThisTurn ? 1 : 0} summoned=${td.summonedThisTurn ? 1 : 0}`,
                `joined=${td.joinedRound ? 1 : 0} pledge=${td.combiningPledge ?? 0}`,
                `recv=${L(td.attacksReceived.map((r: any) => `${r.move}:${r.result}:${r.damage}:${r.critical ? 1 : 0}:${r.sourceId}:${r.sourceBattlerIndex}`))}`,
            ].join(" "),
            `MOVES b=${b} list=${L(p.moveset.map((m: any) => `${m.moveId}:${m.ppUsed}:${m.ppUp}:${num(m.maxPpOverride)}`))}`,
            `HIST b=${b} list=${L(sd.moveHistory.map(turnMove))}`,
            `MQ b=${b} list=${L(sd.moveQueue.map(turnMove))}`,
            `EFFECTIVE b=${b} stats=${[Stat.ATK,Stat.DEF,Stat.SPATK,Stat.SPDEF,Stat.SPD].map(stat=>p.getEffectiveStat(stat)).join(",")}`,
            ...sd.tags.map((t: any) => `BTAG b=${b} type=${t.tagType} turns=${t.turnCount} smove=${t.sourceMove ?? 0} sid=${t.sourceId ?? -1}`
                + ` stock=${t.stockpiledCount ?? 0} crit=${t.critStages ?? 0} auto=${t.autotomizeCount ?? 0}`
                + (boostTags.includes(t.tagType) ? ` stat=${t.stat} mult=${t.multiplier}` : "")
                + (t.tagType === BattlerTagType.SUBSTITUTE ? ` hp=${t.hp} focus=${t.sourceInFocus ? 1 : 0}` : "")
                + (t.tagType === BattlerTagType.SEEDED ? ` sidx=${t.sourceIndex ?? -1}` : "")
                + (t.tagType === BattlerTagType.SHELL_TRAP && t.activated ? " act=1" : "")
                + (t.tagType === BattlerTagType.ROOSTED && t.isBasePureFlying ? " bpf=1" : "")
                + ([BattlerTagType.DISABLED, BattlerTagType.ENCORE, BattlerTagType.GORILLA_TACTICS].includes(t.tagType)
                    ? ` mid=${t.moveId ?? 0}`
                    : "")
                + (t.tagType === BattlerTagType.SUPREME_OVERLORD ? ` fc=${t.faintCount ?? 0}` : "")
                + (t.tagType === BattlerTagType.STOCKPILING
                    ? ` scd=${t.statChangeCounts[Stat.DEF]} scs=${t.statChangeCounts[Stat.SPDEF]}`
                    : "")),
        ];
    };
    const dumpState = (before = false) => {
        const lines: string[] = [];
        const bs = battle.battleSeedState;
        lines.push(`RNG g=${rngState(Phaser.Math.RND.state())} b=${bs ? rngState(bs) : "none"}`);
        const fh = (h: any[]) => L(h.map((e: any) => `${e.pokemon.id}:${e.turn}`));
        lines.push([
            `BATTLE turn=${battle.turn} wave=${battle.waveIndex} bseed=${battle.battleSeed} wseed=${scene.waveSeed}`,
            `lastmove=${battle.lastMove ?? 0} efaints=${battle.enemyFaints} pfaints=${scene.arena.playerFaints}`,
            `pfh=${fh(battle.playerFaintsHistory)} efh=${fh(battle.enemyFaintsHistory)}`,
            `lpi=${num(battle.lastPlayerInvolved)} lei=${num(battle.lastEnemyInvolved)}`,
            `participants=${L([...battle.playerParticipantIds].sort((x: number, y: number) => x - y))}`,
            `loot=${battle.postBattleLoot.length} score=${battle.battleScore} money=${battle.moneyScattered} type=${battle.battleType} double=${battle.double ? 1 : 0}`,
            `mel=${scene.getMaxExpLevel()} bar=${scene.abilityBar.isVisible() ? 1 : 0} ball=${num(battle.lastUsedPokeball)}`,
            `gm=${scene.gameMode.modeId}`,
        ].join(" "));
        const w = scene.arena.weather;
        const t = scene.arena.terrain;
        lines.push(`ARENA weather=${w ? w.weatherType : 0} wturns=${w ? w.turnsLeft : 0} wmax=${w ? w.maxDuration : 0}`
            + ` terrain=${t ? t.terrainType : 0} tturns=${t ? t.turnsLeft : 0} tmax=${t ? t.maxDuration : 0}`
            + ` ignab=${scene.arena.ignoreAbilities ? 1 : 0} ignsrc=${num(scene.arena.ignoringEffectSource)}`
            + ` biome=${scene.arena.biomeId}`);
        for (const at of scene.arena.tags) {
            lines.push(`ATAG type=${at.tagType} side=${at.side} turns=${at.turnCount} max=${at.maxDuration} smove=${at.sourceMove ?? 0} sid=${at.sourceId ?? -1}`
                + (at instanceof EntryHazardTag ? ` layers=${at.layers}` : ""));
        }
        for (const pt of scene.arena.positionalTagManager.tags as any[]) {
            lines.push(`POS type=${pt.tagType} turns=${pt.turnCount} target=${pt.targetIndex} sid=${pt.sourceId ?? -1}`
                + ` smove=${pt.sourceMove ?? 0} heal=${pt.healHp ?? 0}`
                + (before && pt.wisherId !== undefined ? ` wisher=${pt.wisherId}` : ""));
        }
        for (const [side, party] of [[0, scene.getPlayerParty()], [1, scene.getEnemyParty()]] as [
            number,
            any[]
        ][]) {
            party.forEach((p: any, k: number) => {
                lines.push(...monLines(p, k === 0 ? String(side * 2) : `${side ? "e" : "p"}${k}`));
            });
        }
        for (const [side, list] of [[0, scene.modifiers], [1, scene.enemyModifiers]] as [
            number,
            any[]
        ][]) {
            for (const m of list) {
                lines.push(`MOD side=${side} cls=${m.constructor.name} pid=${m.pokemonId ?? 0} stack=${m.stackCount}`
                    + (m.berryType !== undefined ? ` btype=${m.berryType}` : "")
                    + (m.constructor.name === "EnemyAttackStatusEffectChanceModifier" ? ` effect=${m.effect}` : "")
                    + (m.constructor.name === "TurnStatusEffectModifier" ? ` effect=${m.getStatusEffect()}` : "")
                    + (["EvolutionStatBoosterModifier", "SpeciesStatBoosterModifier"].includes(m.constructor.name) ? ` virtual=${m.virtualStackCount} mask=${m.getArgs()[1].reduce((mask: number, stat: number) => mask | (1 << stat), 0)} mult=${m.getArgs()[2]} species=${L(m.species ?? [])}` : ""));
            }
        }
        for (const [side, party] of [[0, scene.getPlayerParty()], [1, scene.getEnemyParty()]] as [
            number,
            any[]
        ][]) {
            lines.push(`PARTY side=${side} list=${L(party.map((m: any) => m.id))}`);
        }
        lines.push(`PICK list=${L(picks)}`);
        if (battle.trainer) {
            const tr = battle.trainer;
            lines.push(`TRAINER partner=${tr.isPartner() ? 1 : 0} names=${JSON.stringify([tr.getName(TrainerSlot.TRAINER), tr.getName(TrainerSlot.TRAINER_PARTNER)])}`);
        }
        for (let k = 0; k < 4; k++) {
            const c = battle.turnCommands[k];
            if (c) {
                lines.push(`CMD b=${k} cmd=${c.command} move=${c.move?.move ?? 0} targets=${L(c.move?.targets ?? [])} mode=${c.move?.useMode ?? 0} skip=${c.skip ? 1 : 0}`
                    + ` cursor=${num(c.cursor)} baton=${c.args?.[0] === true ? 1 : 0}`);
            }
            const pc = battle.preTurnCommands[k];
            if (pc) {
                lines.push(`PRE b=${k} cmd=${pc.command} skip=${pc.skip ? 1 : 0}`);
            }
        }
        const healed = healStatusAttrs.filter(([, x]) => x.statusHealed).map(([id, x]) => `${id}:${x.statusHealed}`);
        if (healed.length) {
            lines.push(`SESSION healed=${L(healed)}`);
        }
        const sel = [MoveId.SLEEP_TALK, MoveId.ASSIST]
            .map(id => [id, (allMoves[id].getAttrs("RandomMovesetMoveAttr")[0] as any).selectedMove])
            .filter(([, v]) => v);
        if (sel.length) {
            lines.push(`CALLSEL list=${L(sel.map(([id, v]) => `${id}:${v}`))}`);
        }
        return lines;
    };
    const afterBattle = new Set([
        "TrainerVictoryPhase", "GameOverPhase", "EggLapsePhase", "ModifierRewardPhase", "SelectModifierPhase",
        "AddEnemyBuffModifierPhase", "SelectBiomePhase", "NewBattlePhase",
    ]);
    const queueLines = () => {
        const tree = pm.phaseQueue;
        const nm = (ph: any) => (ph.phaseName === "DynamicPhaseMarker" ? `Marker:${ph.phaseType}` : ph.phaseName);
        const levels = tree.levels.map((lv: any[]) => L(lv.filter((ph: any) => !afterBattle.has(ph.phaseName)).map(nm)));
        const dyn: string[] = [];
        for (const [name, q] of pm.dynamicQueueManager.dynamicPhaseMap) {
            if (q.queue.length) {
                dyn.push(`${name}:${q.queue.map((ph: any) => ph.battlerIndex).join("/")}`);
            }
        }
        dyn.sort();
        return [`QUEUE levels=${levels.join("|")} deferred=${tree.deferredActive ? 1 : 0} dyn=${L(dyn)}`];
    };
    let started = 0;
    pm.startCurrentPhase = () => {
        started++;
    };
    const boundary = new Set([
        "TurnInitPhase", "GameOverPhase", "BattleEndPhase", "SelectModifierPhase",
    ]);
    scene.applyPartyExp = () => { };
    const phaseTag = (ph: any) => {
        const b = ph.battlerIndex;
        return typeof ph.getPokemon === "function" && typeof b === "number" ? `${ph.phaseName}:${b}` : ph.phaseName;
    };
    const b01 = (v: unknown) => (v ? 1 : 0);
    const stopArgs = (ph: any) => {
        if (ph.phaseName === "SwitchPhase") {
            return ` type=${ph.switchType} field=${ph.fieldIndex} modal=${b01(ph.isModal)} return=${b01(ph.doReturn)}`;
        }
        if (ph.phaseName === "SwitchSummonPhase") {
            return ` type=${ph.switchType} field=${ph.fieldIndex} slot=${ph.slotIndex} return=${b01(ph.doReturn)} player=${b01(ph.player)}`;
        }
        return "";
    };
    let shown: string[] = [];
    const ui = scene.ui as any;
    const uiShowText = ui.showText.bind(ui);
    ui.showText = (text: string, ...rest: any[]) => {
        const prompt = rest[3];
        if (!(prompt && String(text).indexOf("$") > -1)) {
            shown.push(String(text));
        }
        return uiShowText(text, ...rest);
    };
    const uiShowDialogue = ui.showDialogue.bind(ui);
    ui.showDialogue = (text: string, name: string, ...rest: any[]) => {
        shown.push(`${name}: ${text}`);
        return uiShowDialogue(text, name, ...rest);
    };
    const runTurn = async () => {
        const trace: string[] = [];
        shown = [];
        {
            pm.pushNew("TurnStartPhase");
        }
        pm.shiftPhase();
        for (let guard = 0;; guard++) {
            const ph = pm.getCurrentPhase();
            if (boundary.has(ph.phaseName)) {
                return { trace, stop: ph.phaseName + stopArgs(ph) };
            }
            if (guard > 400) {
                throw new Error("turn did not end");
            }
            trace.push(phaseTag(ph));
            const before = started;
            ph.start();
            for (let spins = 0; started === before; spins++) {
                await new Promise(r => setTimeout(r, 0));
                if (spins > 20000) {
                    throw new Error("stuck in " + ph.phaseName);
                }
            }
        }
    };
    const setup = (p: any) => {
        p.resetSummonData();
        p.resetTurnData();
        p.resetBattleAndWaveData();
        p.switchOutStatus = false;
        if (!p.isOnField()) {
            scene.field.add(p);
        }
        const formSpeciesIds = [
            SpeciesId.AEGISLASH,
            SpeciesId.DARMANITAN,
            SpeciesId.GALAR_DARMANITAN,
            SpeciesId.MINIOR,
            SpeciesId.MORPEKO,
        ];
        const sp = speciesDataRegistry.getSpecies(FORMS ? formSpeciesIds[rnd(formSpeciesIds.length)] : ids[rnd(ids.length)]);
        p.species = sp;
        p.formIndex = sp.forms.length ? rnd(sp.forms.length) : 0;
        if (FUSION && rnd(2) === 0) {
            const fsp = speciesDataRegistry.getSpecies(ids[rnd(ids.length)]);
            p.fusionSpecies = fsp;
            p.fusionFormIndex = fsp.forms.length ? rnd(fsp.forms.length) : 0;
            p.fusionAbilityIndex = rnd(3);
        }
        else {
            p.fusionSpecies = null;
        }
        p.level = 1 + rnd(100);
        p.ivs = [0, 0, 0, 0, 0, 0].map(() => rnd(32));
        p.nature = rnd(25);
        p.passive = false;
        p.customPokemonData.passive = AbilityId.BALL_FETCH;
        p.summonData.ability = FORMS
            ? p.getAbility(true).id
            :
                (AbilityId.BALL_FETCH);
        p.gender = rnd(3) - 1;
        p.friendship = rnd(256);
        p.isTerastallized = false;
        p.teraType = rnd(19);
        p.stellarTypesBoosted = [];
        p.calculateStats();
        p.generateName();
        p.hp = FORMS
            ? (rnd(2) === 0 ? p.getMaxHp() : 1 + rnd(Math.floor(p.getMaxHp() * 0.5)))
            : (rnd(3) === 0 ? p.getMaxHp() : 1 + rnd(p.getMaxHp()));
        p.status = null;
        const n = 1 + rnd(4);
        p.moveset = [];
        for (let k = 0; k < n; k++) {
            const m = rnd(10) < 7 ? attacks[rnd(attacks.length)] : usable[rnd(usable.length)];
            p.moveset.push(new PokemonMove(m.id, rnd(8) === 0 ? rnd(m.pp + 1) : 0));
        }
    };
    const commandMove = (p: any, move: MoveId) => {
        const slot = rnd(p.moveset.length);
        p.moveset[slot] = new PokemonMove(move);
        const turnMove = { move, targets: getMoveTargets(p, move).targets, useMode: MoveUseMode.NORMAL };
        battle.turnCommands[p.getBattlerIndex()] = p.isPlayer()
            ? { command: Command.FIGHT, cursor: slot, move: turnMove, args: [MoveUseMode.NORMAL, undefined] }
            : { command: Command.FIGHT, move: turnMove, skip: false };
    };
    const command = (p: any) => {
        const slots = p.moveset.map((_: any, k: number) => k).filter((k: number) => p.moveset[k].ppUsed < p.moveset[k].getMovePp());
        const moveId = slots.length ? p.moveset[slots[rnd(slots.length)]].moveId : MoveId.STRUGGLE;
        const targets = getMoveTargets(p, moveId).targets;
        const b = p.getBattlerIndex();
        if (p.isPlayer()) {
            battle.turnCommands[b] = { command: Command.FIGHT, cursor: 0, move: { move: moveId, targets, useMode: MoveUseMode.NORMAL }, args: [MoveUseMode.NORMAL, undefined] };
            battle.preTurnCommands[b] = { command: Command.FIGHT, targets: [b], skip: true };
        }
        else {
            battle.turnCommands[b] = { command: Command.FIGHT, move: { move: moveId, targets, useMode: MoveUseMode.NORMAL }, skip: false };
        }
    };
    let recordingTurn=false,state=0;
    const stateCounts=Array(30).fill(0), sideCounts=[0,0], applies=Array(30).fill(0),refusals=Array(30).fill(0);
    const originalCan=SpeedBoostAbAttr.prototype.canApply;
    const originalApply=SpeedBoostAbAttr.prototype.apply;
    SpeedBoostAbAttr.prototype.canApply=function(params) {
        const result=originalCan.call(this,params);
        if(recordingTurn&&!params.simulated&&!result)refusals[state]++;
        return result;
    };
    SpeedBoostAbAttr.prototype.apply=function(params) {
        const result=originalApply.call(this,params);
        if(recordingTurn)applies[state]++;
        return result;
    };
    const out:string[]=[];
    const thrown: Record<string, number> = {};
    for (let i = 0; i < N; i++) {
        picks = [0, 1, 2].map(j => (Math.imul(3 * i + j + 1, 2654435761) >>> 0) % 60);
        pickAt = 0;
        pm.clearAllPhases();
        pm.phaseQueue.deferredActive = false;
        pm.dynamicQueueManager.clearLastTurnOrder();
        scene.abilityBar.setVisible(false);
        scene.tweens.add = (config: any) => { config.onComplete?.(); return {} as any; };
        if ((scene as any).ui?.messageHandler)
            (scene as any).ui.messageHandler.prompt = null;
        battle.battleScore = 0;
        battle.playerFaintsHistory.length = 0;
        battle.enemyFaintsHistory.length = 0;
        battle.enemyFaints = 0;
        scene.arena.playerFaints = 0;
        battle.playerParticipantIds.clear();
        scene.modifiers.length = 0;
        scene.enemyModifiers.length = 0;
        scene.arena.tags.length = 0;
        scene.arena.positionalTagManager.tags.length = 0;
        scene.arena.weather = null;
        scene.arena.terrain = null;
        battle.turn = 1 + rnd(30);
        battle.turnCommands = {};
        battle.preTurnCommands = {};
        setup(A);
        setup(D);
        const party = scene.getPlayerParty();
        party.length = 0;
        party.push(A);
        const benchSize = 1 + rnd(playerBench.length);
        for (const b of playerBench) {
            if (b.isOnField()) {
                scene.field.remove(b);
            }
            b.switchOutStatus = false;
            b.resetSummonData();
            b.resetTurnData();
            b.resetBattleAndWaveData();
            b.customPokemonData.ability = -1;
        }
        for (const b of playerBench.slice(0, benchSize)) {
            b.species = speciesDataRegistry.getSpecies(ids[rnd(ids.length)]);
            b.formIndex = 0;
            b.calculateStats();
            b.generateName();
            b.hp = rnd(4) === 0 ? 0 : b.getMaxHp();
            b.status = b.hp ? null : new Status(StatusEffect.FAINT);
            party.push(b);
        }
        battle.playerParticipantIds.add(A.id);
        state=Math.floor(i/2)%30;
        const holder=i%2?D:A, other=holder===A?D:A;
        for(const p of [...party,...scene.getEnemyParty()]) {
            p.species=speciesDataRegistry.getSpecies(SpeciesId.RATTATA);
            p.formIndex=0;p.fusionSpecies=null;p.generateName();
            p.summonData.ability=AbilityId.BALL_FETCH;
            p.customPokemonData.passive=AbilityId.NONE;p.passive=false;
            p.status=null;p.isTerastallized=false;p.summonData.abilitySuppressed=false;
            p.summonData.statStages.fill(0);p.moveset=[new PokemonMove(MoveId.SPLASH)];
            p.level=30+rnd(21);p.calculateStats();p.hp=p.getMaxHp();
            p.tempSummonData.turnCount=2;p.tempSummonData.waveTurnCount=2;
        }
        holder.summonData.ability=AbilityId.SPEED_BOOST;
        if([1,3,27].includes(state))holder.turnData.switchedInThisTurn=true;
        if([2,3,26].includes(state))holder.turnData.failedRunAway=true;
        if([4,20,23].includes(state))holder.summonData.abilitySuppressed=true;
        if([5,8,9,18,20].includes(state)) {
            holder.summonData.ability=state===8?AbilityId.SIMPLE:state===9?AbilityId.CONTRARY:AbilityId.BALL_FETCH;
            holder.passive=true;holder.customPokemonData.passive=AbilityId.SPEED_BOOST;
        }
        if([6,7,13,28].includes(state)) {holder.passive=true;holder.customPokemonData.passive=state===6?AbilityId.SIMPLE:AbilityId.CONTRARY;}
        if([10,28].includes(state))holder.summonData.statStages[Stat.SPD-1]=5;
        if([11,26].includes(state))holder.summonData.statStages[Stat.SPD-1]=6;
        if([12,13].includes(state))holder.summonData.statStages[Stat.SPD-1]=-6;
        if([14,15,16,27].includes(state)) {
            holder.tempSummonData.turnCount=state===14?0:state===15?1:100;
            holder.tempSummonData.waveTurnCount=holder.tempSummonData.turnCount;
        }
        if(state===21) {holder.hp=0;holder.status=new Status(StatusEffect.FAINT);}
        if(state===22) {holder.hp=1;other.level=100;other.calculateStats();other.hp=other.getMaxHp();}
        if(state===24)other.summonData.ability=AbilityId.SPEED_BOOST;
        if(state===25)holder.summonData.ability=AbilityId.BALL_FETCH;
        commandMove(other,[22,29].includes(state)?MoveId.TACKLE:MoveId.SPLASH);
        commandMove(holder,[17,29].includes(state)?MoveId.PROTECT:MoveId.SPLASH);
        let before: string[];
        let run: {
            trace: string[];
            stop: string;
        };
        let texts: string[];
        try {
            before = dumpState(true);
            epicks = [];
            recordingTurn = true;
            run = await runTurn();
            recordingTurn = false;
            texts = shown;
        }
        catch (e: any) {
            const key = String(e?.message ?? e).slice(0, 80);
            if (!thrown[key] && process.env.TURN_STACKS) {
                console.error("THROWN", e?.stack);
            }
            thrown[key] = (thrown[key] ?? 0) + 1;
            continue;
        }
        stateCounts[state]++;
        sideCounts[i%2]++;
        out.push(`CASE ${i}`, ...before, "RUN", `TRACE list=${L(run.trace)}`, `TEXTS ${JSON.stringify(texts)}`, ...dumpState(), ...queueLines(), `END stop=${run.stop}`);
    }
    if (Object.keys(thrown).length)
        throw new Error(`upstream exceptions: ${JSON.stringify(thrown)}`);
    if (stateCounts.reduce((a, b) => a + b, 0) !== N)
        throw new Error("incomplete trace");
    out.unshift(`# SPEEDBOOST_CONTRACT 1 ${N}`);
    out.push(`# SPEEDBOOST_COMPLETE ${N}`);
    console.log(`SPEEDBOOST cases=${N} applies=${applies.join("/")} refusals=${refusals.join("/")} states=${stateCounts.join("/")} sides=${sideCounts.join("/")}`);
    if(N>=2000 && (![0,5,6,7,8,9,10,11,12,13,14,15,16,17,18,24,28,29].every(k=>applies[k]>0)||![1,2,3,26,27].every(k=>refusals[k]>0)))throw new Error("missing actual Speed Boost coverage");
    SpeedBoostAbAttr.prototype.canApply=originalCan;
    SpeedBoostAbAttr.prototype.apply=originalApply;
    fs.writeFileSync(OUT, out.join("\n") + "\n");
}, 3600000);
