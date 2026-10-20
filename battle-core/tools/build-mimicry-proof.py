#!/usr/bin/env python3
"""Native Mimicry dispatcher, sharing the strict raw frame/queue transport."""
from pathlib import Path
import sys
r=Path(__file__).resolve().parents[1];s=(r/'tests/keldeo-forms.producer.test.ts').read_text()
s=s.replace('import { TrainerSlot }','import { Terrain } from "#data/terrain";\nimport { applyAbAttrs } from "#abilities/apply-ab-attrs";\nimport { TrainerSlot }')
s=s.replace('const N=33792,seed=Number(process.env.KELDEO_SEED??777)','const N=16384,seed=Number(process.env.MIMICRY_SEED??777)')
s=s.replace('process.env.KELDEO_OUT??"/tmp/keldeo.txt"','process.env.MIMICRY_OUT??"/tmp/mimicry.txt"').replace('`KELDEO 5 ${N} ${seed}`','`MIMICRY 1 ${N} ${seed}`')
s=s.replace('let queued=0,executed=0,nonquiet=0,modalExecutions=0;','const applied=vi.spyOn(allAbilities[AbilityId.MIMICRY].getAttrs("TerrainEventTypeChangeAbAttr")[0],"apply");let changed=0;')
a=s.index('  const u=');b=s.index('  pm.clearAllPhases();',a)
s=s[:a]+'''  const u=i%8,mode=Math.floor(i/8)%2,terrain=Math.floor(i/16)%5,shape=Math.floor(i/80)%8;
  scene.gameMode=getGameMode(GameModes.CLASSIC);scene.arena.terrain=terrain?new Terrain(terrain):null;
  for(let k=0;k<8;k++){
   const p=actors[k];p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies(k===u?[SpeciesId.KELDEO,SpeciesId.RATTATA,SpeciesId.BULBASAUR][Math.floor(i/640)%3]:SpeciesId.BULBASAUR);p.formIndex=k===u&&p.species.speciesId===SpeciesId.KELDEO?Math.floor(i/1920)%2:0;p.fusionSpecies=null;p.customPokemonData.ability=k===u&&![1,7].includes(shape)?AbilityId.MIMICRY:AbilityId.BALL_FETCH;p.passive=k===u&&[1,2].includes(shape);p.customPokemonData.passive=p.passive?AbilityId.MIMICRY:AbilityId.NONE;p.level=[1,50,256,1000000][Math.floor(i/3840)%4];p.gender=-1;p.abilityIndex=0;p.calculateStats();p.hp=k===u&&shape===3?0:p.getMaxHp();p.status=null;p.moveset=[new PokemonMove(MoveId.SPLASH)];p.visible=false;p.switchOutStatus=false;p.isTerastallized=k===u&&shape===5;p.teraType=18;p.stellarTypesBoosted=[];p.summonData.addedType=k===u?Math.floor(i/3)%19:null;p.summonData.types=k===u?[7,11]:[];p.summonData.abilitySuppressed=k===u&&shape===4;
   if(k===u&&shape===6){p.summonData.speciesForm=speciesDataRegistry.getSpecies(SpeciesId.BULBASAUR);p.summonData.gender=0;p.summonData.moveset=[new PokemonMove(MoveId.TACKLE)];}p.generateName();
  }
'''+s[b:]
a=s.index('  const direct=');b=s.index('  rows.push(...actors.map',a)
s=s[:a]+'''  const before=snapshot(actors[u]);
  const rows=[`C ${i} ${u} ${mode} ${GameModes.CLASSIC} 0 0 ${terrain}`,`W ${scene.waveSeed}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(battle.battleSeedState)}`,...actors.map((p,k)=>`I ${k} ${snapshot(p).join(" ")}`),`IT ${actors.map(p=>p.getTag(BattlerTagType.AUTOTOMIZED)?.autotomizeCount??0).join(" ")}`,`IW ${actors.map(p=>p.getWeight()).join(" ")}`];
  applyAbAttrs("TerrainEventTypeChangeAbAttr",{pokemon:actors[u],simulated:!!mode});
  rows.push("RESULT 0",...queues());changed+=+(before.join(" ")!==snapshot(actors[u]).join(" "));
'''+s[b:]
a=s.index(' if(queued<100');b=s.index('\n},600000);',a)
s=s[:a]+''' if(applied.mock.calls.length<1000||changed<1000)throw Error(`coverage ${applied.mock.calls.length}/${changed}`);write([`MIMICRY_COMPLETE ${N}`]);fs.closeSync(fd);console.log(`MIMICRY applied=${applied.mock.calls.length} changed=${changed}`);'''+s[b:]
s=s.replace('it("native Keldeo learned-move form queues and phase execution"','it("native Mimicry dispatcher with simulated mutation and terrain fallback"')
Path(sys.argv[1]).write_text(s)
