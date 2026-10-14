#!/usr/bin/env python3
"""Compiled stat-order faults and malformed complete capture guards."""
from pathlib import Path
import argparse,os,subprocess
p=argparse.ArgumentParser();p.add_argument('api',type=Path);p.add_argument('turn',type=Path);a=p.parse_args();root=Path(__file__).resolve().parents[1];out=root/'build/stat-order-guards';out.mkdir(parents=True,exist_ok=True)
if not os.environ.get('TURNTEST_LOCALES'):raise SystemExit('TURNTEST_LOCALES required')
faults={
 'total-flat-after-fusion':('pokemon.c','    pr_apply_base_stat_before_fusion(bt, p, base);','', '    pr_apply_base_stat_after_fusion(bt, p, base);','    pr_apply_base_stat_before_fusion(bt,p,base);\n    pr_apply_base_stat_after_fusion(bt,p,base);','form-fusion-test',a.api),
 'vitamins-before-fusion':('pokemon.c','    pr_apply_base_stat_before_fusion(bt, p, base);','    pr_apply_base_stat_before_fusion(bt,p,base);\n    pr_apply_base_stat_after_fusion(bt,p,base);','    pr_apply_base_stat_after_fusion(bt, p, base);','','form-fusion-test',a.api),
 'no-spliced-halving':('pokemon.c','base[s] = ceil(base[s] / 2.0);','base[s] = base[s];',None,None,'form-fusion-test',a.api),
 'flat-uses-permanent-form':('modifiers.c','const uint8_t *bs = pr_pokemon_form(p)->base_stats;','const uint8_t *bs = pr_pokemon_base_form(p)->base_stats;',None,None,'form-fusion-test',a.api),
 'total-multiplies-stacks':('modifiers.c','floor(base[i] + m->num)','floor(base[i] + m->num * m->stack_count)',None,None,'form-fusion-test',a.api),
 'total-not-admitted':('modifiers.c','if (m->cls == PR_MOD_BASE_STAT_TOTAL)\n        return','if (m->cls == PR_MOD_BASE_STAT_TOTAL)\n        return false &&',None,None,'stat-order-turn-test',a.turn),
}
inc=['-I'+str(root/'include'),'-I'+str(root/'gen/out'),'-I'+str(root/'build'),'-I'+str(root/'tests')]
for name,(file,old,new,old2,new2,test,fixture) in faults.items():
 s=(root/'src'/file).read_text();assert s.count(old)==1,(name,s.count(old));s=s.replace(old,new)
 if old2:assert s.count(old2)==1,(name,s.count(old2));s=s.replace(old2,new2)
 src=out/(name+'.c');obj=out/(name+'.o');exe=out/name;src.write_text(s);subprocess.run(['gcc','-O2','-std=c99','-ffp-contract=off',*inc,'-c',str(src),'-o',str(obj)],check=True);subprocess.run(['gcc','-O2','-std=c99',*inc,str(root/'tests'/(test+'.c')),str(obj),str(root/'build/libprcore.a'),'-lm','-o',str(exe)],check=True);r=subprocess.run([str(exe),str(fixture.resolve())],capture_output=True,text=True);(out/(name+'.log')).write_text(r.stdout+r.stderr);assert r.returncode==1,(name,r.returncode);print(name,'caught',flush=True)
refusals=0
for test,fixture in [('form-fusion-test',a.api),('stat-order-turn-test',a.turn)]:
 rows=fixture.read_text().splitlines();bad={'missing-footer':rows[:-1],'extra-tail':rows+['JUNK'],'wrong-version':[rows[0].replace(' 2 ',' 99 ',1).replace(' 1 ',' 99 ',1)]+rows[1:]}
 if test=='form-fusion-test':
  ci=next(i for i,x in enumerate(rows)if x.startswith('C '));ri=next(i for i,x in enumerate(rows)if x.startswith('I '))
  for name,index,col,value in [('fractional-case',ci,1,'0.5'),('bad-position',ci,2,'4'),('bad-ability-index',ci,4,'3'),('bad-fusion-index',ci,7,'3'),('fractional-hp',ri,7,'0.5')]:
   r=rows.copy();v=r[index].split();v[col]=value;r[index]=' '.join(v);bad[name]=r
 for name,lines in bad.items():
  f=out/(test+'-'+name+'.txt');f.write_text('\n'.join(lines)+'\n');r=subprocess.run([str(root/'build'/test),str(f)],capture_output=True,text=True);assert r.returncode==2,(test,name,r.returncode);refusals+=1
print(f'{len(faults)} compiled faults caught; {refusals} malformed fixtures rejected',flush=True)
