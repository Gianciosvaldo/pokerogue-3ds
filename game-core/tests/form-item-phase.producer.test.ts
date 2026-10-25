/* Preliminary vertical owner probe. Real party action, modal dispatch,
 * Pokemon.changeForm, dex setters and phase.end; animation is an explicit
 * completion boundary. State/profile/message results are independently replayed in C. */
import { getPokemonNameWithAffix } from "#app/messages";
import { BattleScene } from "#app/battle-scene";
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { getSpeciesFormChangeMessage, SpeciesFormChangeItemTrigger } from "#data/form-change-triggers";
import { AbilityId } from "#enums/ability-id";
import { BattlerTagType } from "#enums/battler-tag-type";
import { FormChangeItem } from "#enums/form-change-item";
import { MoveId } from "#enums/move-id";
import { PartyUiMode } from "#enums/party-ui-mode";
import { SpeciesId } from "#enums/species-id";
import { UiMode } from "#enums/ui-mode";
import { FormChangeItemModifierType } from "#modifiers/modifier-type";
import { PartyOption, PartyUiHandler } from "#ui/party-ui-handler";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import fs from "node:fs";
import { expect, it, vi } from "vitest";

it("native party activation through actual form state and modal return", async () => {
  const native = BattleScene.prototype.randBattleSeedInt;
  const game = new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
  game.override.battleStyle("double").moveset(MoveId.SPLASH).enemyMoveset(MoveId.SPLASH).enemySpecies(SpeciesId.MEW);
  await game.classicMode.startBattle(SpeciesId.MEW,SpeciesId.MEW,SpeciesId.MEW);
  BattleScene.prototype.randBattleSeedInt = native;
  const scene:any=game.scene,pm:any=scene.phaseManager,party:any[]=scene.getPlayerParty();
  expect(party.length).toBe(3);expect(party[2].isOnField()).toBe(false);
  const ui:any=Object.create(PartyUiHandler.prototype);
  ui.partyUiMode=PartyUiMode.CHECK;ui.selectFilter=()=>null;ui.selectCallback=null;
  ui.clearPartySlots=vi.fn();ui.populatePartySlots=vi.fn();
  // Widgets/assets are omitted. Mechanical methods and source phase selection,
  // override/standby restoration and phase.end itself remain real.
  vi.spyOn(scene.ui,"getHandler").mockImplementation(()=>ui);
  vi.spyOn(scene.ui,"revertMode").mockImplementation(async()=>{scene.ui._mode=UiMode.PARTY;});
  vi.spyOn(pm,"startCurrentPhase").mockImplementation(()=>{});
  vi.spyOn(scene,"updateModifiers").mockImplementation(()=>{});
  for(const p of party){vi.spyOn(p,"loadAssets").mockResolvedValue(undefined);vi.spyOn(p,"updateInfo").mockResolvedValue(undefined);}
  const refs:any[]=[];
  for(const s of speciesDataRegistry.getAllSpecies())if(speciesDataRegistry.hasFormChanges(s.speciesId))
    for(const fc of speciesDataRegistry.getFormChanges(s.speciesId))if(fc.findTrigger(SpeciesFormChangeItemTrigger))refs.push(fc);
  expect(refs.length).toBe(416);
  const index=new Map(refs.map((fc,i)=>[fc,i])),records:any[]=[];
  const make=(item:number,p:any,active:boolean)=>{const t:any=new FormChangeItemModifierType(item);t.id="FORM_CHANGE_ITEM";t.tier=0;const m:any=t.newModifier(p);m.active=active;return m;};
  const state=(p:any)=>({species:p.species.speciesId,form:p.formIndex,level:p.level,nature:p.getNature(),gender:p.gender,
    abilityIndex:p.abilityIndex,baseAbility:p.getAbility(true).id,ability:p.getAbility().id,hp:p.hp,stats:[...p.stats],ivs:[...p.ivs],
    fusionSpecies:p.fusionSpecies?.speciesId??0,fusionForm:p.fusionSpecies?p.fusionFormIndex:0,fusionAbilityIndex:p.fusionSpecies?p.fusionAbilityIndex:-1,
    tera:p.isTerastallized,teraType:p.teraType,auto:!!p.getTag(BattlerTagType.AUTOTOMIZED),onField:p.isOnField()});
  const inputs:any[]=[],focused=process.env.FORM_PHASE_FOCUS==="1",fused=process.env.FORM_PHASE_FUSION==="1";
  if(fused){
    for(let rule=0;rule<refs.length;rule++)for(const owner of [0,2])for(const fusion of [151,479])
      inputs.push({group:3,rule,owner,level:5,nature:-1,fusion});
  }else if(!focused){
    for(let rule=0;rule<refs.length;rule++)for(const owner of [0,2])inputs.push({group:0,rule,owner,level:5,nature:-1});
    for(let rule=0;rule<refs.length;rule++)for(const owner of [0,2])for(const level of [5,1000,65536])for(let nature=0;nature<25;nature++)
      inputs.push({group:1,rule,owner,level,nature});
  }else{
    for(let rule=0;rule<refs.length;rule++)for(const owner of [0,2])for(const level of [5,65536])for(let abilityIndex=0;abilityIndex<3;abilityIndex++)for(const tera of [false,true])
      inputs.push({group:2,rule,owner,level,nature:-1,abilityIndex,tera,hpMode:abilityIndex});
  }
  expect(inputs.length).toBe(fused?1664:focused?9984:63232);
  for(const input of inputs){
    const {group,rule,owner,level,nature}=input;
    const fc=refs[rule],trigger:any=fc.findTrigger(SpeciesFormChangeItemTrigger),p=party[owner];
    for(const q of [...party,...scene.getEnemyParty()]){q.species=speciesDataRegistry.getSpecies(SpeciesId.MEW);q.formIndex=0;q.resetSummonData();q.fusionSpecies=null;q.passive=false;q.hp=1;q.isTerastallized=false;q.summonData.ability=AbilityId.LEVITATE;}
    p.species=speciesDataRegistry.getSpecies(fc.speciesId);p.formIndex=p.species.forms.findIndex((f:any)=>f.formKey===fc.preFormKey);
    if(input.fusion){p.fusionSpecies=speciesDataRegistry.getSpecies(input.fusion);p.fusionFormIndex=input.fusion===479?1:0;p.fusionAbilityIndex=0;p.fusionGender=-1;p.fusionShiny=false;p.fusionVariant=0;}
    p.abilityIndex=input.abilityIndex??0;p.summonData.ability=AbilityId.NONE;p.level=level;p.customPokemonData.nature=-1;
    p.nature=nature>=0?nature:([fc.preFormKey,fc.formKey].includes("lowkey")?1:0);
    p.gender=[fc.preFormKey,fc.formKey].includes("female")?1:p.species.malePercent==null?-1:p.species.malePercent===0?1:0;
    p.isTerastallized=input.tera??false;p.shiny=false;p.variant=0;p.generateName();
    scene.modifiers=[];scene.enemyModifiers=[];
    for(const id of [643,644,791,792,896,897])scene.gameData.dexData[id].caughtAttr=1n;
    scene.gameData.dexData[p.species.speciesId].caughtAttr=p.getDexAttr();
    const m=make(trigger.item,p,!trigger.active);scene.modifiers.push(m);
    if(fc.speciesId===SpeciesId.NECROZMA&&trigger.item===FormChangeItem.ULTRANECROZIUM_Z){
      const lunar=fc.preFormKey==="dawn-wings"||fc.formKey==="dawn-wings";
      scene.modifiers.push(make(lunar?FormChangeItem.N_LUNARIZER:FormChangeItem.N_SOLARIZER,p,true));
    }
    p.calculateStats();p.hp=input.hpMode===0?0:input.hpMode===2?p.getMaxHp():1;p.addTag(BattlerTagType.AUTOTOMIZED,1);
    const before=state(p),preName=getPokemonNameWithAffix(p),globalRng=Phaser.Math.RND.state(),battleRng=scene.currentBattle.battleSeedState;
    const offered=ui.getFormChangeItemsModifiers(p),option=offered.indexOf(m);expect(option).toBeGreaterThanOrEqual(0);
    pm.phaseQueue.levels=[[]];pm.phaseQueue.currentLevel=0;pm.phaseQueue.deferredActive=false;
    pm.standbyPhase=null;const select=pm.create("SelectModifierPhase");pm.currentPhase=select;ui.cursor=owner;
    const result=ui.processActionButtonForOptions(PartyOption.FORM_CHANGE_ITEM+option);
    expect(m.active).toBe(trigger.active);expect(result).toBe(false);
    const phase=pm.getCurrentPhase(),fired=phase!==select;
    if(group===0)expect(fired).toBe(true);
    if(fired){expect(phase.phaseName).toBe("FormChangePhase");expect(phase.modal).toBe(true);expect(pm.standbyPhase).toBe(select);}
    const selected=fired?index.get(phase.formChange):-1;expect(selected).not.toBeUndefined();
    const realDex=scene.gameData.dexData,dexBefore:any={},realStarter=scene.gameData.starterData,starterBefore:any={};
    const observe=(data:any,before:any)=>new Proxy(data,{get(target,key){
      const value=target[key];if(typeof key==="string"&&/^\d+$/.test(key)&&value&&!(key in before))before[key]=structuredClone(value);return value;
    }});
    scene.gameData.dexData=observe(realDex,dexBefore);scene.gameData.starterData=observe(realStarter,starterBefore);
    try{
      if(fired){
        phase.onBeforeSpriteSetup();
        await p.changeForm(phase.formChange); // actual animation completion callback
        phase.end();await Promise.resolve();
      }
    }finally{scene.gameData.dexData=realDex;scene.gameData.starterData=realStarter;}
    expect(pm.getCurrentPhase()).toBe(select);expect(pm.standbyPhase).toBeFalsy();
    expect(!!p.getTag(BattlerTagType.AUTOTOMIZED)).toBe(!fired);
    expect(Phaser.Math.RND.state()).toBe(globalRng);expect(scene.currentBattle.battleSeedState).toBe(battleRng);
    const afterFor=(data:any,keys:any)=>Object.fromEntries(Object.keys(keys).map(key=>[key,structuredClone(data[key])]));
    records.push({group,rule,owner,inputAbilityIndex:input.abilityIndex??0,inputTera:input.tera??false,selected,before,after:state(p),active:m.active,
      message:fired?getSpeciesFormChangeMessage(p,phase.formChange,preName):"",
      dexBefore,dexAfter:afterFor(realDex,dexBefore),starterBefore,starterAfter:afterFor(realStarter,starterBefore)});
  }
  expect(records.length).toBe(fused?1664:focused?9984:63232);
  fs.writeFileSync(process.env.FORM_PHASE_OUT!,JSON.stringify({version:1,pin:"8555c08c823b856cbec4eb99ca84ea52a955836d",
    ids:party.map(p=>p.id),records},(_key,value)=>typeof value==="bigint"?value.toString():value)+"\n");
},600000);
