#!/usr/bin/env python3
"""Compiled Pending Heal faults and strict capture format refusals."""
from pathlib import Path
import argparse,os,subprocess
p=argparse.ArgumentParser();p.add_argument('api',type=Path);p.add_argument('turn',type=Path);p.add_argument('--follow',type=Path);a=p.parse_args();root=Path(__file__).resolve().parents[1];out=root/'build/pending-guards-final';out.mkdir(parents=True,exist_ok=True)
if not os.environ.get('TURNTEST_LOCALES'):raise SystemExit('TURNTEST_LOCALES required')
faults={
 'empty-tag-never-expires':('arena_tags.c','ended[i] = t->n_pending_heals == 0;','ended[i] = false;','pending-heal-test',a.api),
 'duplicate-move-is-queued':('arena_tags.c','t->pending_heals[i].move_id == move_id) return true;','false) return true;','pending-heal-test',a.api),
 'simulation-consumes-effect':('arena_tags.c','if (simulated) return true;','if (false) return true;','pending-heal-test',a.api),
 'ignore-pp-only-need':('arena_tags.c','!can_apply && h.restore_pp && k < PR_N_MOVES(p)','!can_apply && false && k < PR_N_MOVES(p)','pending-heal-test',a.api),
 'never-restore-pp':('arena_tags.c','(h.restore_pp ? PR_HEAL_RESTORE_PP : 0)','0','pending-heal-test',a.api),
 'invalid-source-is-accepted':('arena_tags.c','bool valid_source = pr_pokemon_by_id(bt, h.source_id) != NULL;','bool valid_source = true;','pending-heal-test',a.api),
 'missing-entry-application':('switching.c','pr_arena_apply_pending_heal(bt, e, p, false);','/* entry application removed */','pending-turn-test',a.turn),
 'sacrifice-does-not-store':('move_effects.c','if (a->cls == PR_AC_SacrificialFullRestoreAttr) {','if (false) {','pending-turn-test',a.turn),
}
inc=['-I'+str(root/'include'),'-I'+str(root/'gen/out'),'-I'+str(root/'build'),'-I'+str(root/'tests')]
for name,(file,old,new,test,fixture) in faults.items():
 s=(root/'src'/file).read_text();assert s.count(old)==1,(name,s.count(old));src=out/(name+'.c');obj=out/(name+'.o');exe=out/name;src.write_text(s.replace(old,new));subprocess.run(['gcc','-O2','-std=c99','-ffp-contract=off',*inc,'-c',str(src),'-o',str(obj)],check=True);subprocess.run(['gcc','-O2','-std=c99',*inc,str(root/'tests'/(test+'.c')),str(obj),str(root/'build/libprcore.a'),'-lm','-o',str(exe)],check=True);r=subprocess.run([str(exe),str(fixture.resolve())],capture_output=True,text=True);(out/(name+'.log')).write_text(r.stdout+r.stderr);assert r.returncode==1,(name,r.returncode);print(name,'caught',flush=True)
refusals=0
fixtures=[('pending-heal-test',a.api),('pending-turn-test',a.turn)]
if a.follow:fixtures.append(('pending-turn-test',a.follow))
for test,fixture in fixtures:
 rows=fixture.read_text().splitlines();bad={'missing-footer':rows[:-1],'extra-tail':rows+['JUNK'],'wrong-version':[rows[0].replace(' 2 ',' 99 ',1).replace(' 1 ',' 99 ',1)]+rows[1:]}
 if test=='pending-heal-test':
  ci=next(i for i,x in enumerate(rows)if x.startswith('C '));ri=next(i for i,x in enumerate(rows)if x.startswith('I '));hi=next(i for i,x in enumerate(rows)if x.startswith('H '))
  for name,index,col,value in [('fractional-case',ci,1,'0.5'),('bad-position',ci,2,'4'),('bad-mode',ci,3,'4'),('bad-block',ci,4,'2'),('fractional-hp',ri,1,'0.5'),('bad-heal-move',hi,3,'65535'),('bad-heal-pp',hi,4,'2'),('changed-input-message',hi,5,'00')]:
   r=rows.copy();v=r[index].split();v[col]=value;r[index]=' '.join(v);bad[name]=r
 if a.follow and fixture==a.follow:
  pi=next(i for i,x in enumerate(rows)if x.startswith('PHEAL '))
  for name,replace in [('fractional-target','target=0.5'),('bad-position','target=4'),('bad-pp','pp=2'),('duplicate',None)]:
   r=rows.copy()
   if replace:r[pi]=r[pi].replace(next(t for t in r[pi].split()if t.split('=')[0]==replace.split('=')[0]),replace)
   else:r.insert(pi+1,r[pi])
   bad['follow-'+name]=r
 for name,lines in bad.items():
  f=out/(test+'-'+fixture.stem+'-'+name+'.txt');f.write_text('\n'.join(lines)+'\n');r=subprocess.run([str(root/'build'/test),str(f)],capture_output=True,text=True);assert r.returncode==2,(test,name,r.returncode);refusals+=1
print(f'{len(faults)} compiled faults caught; {refusals} malformed fixtures rejected',flush=True)
