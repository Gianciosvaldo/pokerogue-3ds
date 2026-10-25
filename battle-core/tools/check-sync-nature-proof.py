#!/usr/bin/env python3
"""Compiled faults across actual Synchronize callback, owner and raw/custom setter."""
from pathlib import Path
import os,sys,subprocess,tempfile
r=Path(__file__).resolve().parents[2];raw,prefix=map(lambda p:Path(p).resolve(),sys.argv[1:3]);env=os.environ.copy();assert env.get('TURNTEST_LOCALES');g=r/'game-core';b=r/'battle-core';runner=g/'build/sync_nature_test';host=g/'build/sync_nature_host_test'
for trace in [raw,prefix]:subprocess.run([runner,trace],env=env,check=True)
subprocess.run([host],check=True)
faults=[]
def variants(filename,start,end,changes,trace,kind='raw'):
 s=(r/filename).read_text();a=s.index(start);z=s.index(end,a+len(start));part=s[a:z]
 for name,old,new in changes:
  assert part.count(old)==1,(name,part.count(old));faults.append((name,filename,s[:a]+part.replace(old,new,1)+s[z:],trace,kind))
variants('battle-core/src/ab_attrs.c','    case PR_AC_SyncEncounterNatureAbAttr:','    case PR_AC_DoubleBattleChanceAbAttr:',[
 ('missing-simulated-mutation','        GATE(true);','        GATE(!pp->simulated);'),
 ('wrong-owner-nature','target, p->nature, pp->nature_ctx','target, 0, pp->nature_ctx'),
 ('ignored-custom-scene-setter','if (pp->set_nature)','if (false)'),
 ('missing-default-stats','pr_recalculate_stats(bt, target);','/* missing stats */'),
 ('missing-default-history','        return 1;','        return 0;'),
 ('missing-custom-history','? 1 : -1;','? 0 : -1;')],raw)
variants('game-core/src/gc_encounter.c','bool gc_set_encounter_nature(','static void sync_wild_nature(', [
 ('lost-raw-custom-nature','m->base_nature = (uint8_t)nature;','/* missing underlying nature */'),
 ('overwritten-effective-custom-nature','if (!m->has_custom_nature) p->nature = (uint8_t)nature;','p->nature = (uint8_t)nature;'),
 ('missing-scene-stats','pr_recalculate_stats(bt, p);','/* missing scene stats */'),
 ('ignored-unoverridden-nature','if (!m->has_custom_nature) p->nature = (uint8_t)nature;','if (false) p->nature = (uint8_t)nature;')],raw)
variants('battle-core/src/engine.c','void pr_sync_encounter_nature(','/* NewBiomeEncounterPhase.doEncounter',[
 ('wrong-forward-order','int k = n - 1; k >= 0; k--','int k = 0; k < n; k++'),
 ('only-first-lead','bt->double_battle ? 2 : 1','1'),
 ('wrong-single-field-size','bt->double_battle ? 2 : 1','2'),
 ('forbidden-active-filter','pr_apply_ab_attrs(bt, PR_AC_SyncEncounterNatureAbAttr, &pp);','if(pp.pokemon->on_field&&!pp.pokemon->switch_out_status)pr_apply_ab_attrs(bt, PR_AC_SyncEncounterNatureAbAttr, &pp);'),
 ('lost-custom-owner-setter','.set_nature = setter','.set_nature = NULL')],prefix)
variants('game-core/src/gc_encounter.c','static void sync_wild_nature(','/* newBattle() + EncounterPhase.start()', [
 ('wrong-outgoing-field-size','bt->double_battle = new_double;','bt->double_battle = outgoing_double;'),
 ('missing-passive-refresh','pr_update_passive(bt, p);','p->passive=0;'),
 ('wrong-canonical-party-order','run->party[run->party_order[k]].b','run->party[k].b')],None,'host')
variants('game-core/src/gc_encounter.c','void gc_new_battle(','void gc_new_battle_start(', [('missing-actual-game-owner','        sync_wild_nature(run, nb.double_battle);','        (void)nb.double_battle;')],None,'host')
# Host cases exercise both canonical and swapped party order.
flags=['-O2','-std=c99','-ffp-contract=off',*['-I'+str(r/p) for p in ['battle-core/include','battle-core/gen/out','battle-core/build','run-core/include','game-core/include','game-core/src','ai-core/include','reward-core/include','reward-core/gen/out']]]
with tempfile.TemporaryDirectory(prefix='sync-nature-guards-',dir=r/'work') as tmp:
 d=Path(tmp)
 for name,filename,changed,trace,kind in faults:
  src=d/Path(filename).name;src.write_text(changed);obj=d/'fault.o';exe=d/'fault';subprocess.run(['gcc',*flags,'-c',src,'-o',obj],capture_output=True,check=True)
  args=[g/'tests'/('sync_nature_host_test.c' if kind=='host' else 'sync_nature_test.c'),obj,g/'build/libgc.a',g/'build/reward-core/libreward.a',g/'build/ai-core/libai.a',g/'build/run-core/librun.a',g/'build/run-core/battle-core/libprcore.a']
  if filename.startswith('battle-core/'):
   args[-1]=b/'build/libprcore.a'
  if kind=='host':args+=['-Wl,--wrap=pr_run_new_battle','-Wl,--wrap=gc_mon_new_enemy']
  subprocess.run(['gcc',*flags,*args,'-lm','-o',exe],capture_output=True,check=True)
  p=subprocess.run([exe] if kind=='host' else [exe,trace],env=env,capture_output=True);assert p.returncode==1,(name,p.returncode,p.stdout[-400:],p.stderr[-400:]);print(name,'caught',flush=True)
 data=raw.read_bytes();count=0;bad=d/'bad.txt'
 def reject(name,data):
  global count
  bad.write_bytes(data);p=subprocess.run([runner,bad],env=env,capture_output=True);assert p.returncode==2,(name,p.returncode);count+=1;print(name,'rejected',flush=True)
 def mutation(data,tag,col,value):
  a=data.index(b'\n'+tag)+1;z=data.index(b'\n',a);row=data[a:z].split();row[col]=value;return data[:a]+b' '.join(row)+data[z:]
 reject('version',data.replace(b'SYNC_NATURE 1 ',b'SYNC_NATURE 4 ',1))
 reject('small-count',data.replace(b'102400 ',b'102399 ',1))
 reject('footer',data.rsplit(b'SYNC_NATURE_COMPLETE ',1)[0]+b'SYNC_NATURE_COMPLETE 1\n')
 reject('trailing',data+b'JUNK\n')
 for name,tag,col,value in [('fractional-case',b'C ',1,b'0.5'),('user',b'C ',2,b'8'),('target',b'C ',3,b'8'),('simulation',b'C ',4,b'2'),('setter',b'C ',5,b'2'),('raw-nature',b'NI ',2,b'25'),('custom-nature',b'NI ',3,b'-2'),('effective-nature',b'NI ',4,b'25'),('inconsistent-nature',b'NI ',4,b'1')]:reject(name,mutation(data,tag,col,value))
 data=prefix.read_bytes();reject('prefix-small-count',data.replace(b'1536 ',b'1535 ',1))
 for name,tag,col,value in [('prefix-direct-user',b'C ',2,b'0'),('prefix-simulation',b'C ',4,b'1'),('prefix-field-size',b'C ',6,b'2')]:reject(name,mutation(data,tag,col,value))
 print(f'{len(faults)} compiled faults caught; {count} malformed captures rejected',flush=True)
