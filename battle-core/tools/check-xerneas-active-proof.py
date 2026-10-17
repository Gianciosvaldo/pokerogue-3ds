#!/usr/bin/env python3
"""Compile real trigger faults and reject malformed native fixtures."""
from pathlib import Path
import argparse,os,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('trace',type=Path);p.add_argument('--turns',type=Path);a=p.parse_args();a.trace=a.trace.resolve()
r=Path(__file__).resolve().parents[2];env=os.environ.copy();assert env.get('TURNTEST_LOCALES')
exe=r/'battle-core/build/xerneas-active-test';assert subprocess.run([exe,a.trace],env=env).returncode==0
s=(r/'battle-core/src/phases.c').read_text();start=s.index('    if (mon->species == SPECIES_ID_XERNEAS)');end=s.index('    } else if (stance)',start);scope=s[start:end]
faults=[('disconnect','mon->species == SPECIES_ID_XERNEAS','false'),('invert-activity','is_active_on_field(mon) ? "active" : "neutral"','is_active_on_field(mon) ? "neutral" : "active"'),('ignore-hp','is_active_on_field(mon) ? "active" : "neutral"','is_on_field(mon) ? "active" : "neutral"'),('no-revert','if (from->form_key && strcmp(from->form_key,key))','if (mon->form==0 && from->form_key && strcmp(from->form_key,key))'),('wrong-trigger','trigger_class == PR_FCT_SpeciesFormChangeActiveTrigger','trigger_class == PR_FCT_SpeciesFormChangeWeatherTrigger')]
flags=['-O2','-std=c99','-ffp-contract=off','-I'+str(r/'battle-core/include'),'-I'+str(r/'battle-core/gen/out')]
with tempfile.TemporaryDirectory(prefix='xerneas-guards-',dir=r/'work') as tmp:
 tmp=Path(tmp)
 for name,old,new in faults:
  assert scope.count(old)==1,(name,scope.count(old));src=tmp/(name+'.c');obj=tmp/(name+'.o');binary=tmp/name;src.write_text(s[:start]+scope.replace(old,new)+s[end:])
  subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
  subprocess.run(['gcc',*flags,r/'battle-core/tests/xerneas-active-test.c',obj,r/'battle-core/build/libprcore.a','-lm','-o',binary],check=True,capture_output=True)
  result=subprocess.run([binary,a.trace],env=env,capture_output=True);assert result.returncode==1,(name,result.returncode,result.stderr[-500:]);print(name,'caught',flush=True)
 if a.turns:
  src_text=(r/'battle-core/src/switching.c').read_text();old='pr_form_change_delayed(bt, in, PR_FCT_SpeciesFormChangeActiveTrigger);';assert src_text.count(old)==2
  for index in range(1):
   offset=src_text.index(old) if index==0 else src_text.rindex(old);src=tmp/'switch.c';obj=tmp/'switch.o';binary=tmp/'switch-test';src.write_text(src_text[:offset]+src_text[offset:].replace(old,old.replace('pr_form_change_delayed','pr_form_change'),1))
   subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
   subprocess.run(['gcc',*flags,r/'battle-core/tests/doublestest.c',obj,r/'battle-core/build/libprcore.a','-lm','-o',binary],check=True,capture_output=True)
   result=subprocess.run([binary,a.turns.resolve()],env=env,capture_output=True);assert result.returncode==1 and b'FAIL CASE' in result.stdout,(index,result.returncode,result.stdout[-500:]);print('summon-order',index,'caught',flush=True)
 rows=a.trace.read_text().splitlines();bad={'missing-footer':rows[:-1],'extra-tail':rows+['JUNK'],'footer':rows[:-1]+['XERNEAS_COMPLETE 1'],'version':[rows[0].replace(' 1 ',' 2 ',1)]+rows[1:]}
 for name,prefix,col,value in [('case','C ',1,'-1'),('battler','C ',2,'4'),('kind','C ',3,'3'),('delay','C ',4,'2'),('species','W ',1,'1'),('id','I ',4,'4294967296'),('hp','I ',7,'-1'),('queue','Q ',1,'33'),('history','F ',1,'17')]:
  index=next(i for i,row in enumerate(rows) if row.startswith(prefix));v=rows[index].split();v[col]=value;changed=rows.copy();changed[index]=' '.join(v);bad[name]=changed
 for prefix in ('Q ','A ','O ','RO ','BO ','F ','T '):
  index=next(i for i,row in enumerate(rows) if row.startswith(prefix));bad['missing-'+prefix.strip()]=rows[:index]+rows[index+1:]
 for name,lines in bad.items():
  path=tmp/'bad.txt';path.write_text('\n'.join(lines)+'\n');result=subprocess.run([exe,path],env=env,capture_output=True);assert result.returncode==2,(name,result.returncode,result.stderr[-300:]);print(name,'rejected',flush=True)
 print(f'{len(faults)} runtime faults caught; {len(bad)} malformed fixtures rejected')
