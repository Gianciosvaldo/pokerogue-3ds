#!/usr/bin/env python3
"""Compile queue faults and reject malformed actual-source captures."""
from pathlib import Path
import argparse,os,subprocess
p=argparse.ArgumentParser();p.add_argument('end',type=Path);p.add_argument('trainer',type=Path);a=p.parse_args();r=Path(__file__).resolve().parents[2];out=r/'battle-core/build/encounter-end-guards';out.mkdir(parents=True,exist_ok=True)
mutations={
 'skip-player-second':('if(!allowed[1]->on_field||allowed[1]->switch_out_status){','if(false){',False),
 'return-ally-in-double':('if(bt->double_battle){','if(false){',False),
 'ignore-loaded':('if(!loaded){','if(true){',False),
 'wrong-double-switch-threshold':('&&n_allowed>(bt->double_battle?2:1))','&&n_allowed>1)',False),
 'skip-second-switch':('for(int k=0;k<(bt->double_battle?2:1);k++){','for(int k=0;k<1;k++){',False),
 'skip-trainer-second':('k<(bt->double_battle&&legal>1?2:1)','k<1',True),
 'trainer-second-when-fainted':('bt->double_battle&&legal>1?2:1','bt->double_battle?2:1',True),
 'wrong-switch-name':('e->pool[s].sw_use_name=bt->double_battle;','e->pool[s].sw_use_name=false;',False),
 'wrong-position-argument':('e->pool[t].position_double=true;','e->pool[t].position_double=false;',False),
}
s=(r/'battle-core/src/engine.c').read_text()
for name,(old,new,trainer) in mutations.items():
 assert s.count(old)==1,(name,s.count(old));src=out/(name+'.c');obj=out/(name+'.o');exe=out/name;src.write_text(s.replace(old,new));flags=['-O2','-std=c99','-ffp-contract=off','-I'+str(r/'battle-core/include'),'-I'+str(r/'battle-core/gen/out')];subprocess.run(['gcc',*flags,'-c',str(src),'-o',str(obj)],check=True);subprocess.run(['gcc',*flags,str(r/'battle-core/tests/encounter-end-test.c'),str(obj),str(r/'battle-core/build/libprcore.a'),'-lm','-o',str(exe)],check=True);x=subprocess.run([str(exe),str((a.trainer if trainer else a.end).resolve())],capture_output=True,text=True);(out/(name+'.log')).write_text(x.stdout+x.stderr);assert x.returncode==1,(name,x.returncode);print(name,'caught',flush=True)
rows=a.end.read_text().splitlines();bad={'missing-footer':rows[:-1],'extra-tail':rows+['JUNK'],'wrong-version':[rows[0].replace(' 2 ',' 3 ',1)]+rows[1:],'wrong-footer':rows[:-1]+['ENCOUNTEREND_COMPLETE 1']}
for name,prefix,col,value in [('fractional-case','C ',1,'0.5'),('invalid-double','C ',2,'2'),('no-party','C ',4,'0'),('invalid-mode','C ',6,'5'),('invalid-wave','C ',8,'0'),('negative-hp','P ',1,'-1'),('invalid-field','P ',2,'2'),('unknown-phase','Q ',1,'UnknownPhase'),('invalid-level','L ',2,'99999'),('invalid-dynamic-count','DYNTYPES ',1,'99999')]:
 i=next(i for i,v in enumerate(rows) if v.startswith(prefix));v=rows[i].split();v[col]=value;line=rows.copy();line[i]=' '.join(v);bad[name]=line
for name,line in bad.items():
 path=out/(name+'.txt');path.write_text('\n'.join(line)+'\n');x=subprocess.run([str(r/'battle-core/build/encounter-end-test'),str(path)],capture_output=True,text=True);assert x.returncode==2,(name,x.returncode);print(name,'rejected',flush=True)
print(f'{len(mutations)} compiled mutants caught; {len(bad)} malformed captures rejected')
