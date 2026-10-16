#!/usr/bin/env python3
"""Compiled faults and strict malformed-input controls; production files are untouched."""
from pathlib import Path
import argparse,os,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('trace',type=Path);a=p.parse_args();a.trace=a.trace.resolve()
r=Path(__file__).resolve().parents[2];env=os.environ.copy();assert env.get('TURNTEST_LOCALES')
runner=r/'battle-core/build/move-theft-test'
assert subprocess.run([runner,a.trace],env=env).returncode==0
faults=[
 ('chance','item_transfer.c','pr_rng_seed_float(&bt->rng)>chance','pr_rng_seed_float(&bt->rng)>0'),
 ('global-rng','item_transfer.c','pr_rng_seed_float(&bt->rng)>chance','0>chance'),
 ('cached-tier','item_transfer.c','if (m->type_tier > 1 && m->type_tier <= 6) return m->type_tier - 1;','if (false) return m->type_tier - 1;'),
 ('trainer-tier','item_transfer.c','tier = trainer ? 4 : 3','tier = 3'),
 ('lock','item_transfer.c','&& m->is_transferable) indices[count++]=k;',' ) indices[count++]=k;'),
 ('highest-tier','item_transfer.c','if (tiers[k]==highest) chosen[n++]=indices[k];','if (true) chosen[n++]=indices[k];'),
 ('tie-rng','item_transfer.c','chosen[pr_battle_rand_seed_int(bt,n,0)]','chosen[0]'),
 ('cross-side','item_transfer.c','if (source && source->is_player != receiver->is_player) {','if (source) {'),
 ('sticky','item_transfer.c','if (cancelled) return false;','if (false) return false;'),
 ('cap','item_transfer.c','existing>=cap','existing>cap'),
 ('donor-stack','item_transfer.c','bt->mods[from_side][index].stack_count-=taken;','bt->mods[from_side][index].stack_count-=0;'),
 ('same-side-index','item_transfer.c','if (from_side==side && matching>index) matching--;','if (false) matching--;'),
 ('receiver-stack','item_transfer.c','copy.stack_count=existing+taken;','copy.stack_count=taken;'),
 ('stat-refresh','item_transfer.c','    pr_refresh_berry_modifiers(bt,side);\n    if (source) {','    /* missing refresh */\n    if (source) {'),
 ('unburden','item_transfer.c','pr_apply_ab_attrs(bt,PR_AC_PostItemLostAbAttr,&lost);','(void)lost;'),
 ('user-hp','item_transfer.c','if (pr_rng_seed_float(&bt->rng)>chance) return false;','if (pr_rng_seed_float(&bt->rng)>chance || user->hp<=0) return false;'),
 ('dispatch','move_effects.c','pr_steal_move_held_item(bt, e, user, target, P(StealHeldItemChanceAttr, a)->chance);','(void)target;'),
]
flags=['-O2','-std=c99','-ffp-contract=off','-I'+str(r/'battle-core/include'),'-I'+str(r/'battle-core/gen/out')]
with tempfile.TemporaryDirectory(prefix='theft-guards-',dir=r/'work')as tmp:
 tmp=Path(tmp)
 for name,file,old,new in faults:
  s=(r/'battle-core/src'/file).read_text()
  if file=='item_transfer.c':
   cut=s.index('/* getOrInferTier caches positive tiers.');prefix=s[:cut];scope=s[cut:]
  else:prefix='';scope=s
  assert scope.count(old)==1,(name,scope.count(old))
  source=tmp/(name+'.c');obj=tmp/(name+'.o');binary=tmp/name;source.write_text(prefix+scope.replace(old,new))
  subprocess.run(['gcc',*flags,'-c',source,'-o',obj],check=True,capture_output=True)
  subprocess.run(['gcc',*flags,r/'battle-core/tests/move-theft-test.c',obj,r/'battle-core/build/libprcore.a','-lm','-o',binary],check=True,capture_output=True)
  x=subprocess.run([binary,a.trace],env=env,capture_output=True);assert x.returncode==1,(name,x.returncode,x.stdout,x.stderr)
  print(name,'caught',flush=True)
 rows=a.trace.read_text().splitlines();bad={'missing-footer':rows[:-1],'extra-tail':rows+['JUNK'],'version':[rows[0].replace(' 1 ',' 2 ',1)]+rows[1:],'footer':rows[:-1]+['MOVETHEFT_COMPLETE 1']}
 for name,prefix,col,value in [('fractional-case','C ',1,'0.5'),('holder','C ',2,'4'),('target','C ',3,'4'),('mode','C ',4,'2'),('move','C ',5,'0'),('result','C ',6,'2'),('id','MON ',1,'4294967296'),('nature','MON ',5,'25'),('trainer','MON ',8,'2'),('unknown-item','M ',1,'UNKNOWN'),('owner','M ',3,'4'),('lock','M ',6,'2'),('negative-stack','M ',4,'-1'),('tier','M ',8,'6'),('turn','TURN ',1,'0')]:
  index=next(i for i,row in enumerate(rows)if row.startswith(prefix));v=rows[index].split();v[col]=value;malformed=rows.copy();malformed[index]=' '.join(v);bad[name]=malformed
 for name,lines in bad.items():
  path=tmp/'bad.txt';path.write_text('\n'.join(lines)+'\n');x=subprocess.run([runner,path],env=env,capture_output=True);assert x.returncode==2,(name,x.returncode)
  print(name,'rejected',flush=True)
 print(f'{len(faults)} compiled runtime faults caught; {len(bad)} malformed captures rejected')
