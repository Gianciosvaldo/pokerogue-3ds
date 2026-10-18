#!/usr/bin/env python3
"""Real biome/terrain, chance, child and owning dispatch faults."""
from pathlib import Path
import os,sys,subprocess,tempfile
r=Path(__file__).resolve().parents[2];trace=Path(sys.argv[1]).resolve();turns=Path(sys.argv[2]).resolve();env=os.environ.copy();assert env.get('TURNTEST_LOCALES')
runner=r/'battle-core/build/secret-power-test';subprocess.run([runner,trace],check=True,env=env);subprocess.run([r/'battle-core/build/wide-combat-test',turns],check=True,env=env)
s=(r/'battle-core/src/move_effects.c').read_text();a=s.index('    case PR_AC_SecretPowerAttr:');b=s.index('    case PR_AC_StatStageChangeAttr:',a);leaf=s[a:b];faults=[]
def fault(name,old,new,count=1):
 assert leaf.count(old)==count,(name,leaf.count(old));faults.append((name,'move_effects.c',s[:a]+leaf.replace(old,new,count)+s[b:],False))
fault('missing-outer-hp-gate','if (!effect_can_apply(ef, user, target)) return;','/* no outer gate */')
fault('extra-user-hp-gate','if (!effect_can_apply(ef, user, target)) return;','if (!effect_can_apply(ef,user,target)||user->hp<=0) return;')
fault('double-chance','int effect = STATUS_EFFECT_PARALYSIS;','if (!chance_roll(bt,get_move_chance(bt,ef,user,target,move,false,true))) return;\n        int effect = STATUS_EFFECT_PARALYSIS;')
fault('ignore-terrain','if (bt->terrain != TERRAIN_NONE)','if (false)')
fault('paralysis-mapping','int effect = STATUS_EFFECT_PARALYSIS;','int effect = STATUS_EFFECT_POISON;')
fault('sleep-mapping','effect = STATUS_EFFECT_SLEEP','effect = STATUS_EFFECT_PARALYSIS',2)
fault('freeze-mapping','effect = STATUS_EFFECT_FREEZE','effect = STATUS_EFFECT_PARALYSIS')
fault('burn-mapping','effect = STATUS_EFFECT_BURN','effect = STATUS_EFFECT_PARALYSIS')
fault('misty-stat','case TERRAIN_MISTY: stat = STAT_SPATK;','case TERRAIN_MISTY: stat = STAT_DEF;')
fault('psychic-stat','case TERRAIN_PSYCHIC: stat = STAT_SPD;','case TERRAIN_PSYCHIC: stat = STAT_ATK;')
fault('speed-biomes','stat = STAT_SPD; break;\n            case BIOME_ID_ICE_CAVE','stat = STAT_ATK; break;\n            case BIOME_ID_ICE_CAVE')
fault('fairy-cave-stat','case BIOME_ID_FAIRY_CAVE: stat = STAT_SPATK;','case BIOME_ID_FAIRY_CAVE: stat = STAT_DEF;')
fault('accuracy-biomes','case BIOME_ID_ISLAND: case BIOME_ID_BADLANDS: stat = STAT_ACC;','case BIOME_ID_ISLAND: case BIOME_ID_BADLANDS: stat = STAT_ATK;')
fault('attack-biomes','case BIOME_ID_SEA: case BIOME_ID_LAKE: case BIOME_ID_SEABED: stat = STAT_ATK;','case BIOME_ID_SEA: case BIOME_ID_LAKE: case BIOME_ID_SEABED: stat = STAT_SPD;')
fault('end-biome','case BIOME_ID_END: stat = STAT_DEF;','case BIOME_ID_END: stat = STAT_SPATK;')
fault('missing-flinch','flinch = true;','flinch = false;')
fault('wrong-tag','BATTLER_TAG_TYPE_FLINCHED','BATTLER_TAG_TYPE_CONFUSED')
fault('wrong-stages','&stat, 1, -1','&stat, 1, 1')
fault('wrong-stat-source','PR_AC_StatStageChangeAttr, user, target, move','PR_AC_StatStageChangeAttr, target, target, move')
fault('wrong-child-self-target','.chance_override = -1, .condition = PR_NO_FN','.self_target = true, .chance_override = -1, .condition = PR_NO_FN')
fault('force-child-chance','.chance_override = -1, .condition = PR_NO_FN','.chance_override = 100, .condition = PR_NO_FN')
fault('overlap-gate','const pr_attr_AddBattlerTagAttr tag =','if (pr_get_battler_tag(target,BATTLER_TAG_TYPE_FLINCHED)) return;\n            const pr_attr_AddBattlerTagAttr tag =')
for name,old,new in [('rainbow-boost','&& !pr_move_has_attr((uint16_t)move, PR_AC_SecretPowerAttr)','&& true'),('skip-flinch-duration-rng','int turns = pr_battle_rand_seed_int(bt, x->turnCountMax - x->turnCountMin + 1, x->turnCountMin);','int turns = move==MOVE_ID_SECRET_POWER ? 0 : pr_battle_rand_seed_int(bt,x->turnCountMax-x->turnCountMin+1,x->turnCountMin);')]:
 assert s.count(old)==1,(name,s.count(old));faults.append((name,'move_effects.c',s.replace(old,new,1),False))
