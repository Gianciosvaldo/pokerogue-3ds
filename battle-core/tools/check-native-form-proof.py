#!/usr/bin/env python3
"""Compile native-form faults and reject malformed complete fixtures."""
from pathlib import Path
import argparse,os,subprocess
parser=argparse.ArgumentParser();parser.add_argument('api',type=Path);parser.add_argument('phase',type=Path);parser.add_argument('natural',type=Path);parser.add_argument('aura',type=Path);args=parser.parse_args()
root=Path(__file__).resolve().parents[1];out=root/'build/native-form-guards';out.mkdir(parents=True,exist_ok=True);env=os.environ.copy()
if not env.get('TURNTEST_LOCALES'):raise SystemExit('TURNTEST_LOCALES required')
faults={
 'missing-native-routing':('phases.c','native_ability_form_target(mon,&target_form)','false',args.api,False),
 'missing-schooling':('phases.c','case SPECIES_ID_WISHIWASHI: case SPECIES_ID_MORPEKO:','case SPECIES_ID_MORPEKO:',args.api,False),
 'wrong-minior-offset':('phases.c','p->form<7?p->form+7:p->form-7','p->form<7?p->form+6:p->form-6',args.phase,True),
 'zygarde-keeps-pc-form':('phases.c','to=keys[i^1];','to=keys[i];',args.phase,True),
 'missing-power-construct-condition':('closures_abilities.c','return a->form==4 || a->form==5;','return false;',args.phase,True),
 'power-construct-too-late':('closures_abilities.c','pr_get_hp_ratio(a,false)<=.5','pr_get_hp_ratio(a,false)<.4',args.natural,True),
 'schooling-ignores-level':('closures_abilities.c','a->level < 20 || pr_get_hp_ratio(a, false) <= .25','pr_get_hp_ratio(a, false) <= .25',args.natural,True),
 'missing-ice-face-weather':('closures_abilities.c','{"ability:248", "ea591d93bc79", weather_in}','{"ability:248", "ea591d93bc79", form_zero}',args.api,False),
 'missing-aura-break-condition':('closures_abilities.c','{"ability:188", "0eab2243a547", field_any_aura}','{"ability:188", "0eab2243a547", form_zero}',args.aura,True),
 'missing-aura-break-message':('ab_attrs.c','case ABILITY_ID_AURA_BREAK: key = "abilityTriggers:postSummonAuraBreak"; break;','case ABILITY_ID_AURA_BREAK: return pr_msg0(NULL);',args.aura,True),
}
for name,(file,old,new,fixture,phase) in faults.items():
 s=(root/'src'/file).read_text();assert s.count(old)==1,(name,s.count(old));source=out/(name+'.c');obj=out/(name+'.o');exe=out/name;source.write_text(s.replace(old,new))
 inc=['-I'+str(root/'include'),'-I'+str(root/'gen/out')]
 subprocess.run(['gcc','-O2','-std=c99','-ffp-contract=off',*inc,'-c',str(source),'-o',str(obj)],check=True)
 subprocess.run(['gcc','-O2','-std=c99',*inc,str(root/'tests/native-form-test.c'),str(obj),str(root/'build/libprcore.a'),'-lm','-o',str(exe)],check=True)
 cmd=[str(exe),str(fixture.resolve())]+(['--phase']if phase else[])
 r=subprocess.run(cmd,env=env,capture_output=True,text=True);(out/(name+'.log')).write_text(r.stdout+r.stderr);assert r.returncode==1,(name,r.returncode,r.stdout,r.stderr);print(name,'caught',flush=True)
rows=args.aura.read_text().splitlines();ci=next(i for i,s in enumerate(rows)if s.startswith('C '));ii=next(i for i,s in enumerate(rows)if s.startswith('I '));wi=next(i for i,s in enumerate(rows)if s.startswith('W '));pi=next(i for i,s in enumerate(rows)if s.startswith('P '))
bad={'missing-footer':rows[:-1],'extra-tail':rows+['JUNK'],'wrong-version':['NATIVE_FORM_PHASE 2 8192']+rows[1:],'wrong-footer':rows[:-1]+['NATIVE_FORM_PHASE_COMPLETE 1']}
for name,index,col,value in [('species-overflow',ci,2,'4294967297'),('wrong-species',ci,2,'1'),('bad-mode',ci,3,'4'),('bad-level',ci,4,'256'),('bad-simulation',ci,9,'2'),('bad-gain',ci,10,'2'),('fractional-hp',ii,5,'0.5'),('bad-index',ii,2,'3'),('bad-weather',wi,1,'999'),('bad-ball',wi,2,'9'),('bad-world-ability',wi,3,'999'),('bad-world-flag',wi,6,'2'),('unknown-phase',pi,1,'NothingPhase'),('bad-target',pi,2,'256'),('bad-passive',pi,3,'2'),('bad-text',pi,4,'zz')]:
 r=rows.copy();v=r[index].split();v[col]=value;r[index]=' '.join(v);bad[name]=r
for name,rows in bad.items():
 fixture=out/(name+'.txt');fixture.write_text('\n'.join(rows)+'\n');r=subprocess.run([str(root/'build/native-form-test'),str(fixture),'--phase'],env=env,capture_output=True,text=True);assert r.returncode==2,(name,r.returncode,r.stdout,r.stderr);print(name,'rejected',flush=True)
print(f'{len(faults)} compiled native-form faults caught; {len(bad)} malformed fixtures rejected',flush=True)
