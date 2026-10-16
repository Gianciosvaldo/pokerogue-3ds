#!/usr/bin/env python3
"""Require the real owning-summon capture to catch runtime faults."""
from pathlib import Path
import argparse,os,subprocess
p=argparse.ArgumentParser();p.add_argument('trace',type=Path);a=p.parse_args();r=Path(__file__).resolve().parents[2];out=r/'battle-core/build/summon-entry-guards';out.mkdir(parents=True,exist_ok=True);env=os.environ.copy();assert env.get('TURNTEST_LOCALES')
s=(r/'battle-core/src/switching.c').read_text();start=s.index('void pr_summon_phase(');end=s.index('/* InitEncounterPhase:',start);chunk=s[start:end]
mutations={
 'stale-ally-reference':('for (int k=0;k<(bt->double_battle?2:1);k++)\n            bt->field[side*2+k]=k<bt->n_party[side]?bt->party[side][k]:NULL;','(void)side;'),
 'skip-shiny':('if (p->shiny) pr_unshift_new(bt, e, PH_SHINY_SPARKLE, b);','(void)b;'),
 'skip-turn-reset':('pr_reset_turn_data(p);','(void)p;'),
 'skip-stat-refresh':('for (int k = 0; k < bt->n_party[side]; k++) pr_recalculate_stats(bt, bt->party[side][k]);','(void)side;'),
 'wrong-field-slot':('int b = side * 2 + field;','int b = side * 2;'),
 'inactive-summon':('p->on_field = true;','p->on_field = false;'),
 'skip-summoned-flag':('p->summoned_this_turn = true;','p->summoned_this_turn = false;'),
 'lose-fainted-party-member':('bt->party[side][legal] = p;','(void)p;'),
 'wrong-trainer-text':('bool player = s->sw_player;','bool player = true;'),
}
for name,(old,new) in mutations.items():
 assert chunk.count(old)==1,(name,chunk.count(old));src=out/(name+'.c');obj=out/(name+'.o');exe=out/name;src.write_text(s[:start]+chunk.replace(old,new)+s[end:]);flags=['-O2','-std=c99','-ffp-contract=off','-I'+str(r/'battle-core/include'),'-I'+str(r/'battle-core/gen/out')];subprocess.run(['gcc',*flags,'-c',str(src),'-o',str(obj)],check=True);subprocess.run(['gcc',*flags,str(r/'battle-core/tests/summon-entry-test.c'),str(obj),str(r/'battle-core/build/libprcore.a'),'-lm','-o',str(exe)],check=True);x=subprocess.run([str(exe),str(a.trace.resolve())],env=env,capture_output=True,text=True);(out/(name+'.log')).write_text(x.stdout+x.stderr);assert x.returncode==1,(name,x.returncode);print(name,'caught',flush=True)
rows=a.trace.read_text().splitlines();bad={'missing-footer':rows[:-1],'extra-tail':rows+['JUNK'],'wrong-version':[rows[0].replace(' 3 ',' 4 ',1)]+rows[1:],'wrong-footer':rows[:-1]+['SUMMONENTRY_COMPLETE 1']}
for name,prefix,col,value in [('fractional-case','C ',1,'0.5'),('invalid-double','C ',2,'2'),('invalid-side','C ',3,'2'),('single-second-slot','C ',4,'1'),('overflow-id','MON ',1,'4294967296'),('invalid-level','MON ',4,'256'),('invalid-nature','MON ',5,'25'),('invalid-shiny','MON ',13,'2'),('invalid-iv','MON ',15,'32'),('unknown-phase','Q ',1,'UnknownPhase')]:
 i=next(i for i,v in enumerate(rows) if v.startswith(prefix));v=rows[i].split();v[col]=value;line=rows.copy();line[i]=' '.join(v);bad[name]=line
name=next(i for i,v in enumerate(rows) if v.startswith('NAME '));bad['bad-name-hex']=rows.copy();bad['bad-name-hex'][name]='NAME zz'
for name,line in bad.items():
 path=out/(name+'.txt');path.write_text('\n'.join(line)+'\n');x=subprocess.run([str(r/'battle-core/build/summon-entry-test'),str(path)],env=env,capture_output=True,text=True);assert x.returncode==2,(name,x.returncode);print(name,'rejected',flush=True)
print(f'{len(mutations)} compiled mutants caught; {len(bad)} malformed captures rejected')
