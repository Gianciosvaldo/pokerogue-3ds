#!/usr/bin/env python3
"""Compiled lifecycle faults and complete-fixture transport refusals."""
from pathlib import Path
import argparse,os,subprocess
p=argparse.ArgumentParser();p.add_argument('gas',type=Path);p.add_argument('bulk',type=Path);p.add_argument('imprison',type=Path);p.add_argument('cleanup',type=Path);a=p.parse_args()
root=Path(__file__).resolve().parents[1];out=root/'build/arena-guards';out.mkdir(parents=True,exist_ok=True)
if not os.environ.get('TURNTEST_LOCALES'):raise SystemExit('TURNTEST_LOCALES required')
faults={
 'wrong-gas-constructor':('arena_tags.c','t->source_count=1; break;','t->source_count=0; break;','gas-lifecycle-test',a.gas,[]),
 'missing-gas-removal-flag':('arena_tags.c','if(stored)stored->being_removed=true;','if(stored)stored->being_removed=false;','gas-lifecycle-test',a.gas,[]),
 'primary-before-passive':('arena_tags.c','for(int passive=1;passive>=0;passive--)','for(int passive=0;passive<=1;passive++)','gas-lifecycle-test',a.gas,[]),
 'missing-last-gas-reactivation':('arena_tags.c','if(gas->source_count!=1)return;','return;','gas-lifecycle-test',a.gas,[]),
 'bulk-drops-callbacks':('arena_tags.c','for(int i=0;i<bt->n_arena_tags;i++)on_remove(bt,e,&bt->arena_tags[i],false);','/* callbacks removed */','arena-remove-all-test',a.bulk,[]),
 'bulk-quiet-messages':('arena_tags.c','for(int i=0;i<bt->n_arena_tags;i++)on_remove(bt,e,&bt->arena_tags[i],false);','for(int i=0;i<bt->n_arena_tags;i++)on_remove(bt,e,&bt->arena_tags[i],true);','arena-remove-all-test',a.bulk,[]),
 'imprison-restricts-permanent-moves':('commands.c','return src && in_moveset(src, move)','return src && src->moves[0]==move','imprison-test',a.imprison,[]),
 'imprison-counts-down':('battler_tags.c','return active_on_field(source);','return --t->turn_count>0 && active_on_field(source);','imprison-test',a.imprison,[]),
 'imprison-no-cancellation':('battler_tags.c','if(pr_tag_restricts_move(bt,p,t,move))','if(false)','imprison-test',a.imprison,[]),
 'cleanup-drops-tag-callbacks':('engine.c','    pr_arena_remove_all_tags(bt, e);','    bt->n_arena_tags=0;','reset-arena-test',a.cleanup,['--effects']),
 'cleanup-clears-permanent-weather':('engine.c','if (bt->weather_turns_left != 0)','if (true)','reset-arena-test',a.cleanup,['--effects']),
 'cleanup-keeps-faint-count':('engine.c','    bt->player_faints = 0;','    /* old faint count retained */','reset-arena-test',a.cleanup,['--effects']),
}
inc=['-I'+str(root/'include'),'-I'+str(root/'gen/out')]
for name,(file,old,new,test,fixture,options) in faults.items():
 s=(root/'src'/file).read_text();assert s.count(old)==1,(name,s.count(old));src=out/(name+'.c');obj=out/(name+'.o');exe=out/name;src.write_text(s.replace(old,new))
 subprocess.run(['gcc','-O2','-std=c99','-ffp-contract=off',*inc,'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run(['gcc','-O2','-std=c99',*inc,str(root/'tests'/(test+'.c')),str(obj),str(root/'build/libprcore.a'),'-lm','-o',str(exe)],check=True)
 r=subprocess.run([str(exe),str(fixture.resolve()),*options],capture_output=True,text=True);(out/(name+'.log')).write_text(r.stdout+r.stderr);assert r.returncode==1,(name,r.returncode);print(name,'caught',flush=True)
refusals=0
for test,fixture,options in [('gas-lifecycle-test',a.gas,[]),('arena-remove-all-test',a.bulk,[]),('imprison-test',a.imprison,[]),('reset-arena-test',a.cleanup,['--effects'])]:
 rows=fixture.read_text().splitlines()
 variants={'missing-footer':rows[:-1],'extra-tail':rows+['JUNK'],'bad-version':[rows[0].replace(' 1 ',' 999 ',1).replace(' 2 ',' 999 ',1)]+rows[1:]}
 ci=next(i for i,s in enumerate(rows)if s.startswith('C '));r=rows.copy();r[ci]=r[ci].replace('C 0 ','C 0.5 ',1);variants['fractional-case']=r
 for name,lines in variants.items():
  bad=out/(test+'-'+name+'.txt');bad.write_text('\n'.join(lines)+'\n');r=subprocess.run([str(root/'build'/test),str(bad),*options],capture_output=True,text=True);assert r.returncode==2,(test,name,r.returncode);refusals+=1
print(f'{len(faults)} compiled faults caught; {refusals} malformed fixtures rejected',flush=True)
