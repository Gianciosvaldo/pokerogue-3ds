#!/usr/bin/env python3
"""Mutate only the real C getBaseTypes branch of pr_get_types, rebuild, and replay a source trace.
usage: custom-types-mutations.py <trace>; every mutant must make custom-types-test exit 1 (the intact build, 0)."""
from pathlib import Path
import shutil, subprocess, sys, tempfile
root=Path(__file__).resolve().parent.parent
trace=Path(sys.argv[1]).resolve()
S='src/pokemon.c'
mutants={
 'ignore-custom-first':('p->custom_types[0] ? (int8_t)(p->custom_types[0] - 1) : f->type1','f->type1'),
 'ignore-custom-second':('p->custom_types[1] ? (int8_t)(p->custom_types[1] - 1)\n                           : f->type2 < 0','f->type2 < 0'),
 'custom-off-by-one':('(int8_t)(p->custom_types[0] - 1)','(int8_t)(p->custom_types[0])'),
 'custom-second-off-by-one':('(int8_t)(p->custom_types[1] - 1)\n                           :','(int8_t)(p->custom_types[1])\n                           :'),
 'ignore-fusion-custom-first':('p->fusion_custom_types[0] ? (int8_t)(p->fusion_custom_types[0] - 1) : ff->type1','ff->type1'),
 'ignore-fusion-custom-second':('p->fusion_custom_types[1] ? (int8_t)(p->fusion_custom_types[1] - 1)\n                                : ff->type2 < 0','ff->type2 < 0'),
 'fusion-reads-own-custom':('p->fusion_custom_types[0] ? (int8_t)(p->fusion_custom_types[0] - 1) : ff->type1','p->custom_types[0] ? (int8_t)(p->custom_types[0] - 1) : ff->type1'),
 'custom-only-without-fusion':('int8_t type1 = p->custom_types[0] ?','int8_t type1 = (p->custom_types[0] && !ff) ?'),
 'custom-beats-summon':('if (!(flags & PR_TYPES_BYPASS_SUMMON) && p->n_summon_types && !illusion) {','if (!(flags & PR_TYPES_BYPASS_SUMMON) && p->n_summon_types && !illusion && !p->custom_types[0]) {'),
 'null-slot-is-normal':('p->custom_types[0] ? (int8_t)(p->custom_types[0] - 1) : f->type1','(int8_t)(p->custom_types[0] ? p->custom_types[0] - 1 : 0)'),
}
with tempfile.TemporaryDirectory(prefix='custom-types-mutation-') as temp:
 for name,edit in [('intact',None),*mutants.items()]:
  dest=Path(temp)/name;shutil.copytree(root,dest,ignore=shutil.ignore_patterns('build'))
  if edit:
   p=dest/S;s=p.read_text();old,new=edit
   assert s.count(old)==1,(name,s.count(old));p.write_text(s.replace(old,new,1))
  log=Path(temp)/(name+'.log')
  with log.open('w') as f:
   build=subprocess.run(['make','-s','-j2','-C',str(dest),'build/custom-types-test'],stdout=f,stderr=subprocess.STDOUT)
   if build.returncode:raise SystemExit(f'{name}: build failed')
   replay=subprocess.run([str(dest/'build/custom-types-test'),str(trace)],stdout=f,stderr=subprocess.STDOUT)
  print(f'{name}: exit {replay.returncode}')
  if replay.returncode!=(0 if name=='intact' else 1):raise SystemExit(f'{name}: unexpected status')
