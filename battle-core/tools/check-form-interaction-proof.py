#!/usr/bin/env python3
"""Mutation and complete transport checks for the transformed-ability gate."""
from pathlib import Path
import argparse,os,subprocess
p=argparse.ArgumentParser();p.add_argument('api',type=Path);p.add_argument('turn',type=Path);a=p.parse_args();root=Path(__file__).resolve().parents[1];out=root/'build/form-interaction-guards';out.mkdir(parents=True,exist_ok=True)
if not os.environ.get('TURNTEST_LOCALES'):raise SystemExit('TURNTEST_LOCALES required')
s=(root/'src/ab_attrs.c').read_text();old='if (p->transformed && pr_ability_first_attr(ability, PR_AC_NoTransformAbilityAbAttr)) return false;';assert s.count(old)==1
faults={
 'missing-no-transform-gate':('', 'form-interaction-test',a.api),
 'all-transformed-abilities-blocked':('if (p->transformed) return false;', 'form-interaction-test',a.api),
 'untransformed-natives-blocked':('if (pr_ability_first_attr(ability, PR_AC_NoTransformAbilityAbAttr)) return false;', 'form-interaction-test',a.api),
 'only-disguise-blocked':('if (p->transformed && ability==ABILITY_ID_DISGUISE) return false;', 'form-interaction-test',a.api),
 'gate-reads-permanent-ability':('if (p->transformed && pr_ability_first_attr(p->base_ability, PR_AC_NoTransformAbilityAbAttr)) return false;', 'form-interaction-test',a.api),
 'missing-damage-restriction':('', 'form-interaction-turn-test',a.turn),
}
inc=['-I'+str(root/'include'),'-I'+str(root/'gen/out'),'-I'+str(root/'build'),'-I'+str(root/'tests')]
for name,(new,test,fixture) in faults.items():
 src=out/(name+'.c');obj=out/(name+'.o');exe=out/name;src.write_text(s.replace(old,new));subprocess.run(['gcc','-O2','-std=c99','-ffp-contract=off',*inc,'-c',str(src),'-o',str(obj)],check=True);subprocess.run(['gcc','-O2','-std=c99',*inc,str(root/'tests'/(test+'.c')),str(obj),str(root/'build/libprcore.a'),'-lm','-o',str(exe)],check=True);r=subprocess.run([str(exe),str(fixture.resolve())],capture_output=True,text=True);(out/(name+'.log')).write_text(r.stdout+r.stderr);assert r.returncode==1,(name,r.returncode);print(name,'caught',flush=True)
refusals=0
for test,fixture in [('form-interaction-test',a.api),('form-interaction-turn-test',a.turn)]:
 rows=fixture.read_text().splitlines();bad={'missing-footer':rows[:-1],'extra-tail':rows+['JUNK'],'bad-version':[rows[0].replace(' 1 ',' 99 ',1)]+rows[1:]}
 if test=='form-interaction-test':
  ci=next(i for i,x in enumerate(rows)if x.startswith('C '));ii=next(i for i,x in enumerate(rows)if x.startswith('I '));qi=next(i for i,x in enumerate(rows)if x.startswith('Q '))
  for name,index,col,value in [('fractional-case',ci,1,'0.5'),('bad-battler',ci,2,'4'),('bad-simulation',ci,3,'2'),('bad-level',ii,6,'256'),('fractional-hp',ii,8,'0.5'),('bad-transform-flag',ii,64,'2'),('bad-deferred',qi,4,'2')]:
   r=rows.copy();v=r[index].split();v[col]=value;r[index]=' '.join(v);bad[name]=r
 for name,lines in bad.items():
  f=out/(test+'-'+name+'.txt');f.write_text('\n'.join(lines)+'\n');r=subprocess.run([str(root/'build'/test),str(f)],capture_output=True,text=True);assert r.returncode==2,(test,name,r.returncode);refusals+=1
print(f'{len(faults)} compiled faults caught; {refusals} malformed fixtures rejected',flush=True)
