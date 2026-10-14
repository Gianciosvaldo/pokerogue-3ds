#!/usr/bin/env python3
"""Actual native form turns with total/flat effects before Spliced halving."""
from pathlib import Path
import subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
 base=Path(tmp)/'base';subprocess.run([sys.executable,str(root/'tools/build-native-form-turn-proof.py'),sys.argv[1],str(base)],check=True);s=base.read_text()
if sys.argv[1]=='producer':
 s=s.replace('NATIVEFORMTURN','STATORDERTURN').replace('actual native form full move turns','actual stat ordering full move turns')
 s=s.replace('import { BattleScene }','import { getGameMode } from "#app/game-mode";\nimport { GameModes } from "#enums/game-modes";\nimport { BaseStatModifier,PokemonBaseStatTotalModifier,PokemonBaseStatFlatModifier } from "#modifiers/modifier";\nimport { BattleScene }')
 needle='        const max=holder.getMaxHp(),hp='
 assert s.count(needle)==1;s=s.replace(needle,'''        scene.gameMode=getGameMode(state>=8?GameModes.SPLICED_ENDLESS:GameModes.CLASSIC);
        const mt=modifierTypes.LEFTOVERS(),mods:any[]=[];
        mods.push(new PokemonBaseStatTotalModifier(mt,holder.id,state%2?10:-15,1));
        if(state%3!==0)mods.push(new PokemonBaseStatFlatModifier(mt,holder.id,1));
        if(state%3===2)mods.push(new BaseStatModifier(mt,holder.id,Stat.ATK,1));
        holder.ivs.fill(15);if(holder.isPlayer())scene.modifiers=mods;else scene.enemyModifiers=mods;
        for(const q of [...scene.getPlayerParty(),...scene.getEnemyParty()]){q.calculateStats();q.hp=q.getMaxHp();}
'''+needle)
 needle='`MOD side=${side} cls=${m.constructor.name} pid=${m.pokemonId ?? 0} stack=${m.stackCount}`'
 assert s.count(needle)==1;s=s.replace(needle,needle+''' + (["BaseStatModifier","PokemonBaseStatTotalModifier","PokemonBaseStatFlatModifier"].includes(m.constructor.name)?` virtual=${m.virtualStackCount} num=${m.statModifier??0} stat=${m.stat??0}`:"")''')
elif sys.argv[1]=='replay':
 needle='            m->is_transferable = true;'
 assert s.count(needle)==1;s=s.replace(needle,needle+'''
            if(m->cls==PR_MOD_BASE_STAT||m->cls==PR_MOD_BASE_STAT_TOTAL||m->cls==PR_MOD_BASE_STAT_FLAT){
                m->virtual_stack_count=inum(l,"virtual");m->num=inum(l,"num");m->stat=inum(l,"stat");m->is_transferable=false;
            }
''')
 needle='            else\n                add_line(L, "MOD side=%d cls=%s pid=%lld stack=%d", side,'
 assert s.count(needle)==1;s=s.replace(needle,'''            else if(m->cls==PR_MOD_BASE_STAT||m->cls==PR_MOD_BASE_STAT_TOTAL||m->cls==PR_MOD_BASE_STAT_FLAT)
                add_line(L,"MOD side=%d cls=%s pid=%lld stack=%d virtual=%d num=%d stat=%d",side,pr_mod_class_name[m->cls],(long long)m->pokemon_id,m->stack_count,m->virtual_stack_count,m->num,m->stat);
'''+needle)
else:raise SystemExit('producer or replay required')
Path(sys.argv[2]).write_text(s)
