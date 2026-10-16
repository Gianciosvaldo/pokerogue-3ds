#!/usr/bin/env python3
"""Rebuild ABI-consistent narrowed cores; preserve strict capture parsing."""
from pathlib import Path
import argparse, os, shutil, subprocess
p=argparse.ArgumentParser();p.add_argument('trace',type=Path);p.add_argument('score',type=Path);a=p.parse_args()
root=Path(__file__).resolve().parents[2];core=root/'battle-core';out=core/'build/wide-level-guards';out.mkdir(parents=True,exist_ok=True)
trace=a.trace.resolve();score=a.score.resolve()
for name,typ in [('byte','uint8_t'),('word','uint16_t')]:
 build=out/name;inc=build/'include';shutil.copytree(core/'include',inc,dirs_exist_ok=True)
 header=inc/'pr/battle.h';s=header.read_text();assert s.count('    int32_t level;')==1;header.write_text(s.replace('    int32_t level;',f'    {typ} level;'))
 flags=['-O2','-std=c99','-ffp-contract=off','-I'+str(inc),'-I'+str(core/'gen/out')]
 with (out/(name+'-build.log')).open('w') as log:
  subprocess.run(['make','-s','-C',str(core),'-j4','BUILD='+str(build),'CPPFLAGS=-I'+str(inc)+' -Igen/out',str(build/'libprcore.a')],stdout=log,stderr=subprocess.STDOUT,check=True)
 for runner,capture in [('wide-level-test',trace)]+([('wide-boss-score-test',score)] if name=='byte' else []):
  exe=build/runner;subprocess.run(['gcc',*flags,str(core/'tests'/f'{runner}.c'),str(build/'libprcore.a'),'-lm','-o',str(exe)],check=True)
  r=subprocess.run([str(exe),str(capture)],capture_output=True,text=True);(out/f'{name}-{runner}.log').write_text(r.stdout+r.stderr);assert r.returncode==1,(name,runner,r.returncode);print(name,runner,r.stdout.strip(),flush=True)
rows=trace.read_text().splitlines();bad={'missing-footer':rows[:-1],'extra-tail':rows+['JUNK'],'wrong-version':['WIDE_LEVEL 2 16800']+rows[1:],'wrong-count':['WIDE_LEVEL 1 16799']+rows[1:]}
for name,prefix,col,value in [('fractional-id','C ',1,'0.5'),('invalid-mode','C ',2,'0'),('invalid-species','C ',3,'0'),('invalid-form','C ',4,'99'),('invalid-fusion','C ',5,'1'),('unsupported-level','C ',6,'1000001'),('overflow-level','C ',6,'4294967297'),('invalid-nature','C ',7,'25'),('invalid-ability','C ',8,'99999'),('invalid-passive','C ',9,'1'),('invalid-hp-mode','C ',10,'4'),('invalid-side','C ',11,'2'),('invalid-iv','I ',1,'32'),('fractional-stat','O ',2,'1.5'),('nonfinite-stat','O ',2,'nan'),('negative-hp','O ',1,'-1')]:
 j=next(i for i,row in enumerate(rows) if row.startswith(prefix));words=rows[j].split();words[col]=value;lines=rows.copy();lines[j]=' '.join(words);bad[name]=lines
for name,lines in bad.items():
 path=out/(name+'.txt');path.write_text('\n'.join(lines)+'\n');r=subprocess.run([str(core/'build/wide-level-test'),str(path)],capture_output=True,text=True);assert r.returncode==2,(name,r.returncode);print(name,'rejected',flush=True)
# Override only the save reader object in otherwise unchanged, matching wide ABI.
source=(root/'game-core/src/gc_save.c').read_text()
for name,code in [('missing-level-migration',source.replace('f == pokemon_fields &&', 'false && f == pokemon_fields &&')),('missing-met-migration',source.replace('f == mon_fields &&', 'false && f == mon_fields &&')),('signed-legacy-byte',source.replace('int32_t level = src[at[i]]','int32_t level = (int8_t)src[at[i]]'))]:
 assert code!=source
 path=out/(name+'.c');path.write_text(code)
 flags=['-O2','-std=c99','-ffp-contract=off']+['-I'+str(root/p) for p in ['game-core/src','game-core/include','run-core/include','ai-core/include','reward-core/include','reward-core/gen/out','battle-core/include','battle-core/gen/out']]
 obj=out/(name+'.o');exe=out/name
 subprocess.run(['gcc',*flags,'-c',str(path),'-o',str(obj)],check=True)
 libs=[root/'game-core/build'/p for p in ['libgc.a','reward-core/libreward.a','ai-core/libai.a','run-core/librun.a','run-core/battle-core/libprcore.a']]
 subprocess.run(['gcc',*flags,str(root/'game-core/tests/wide_level_host_test.c'),str(obj),*map(str,libs),'-lm','-o',str(exe)],check=True)
 r=subprocess.run([str(exe)],cwd=root/'game-core',capture_output=True,text=True);(out/(name+'.log')).write_text(r.stdout+r.stderr);assert r.returncode==1,(name,r.returncode);print(name,'caught',flush=True)
print(f'2 ABI narrowing faults, 3 migration faults caught; {len(bad)} malformed captures rejected')
