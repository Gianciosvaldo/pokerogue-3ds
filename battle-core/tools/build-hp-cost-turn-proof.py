#!/usr/bin/env python3
"""Actual self-cost stat boosts, custom messages and maximum-HP overrides."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1])
subprocess.run([sys.executable,root/'tools/build-wide-combat-proof.py',out],check=True)
s=out.read_text()
def put(old,new):
 global s
 assert s.count(old)==1,old[:100]
 s=s.replace(old,new,1)
put('  const rawDamage=vi.spyOn(Pokemon.prototype,"getAttackDamage");','  const observed=[MoveId.BELLY_DRUM,MoveId.CLANGOROUS_SOUL,MoveId.FILLET_AWAY].map(m=>vi.spyOn(allMoves[m].getAttrs("CutHpStatStageBoostAttr")[0],"apply"));')
put('moves=[MoveId.TACKLE,MoveId.DRAGON_PULSE,MoveId.SEISMIC_TOSS,MoveId.DRAGON_RAGE,MoveId.FISSURE,MoveId.ERUPTION,MoveId.HEAT_CRASH,MoveId.STORED_POWER]','moves=[MoveId.BELLY_DRUM,MoveId.CLANGOROUS_SOUL,MoveId.FILLET_AWAY,MoveId.TACKLE]')
put('      if(i%3!==0){','''      p.customPokemonData.ability=[AbilityId.BALL_FETCH,AbilityId.SIMPLE,AbilityId.CONTRARY][(Math.floor(i/32)+k)%3];p.summonData.ability=p.customPokemonData.ability;
      p.hp=[1,p.getMaxHp(),Math.max(1,Math.floor(p.getMaxHp()/2)),Math.max(1,p.getMaxHp()-1)][(Math.floor(i/16)+k)%4];
      for(let stat=1;stat<=7;stat++)p.summonData.statStages[stat-1]=[-6,0,3,6][(Math.floor(i/64)+k+stat)%4];
      if(i%3!==0){''')
put('''  const results=rawDamage.mock.results.filter(x=>x.type==="return").map(x=>(x.value as any).damage),wide=results.filter(x=>x>2147483647).length;
  if(threwCount||wide<100)throw Error(`wide native combat missing: exceptions ${threwCount}, wide ${wide}`);
  console.log(`WIDE_COMBAT raw=${results.length} aboveInt32=${wide} max=${Math.max(...results)}`);''','''  const applied=observed.map(spy=>spy.mock.results.filter(x=>x.type==="return"&&x.value===true).length);
  if(threwCount||applied.some(n=>n<100)||maxHpControls<128)throw Error(`HP cost turns missing: exceptions ${threwCount}, applied ${applied}, max controls ${maxHpControls}`);
  console.log(`HP_COST_TURNS applied=${applied} maxHpControls=${maxHpControls}`);''')
put('  let written = 0;', '  let maxHpControls=0;\n  let written = 0;')
put('    // the parties in field order:', r'''    if(i>=8192){
      for(const p of actors){p.summonData.tags=[];p.level=50;p.calculateStats();p.hp=p.getMaxHp();p.setStat(Stat.HP,i%2?2*p.getMaxHp():1,false);p.summonData.statStages.fill(0);p.customPokemonData.ability=AbilityId.BALL_FETCH;p.summonData.ability=AbilityId.BALL_FETCH;p.moveset=[new PokemonMove(MoveId.SPLASH)];p.summonData.statStages[Stat.SPD-1]=-6;}
      A0.customPokemonData.ability=[AbilityId.BALL_FETCH,AbilityId.SIMPLE,AbilityId.CONTRARY][i%3];A0.summonData.ability=A0.customPokemonData.ability;A0.summonData.statStages[Stat.SPD-1]=6;A0.moveset=[new PokemonMove([MoveId.BELLY_DRUM,MoveId.CLANGOROUS_SOUL,MoveId.FILLET_AWAY][i%3])];
    }
    const appliedBefore=observed.reduce((n,spy)=>n+spy.mock.calls.length,0);
    // the parties in field order:''')
put('    written++;', '    if(i>=8192){if(observed.reduce((n,spy)=>n+spy.mock.calls.length,0)-appliedBefore<1)throw Error("missing real self-cost callback in HP override control");maxHpControls++;}\n    written++;')
put('process.env.DOUBLES_N ?? 8192','process.env.DOUBLES_N ?? 8320')
put('it("wide native combat doubles turns"','it("native HP-cost stat-boost doubles turns"')
put('    if(i>=8192){\n      for','    if(i>=8192&&i<8320){\n      for')
put('    const appliedBefore=',r'''    if(i>=8320){
      game.override.ability(AbilityId.NONE).enemyAbility(AbilityId.NONE);
      for(const p of actors){p.summonData.tags=[];p.level=50;p.calculateStats();p.hp=p.getMaxHp();p.summonData.statStages.fill(0);p.customPokemonData.ability=AbilityId.BALL_FETCH;p.summonData.ability=AbilityId.BALL_FETCH;p.moveset=[new PokemonMove(MoveId.SPLASH)];p.summonData.statStages[Stat.SPD-1]=-6;}
      A0.species=speciesDataRegistry.getSpecies(SpeciesId.SHEDINJA);A0.formIndex=0;A0.customPokemonData.ability=AbilityId.WONDER_GUARD;A0.summonData.ability=AbilityId.WONDER_GUARD;A0.calculateStats();A0.hp=A0.getMaxHp();A0.generateName();A0.summonData.statStages[Stat.SPD-1]=6;A0.moveset=[new PokemonMove(MoveId.BELLY_DRUM)];if(A0.getMaxHp()!==1)throw Error("Wonder Guard HP must be one");
    }
    const appliedBefore=''')
put('let maxHpControls=0;', 'let maxHpControls=0,shedControls=0;')
put('maxHpControls<128)', 'maxHpControls<128||shedControls<128)')
put('maxHpControls++;}', 'if(i<8320)maxHpControls++;else shedControls++;}')
put('maxHpControls=${maxHpControls}`','maxHpControls=${maxHpControls} shedControls=${shedControls}`')
put('process.env.DOUBLES_N ?? 8320','process.env.DOUBLES_N ?? 8448')

out.write_text(s)
