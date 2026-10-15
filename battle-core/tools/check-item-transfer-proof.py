#!/usr/bin/env python3
"""Reject runtime transfer faults and malformed actual-source captures."""
from pathlib import Path
import argparse,os,subprocess
p=argparse.ArgumentParser();p.add_argument('trace',type=Path);p.add_argument('turn_end',type=Path);a=p.parse_args()
r=Path(__file__).resolve().parents[2];out=r/'battle-core/build/item-transfer-guards';out.mkdir(parents=True,exist_ok=True)
env=os.environ.copy();assert env.get('TURNTEST_LOCALES')
mutations={
 'skip-sticky':('item_transfer.c','if(cancelled)return false;','cancelled=false;'),
 'wrong-opponent':('item_transfer.c','opponents[pr_battle_rand_seed_int(bt,n,0)]','opponents[0]'),
 'wrong-stock':('item_transfer.c','indices[pr_battle_rand_seed_int(bt,count,0)]','indices[0]'),
 'ignore-lock':('item_transfer.c','&&m->is_transferable)',')'),
 'wrong-cap':('item_transfer.c','matching].stack_count>=cap','matching].stack_count>cap'),
 'skip-source-loss':('item_transfer.c','bt->mods[other][index].stack_count-=taken;','bt->mods[other][index].stack_count-=0;'),
 'skip-unburden':('item_transfer.c','pr_apply_ab_attrs(bt,PR_AC_PostItemLostAbAttr,&lost);','(void)lost;'),
 'skip-stat-refresh':('item_transfer.c','pr_refresh_berry_modifiers(bt,side);','(void)side;'),
 'skip-turn-end':('phases.c','pr_apply_turn_held_item_transfers(bt,e,p);','(void)p;'),
}
for name,(file,old,new) in mutations.items():
 s=(r/'battle-core/src'/file).read_text();assert s.count(old)==1,(name,s.count(old));src=out/(name+'.c');obj=out/(name+'.o');exe=out/name;src.write_text(s.replace(old,new))
 flags=['-O2','-std=c99','-ffp-contract=off','-I'+str(r/'battle-core/include'),'-I'+str(r/'battle-core/gen/out')]
 subprocess.run(['gcc',*flags,'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run(['gcc',*flags,str(r/'battle-core/tests/item-transfer-test.c'),str(obj),str(r/'battle-core/build/libprcore.a'),'-lm','-o',str(exe)],check=True)
 args=[str(exe),str((a.turn_end if name=='skip-turn-end' else a.trace).resolve())]+(['--turn-end'] if name=='skip-turn-end' else [])
 x=subprocess.run(args,env=env,capture_output=True,text=True);(out/(name+'.log')).write_text(x.stdout+x.stderr);assert x.returncode==1,(name,x.returncode);print(name,'caught',flush=True)
rows=a.trace.read_text().splitlines();bad={'missing-footer':rows[:-1],'extra-tail':rows+['JUNK'],'wrong-version':[rows[0].replace(' 1 ',' 2 ',1)]+rows[1:],'wrong-footer':rows[:-1]+['ITEMTRANSFER_COMPLETE 1']}
for name,prefix,col,value in [('fractional-case','C ',1,'0.5'),('bad-holder','C ',2,'4'),('overflow-id','MON ',1,'4294967296'),('bad-level','MON ',4,'256'),('bad-nature','MON ',5,'25'),('unknown-item','M ',1,'UNKNOWN_ITEM'),('bad-owner','M ',3,'4'),('bad-lock','M ',6,'2'),('negative-stack','M ',4,'-1'),('bad-turn','TURN ',1,'0')]:
 i=next(i for i,x in enumerate(rows) if x.startswith(prefix));v=rows[i].split();v[col]=value;t=rows.copy();t[i]=' '.join(v);bad[name]=t
for name,lines in bad.items():
 path=out/(name+'.txt');path.write_text('\n'.join(lines)+'\n');x=subprocess.run([str(r/'battle-core/build/item-transfer-test'),str(path)],env=env,capture_output=True,text=True);assert x.returncode==2,(name,x.returncode);print(name,'rejected',flush=True)
print(f'{len(mutations)} compiled mutants caught; {len(bad)} malformed traces rejected')
