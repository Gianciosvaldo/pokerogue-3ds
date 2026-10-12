#!/usr/bin/env python3
"""Compiled source-algorithm mutations must fail complete turns or actual getter probes."""
from pathlib import Path
import os,subprocess,sys,tempfile
root=Path(__file__).resolve().parent.parent;trace,helpers=map(lambda p:Path(p).resolve(),sys.argv[1:])
attrs=(root/'src/move_attrs.c').read_text();phases=(root/'src/phases.c').read_text()
c0=attrs.index('int pr_beat_up_hit_count(');c1=attrs.index('static int count_positive_stat_stages',c0)
p0=attrs.index('        case PR_AC_BeatUpAttr: {');p1=attrs.index('        case PR_AC_PresentPowerAttr:',p0)
mutants=[
 ('all-party-members','count','if (p->id == user->id || p->status == STATUS_EFFECT_NONE) count++;','count++;'),
 ('status-excludes-user','count','p->id == user->id || p->status == STATUS_EFFECT_NONE','p->status == STATUS_EFFECT_NONE'),
 ('hp-filter','count','if (p->id == user->id || p->status == STATUS_EFFECT_NONE)','if (p->hp > 0 && (p->id == user->id || p->status == STATUS_EFFECT_NONE))'),
 ('field-filter','count','if (p->id == user->id || p->status == STATUS_EFFECT_NONE)','if (p->on_field && (p->id == user->id || p->status == STATUS_EFFECT_NONE))'),
 ('wrong-party-side','count','user->is_player ? 0 : 1','user->is_player ? 1 : 0'),
 ('missing-count-caller','phases','hits = pr_beat_up_hit_count(bt, user);','hits = 1;'),
 ('wrong-count-user','phases','pr_beat_up_hit_count(bt, user)','pr_beat_up_hit_count(bt, first)'),
 ('filtered-kth-power','filtered','',''),
 ('current-form-power','power','pr_pokemon_form(&sp)->base_stats[STAT_ATK]','pr_pokemon_form(party[k])->base_stats[STAT_ATK]'),
 ('actual-stat-power','power','pr_pokemon_form(&sp)->base_stats[STAT_ATK]','party[k]->stats[STAT_ATK]'),
 ('no-power-modulo','power','(user->hit_count - user->hits_left) % allies','(user->hit_count - user->hits_left)'),
 ('fusion-species-power','power','.species = party[k]->species','.species = party[k]->fusion_species ? party[k]->fusion_species : party[k]->species'),
]
with tempfile.TemporaryDirectory(prefix='beat-up-mutants-') as work:
 tmp=Path(work);flags=[os.environ.get('CC','gcc'),'-O2','-std=c99','-ffp-contract=off','-I'+str(root/'include'),'-I'+str(root/'gen/out')]
 for name,target,a,b in [('intact','count','','')]+mutants:
  changed_attrs,changed_phases=attrs,phases
  if target=='filtered':
   part=attrs[p0:p1];part=part.replace('for (int k = idx; k < n; k++)','for (int k = 0; k < n; k++)',1)
   anchor='if (party[k]->id != user->id && party[k]->status) continue;'
   assert part.count(anchor)==1;part=part.replace(anchor,anchor+'\n                if(idx-- > 0)continue;',1)
   changed_attrs=attrs[:p0]+part+attrs[p1:]
  elif a:
   if target=='phases':
    assert phases.count(a)==1,(name,a);changed_phases=phases.replace(a,b,1)
   else:
    start,end=(c0,c1) if target=='count' else (p0,p1);part=attrs[start:end]
    assert part.count(a)==1,(name,a);changed_attrs=attrs[:start]+part.replace(a,b,1)+attrs[end:]
  objects=[]
  for filename,body,original in [('move_attrs.c',changed_attrs,attrs),('phases.c',changed_phases,phases)]:
   if name!='intact' and body==original:continue
   p=tmp/filename;p.write_text(body);obj=p.with_suffix('.o');objects.append(str(obj))
   subprocess.run(flags+['-c',str(p),'-o',str(obj)],check=True)
  statuses=[]
  for src,input_file in [('beat-up-test.c',trace),('beat-up-helper-test.c',helpers)]:
   exe=tmp/src[:-2]
   subprocess.run(flags+[str(root/'tests'/src)]+objects+[str(root/'build/libprcore.a'),'-lm','-o',str(exe)],check=True)
   r=subprocess.run([str(exe),str(input_file)],capture_output=True,text=True);statuses.append(r.returncode)
  print(name,'turn/helper exits',statuses,flush=True)
  if name=='intact':
   if statuses!=[0,0]:raise SystemExit('intact must pass')
  elif any(s not in (0,1) for s in statuses) or 1 not in statuses:raise SystemExit('mutation not detected')