a=s.index('static bool status_effect_apply(');b=s.index('static void unshift_pokemon_phase(',a);part=s[a:b]
for name,old,new in [('wrong-status-recipient','pr_try_set_status(bt, e, target, effect, user','pr_try_set_status(bt, e, user, effect, user'),('wrong-status-source','target, effect, user, -1','target, effect, target, -1'),('noisy-status','pr_moves[move].category != MOVE_CATEGORY_STATUS','false')]:
 assert part.count(old)==1,(name,part.count(old));faults.append((name,'move_effects.c',s[:a]+part.replace(old,new,1)+s[b:],False))
p=(r/'battle-core/src/phases.c').read_text();old='if (!pr_attr_is(pr_attrs[ai].cls, PR_AC_MoveEffectAttr)) continue;';assert p.count(old)==1;faults.append(('missing-owning-callback','phases.c',p.replace(old,old+'\n        if (pr_attrs[ai].cls==PR_AC_SecretPowerAttr) continue;',1),True))
flags=['-O2','-g','-std=c99','-ffp-contract=off',*['-I'+str(r/x) for x in ['battle-core/include','battle-core/gen/out','battle-core/build','battle-core/tests']]]
with tempfile.TemporaryDirectory(prefix='secret-power-guards-',dir=r/'work') as tmp:
 d=Path(tmp)
 for name,file,changed,own in faults:
  src=d/'fault.c';src.write_text(changed);obj=d/'fault.o';exe=d/'fault';subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
  test=r/('battle-core/build/wide-combat-replay.c' if own else 'battle-core/tests/secret-power-test.c');subprocess.run(['gcc',*flags,test,obj,r/'battle-core/build/libprcore.a',*([] if own else ['-Wl,--wrap=pr_battle_rand_seed_int']),'-lm','-o',exe],check=True,capture_output=True)
  result=subprocess.run([exe,turns if own else trace],env=env,capture_output=True);assert result.returncode==1,(name,result.returncode,result.stdout[-400:],result.stderr[-400:]);print(name,'caught',flush=True)
 rows=trace.read_text().splitlines();bad={'footer':rows[:-1]+['SECRET_POWER_COMPLETE 1'],'missing-footer':rows[:-1],'tail':rows+['JUNK'],'version':['SECRET_POWER 3 12288 777']+rows[1:]}
 for name,prefix,col,value in [('case','C ',1,'0.5'),('user','C ',2,'4'),('target','C ',3,'4'),('biome','WORLD ',1,'51'),('terrain','WORLD ',2,'5'),('arena-tag','AI ',2,'INVALID'),('id','I ',5,'4294967296'),('level','I ',6,'2147483648'),('nan','I ',26,'NaN'),('context','QIN ',2,'2'),('pending','SI ',6,'8'),('tag','TI ',3,'INVALID'),('return','RET ',1,'2'),('queue','QCOUNT ',1,'17'),('dyn-count','DCOUNT ',1,'5'),('stat','CHANGE ',1,'8')]:
  ix=next(i for i,row in enumerate(rows) if row.startswith(prefix));row=rows[ix].split();row[col]=value;changed=rows.copy();changed[ix]=' '.join(row);bad[name]=changed
 for name,changed in bad.items():
  f=d/'bad.txt';f.write_text('\n'.join(changed)+'\n');result=subprocess.run([runner,f],env=env,capture_output=True);assert result.returncode==2,(name,result.returncode);print(name,'rejected',flush=True)
 print(f'{len(faults)} runtime faults caught; {len(bad)} malformed captures rejected')
