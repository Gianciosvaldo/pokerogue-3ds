#!/usr/bin/env python3
"""Require the full native turns to detect fusion arithmetic and owner faults."""
from pathlib import Path
import os,subprocess,sys,tempfile
root=Path(__file__).resolve().parents[2]
trace=Path(sys.argv[1]).resolve();env=os.environ.copy();assert env.get('TURNTEST_LOCALES')
runner=root/'battle-core/build/fused-held-test'
subprocess.run([runner,trace],env=env,check=True)
mods=(root/'battle-core/src/modifiers.c').read_text()
faults=[]
def fault(name,old,new):
 assert mods.count(old)==1,(name,mods.count(old))
 faults.append((name,mods.replace(old,new)))
fault('mixed-evolution','if(fusion&&fusion->has_evolutions!=unevolved)*value*=1+(m->mult-1)/2;','if(fusion&&fusion->has_evolutions!=unevolved)*value*=m->mult;')
fault('evolved-refusal','else if(unevolved)*value*=m->mult;','else *value*=m->mult;')
fault('species-secondary','&&!(p->fusion_species&&species_listed(m,p->fusion_species))','')
fault('leek-secondary','&&!(src->fusion_species&&species_listed(m,src->fusion_species))','')
fault('secondary-max','return fusion&&p->fusion_form<fusion->nforms&&max_key(pr_forms[fusion->form0+p->fusion_form].form_key);','return false;')
a=mods.index('static void apply_crit_booster_modifiers(');b=mods.index('void pr_apply_temp_crit_booster_modifiers(',a)
scope=mods[a:b];old='if (bounded && !pr_mod_is_ported_item(m))';assert scope.count(old)==1
faults.append(('bounded-fusion-refusal',mods[:a]+scope.replace(old,'if (bounded && (!pr_mod_is_ported_item(m)||(m->cls==PR_MOD_SPECIES_CRIT_BOOSTER&&src->fusion_species)))')+mods[b:]))
old='if (m->cls==PR_MOD_SPECIES_CRIT_BOOSTER&&!species_listed(m,src->species)\n            &&!(src->fusion_species&&species_listed(m,src->fusion_species)))continue;'
fault('ineligible-fusion',old,'/* wrongly boost every fused species */')
phases=(root/'battle-core/src/phases.c').read_text();old='if (pr_mod_is_ported_item(m) && !vitamin_over_cap) continue;';assert phases.count(old)==1
faults.append(('turn-owner-refusal',phases.replace(old,'if(p&&p->fusion_species&&(m->cls==PR_MOD_EVOLUTION_STAT_BOOSTER||m->cls==PR_MOD_SPECIES_STAT_BOOSTER||m->cls==PR_MOD_SPECIES_CRIT_BOOSTER)){pr_note_unported(bt,PR_FEAT_TURN_MODIFIERS);return;}\n        '+old)))
flags=['-O2','-std=c99','-ffp-contract=off',*['-I'+str(root/p) for p in ['battle-core/include','battle-core/gen/out','battle-core/src','battle-core/tests','battle-core/build']]]
with tempfile.TemporaryDirectory(prefix='fused-held-guards-') as tmp:
 tmp=Path(tmp)
 for name,source in faults:
  src,obj,binary=tmp/(name+'.c'),tmp/(name+'.o'),tmp/name
  src.write_text(source)
  subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
  subprocess.run(['gcc',*flags,root/'battle-core/tests/fused-held-test.c',obj,root/'battle-core/build/libprcore.a','-lm','-o',binary],check=True,capture_output=True)
  r=subprocess.run([binary,trace],env=env,capture_output=True)
  assert r.returncode==1,(name,r.returncode,r.stdout,r.stderr)
  print(name,'caught',flush=True)
 rows=trace.read_text().splitlines()
 malformed={'missing-footer':rows[:-1],'extra-record':rows+['JUNK'],'wrong-count':['# FUSEDHELD_CONTRACT 1 2559']+rows[1:],'wrong-footer':rows[:-1]+['# FUSEDHELD_COMPLETE 2559']}
 for name,prefix,old,new in [('case-id','CASE ','CASE 0','CASE 1'),('unknown-item','MOD ','cls=EvolutionStatBoosterModifier','cls=UNKNOWN')]:
  index=next(i for i,r in enumerate(rows) if r.startswith(prefix));assert old in rows[index]
  changed=rows.copy();changed[index]=changed[index].replace(old,new);malformed[name]=changed
 for name,content in malformed.items():
  path=tmp/'bad.txt';path.write_text('\n'.join(content)+'\n')
  r=subprocess.run([runner,path],env=env,capture_output=True)
  assert r.returncode==2,(name,r.returncode,r.stdout,r.stderr)
  print(name,'rejected',flush=True)
 print('8 compiled faults caught; 6 malformed captures rejected')
