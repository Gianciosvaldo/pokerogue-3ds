#!/usr/bin/env python3
"""Compiled production faults plus strict raw/caller transport rejection."""
from pathlib import Path
import os,sys,subprocess,tempfile
r=Path(__file__).resolve().parents[2];raw=Path(sys.argv[1]).resolve();mimicry=Path(sys.argv[2]).resolve();learn=Path(sys.argv[3]).resolve();env=os.environ.copy();assert env.get('TURNTEST_LOCALES')
runners={'raw':r/'battle-core/build/keldeo-forms-test','mimicry':r/'battle-core/build/mimicry-test','learn':r/'game-core/build/keldeo_learn_test','host':r/'game-core/build/keldeo_host_test'}
traces={'raw':raw,'mimicry':mimicry,'learn':learn}
for kind,trace in traces.items():subprocess.run([runners[kind],trace],check=True,env=env)
subprocess.run([runners['host']],check=True,env=env)
faults=[]
def fault(name,file,old,new,driver='raw'):
 s=(r/file).read_text();assert s.count(old)==1,(name,s.count(old));faults.append((name,file,s.replace(old,new,1),driver))
fault('Daily-gate','battle-core/src/phases.c','    if (bt->game_mode == GAME_MODES_DAILY) return false;','    /* missing Daily eligibility */')
fault('effective-instead-of-original-moves','battle-core/src/phases.c','for (int i = 0; i < p->n_moves; i++) known |= p->moves[i] == MOVE_ID_SECRET_SWORD;','for (int i = 0; i < PR_N_MOVES(p); i++) known |= PR_MOVES(p)[i] == MOVE_ID_SECRET_SWORD;')
fault('quiet-player-phase','battle-core/src/phases.c','bool nonquiet = mon->species == SPECIES_ID_KELDEO && mon->is_player;','bool nonquiet = false;')
fault('modal-queues-EndEvolution','battle-core/src/phases.c','if (!PH(ph)->sw_modal)','if (true)')
fault('missing-EndEvolution','battle-core/src/phases.c','pr_phase_unshift(bt, e, pr_phase_new(e, PH_END_EVOLUTION, -1));','(void)0;')
fault('nonquiet-runs-post-form-abilities','battle-core/src/phases.c','pr_change_form_state(bt, p, PH(ph)->move_id);','pr_change_form_state(bt, p, PH(ph)->move_id); pr_ab_params extra={.pokemon=p,.move=-1,.after_form_change=true};pr_apply_ab_attrs(bt,PR_AC_PostSummonAbAttr,&extra);')
fault('nonquiet-clears-added-type','battle-core/src/phases.c','pr_change_form_state(bt, p, PH(ph)->move_id);','pr_change_form_state(bt, p, PH(ph)->move_id);p->added_type=0;')
fault('nonquiet-ends-Tera','battle-core/src/phases.c','pr_change_form_state(bt, p, PH(ph)->move_id);','pr_change_form_state(bt, p, PH(ph)->move_id);p->terastallized=false;p->stellar_types_boosted=0;')
# Restrict this fault to the nonquiet body; the quiet removal remains untouched.
s=(r/'battle-core/src/phases.c').read_text();a=s.index('static void form_change_phase(');b=s.index('static void quiet_form_change_phase(',a);part=s[a:b];old='    pr_remove_tag(bt, e, p, BATTLER_TAG_TYPE_AUTOTOMIZED);';assert part.count(old)==1;faults.append(('retained-Autotomized','battle-core/src/phases.c',s[:a]+part.replace(old,'    /* omitted end removal */')+s[b:],'raw'))
fault('effective-Transform-weight','battle-core/src/pokemon.c','const pr_form *f = pr_pokemon_base_form(p);\n    double w = f->weight_x10','const pr_form *f = pr_pokemon_form(p);\n    double w = f->weight_x10')
fault('copied-form-in-bypass-query','battle-core/src/pokemon.c','if ((flags & PR_TYPES_BYPASS_SUMMON) && p->transformed)','if (false && (flags & PR_TYPES_BYPASS_SUMMON) && p->transformed)','mimicry')
fault('Mimicry-Tera-gate','battle-core/src/ab_attrs.c','GATE(!p->terastallized);','GATE(true);','mimicry')
fault('simulated-Mimicry-no-mutation','battle-core/src/ab_attrs.c','GATE(!p->terastallized);','GATE(!p->terastallized);if(pp->simulated)return 1;','mimicry')
fault('Mimicry-added-type-retained','battle-core/src/ab_attrs.c','if (types[i] == w->added_type) { w->added_type = 0; break; }','if (types[i] == w->added_type) { break; }','mimicry')
fault('Mimicry-wrong-Grassy-type','battle-core/src/ab_attrs.c','bt->terrain == TERRAIN_GRASSY ? POKEMON_TYPE_GRASS\n                 :','bt->terrain == TERRAIN_GRASSY ? POKEMON_TYPE_WATER\n                 :','mimicry')
fault('Mimicry-uses-overridden-types','battle-core/src/ab_attrs.c','else n = pr_get_types(p, types, PR_TYPES_NO_TERA | PR_TYPES_BYPASS_SUMMON | PR_TYPES_IGNORE_THIRD);','else n = pr_get_types(p, types, PR_TYPES_NO_TERA | PR_TYPES_IGNORE_THIRD);','mimicry')
fault('missing-learn-callback','game-core/src/gc_phases.c','if (pr_learned_form_target(&run->bt, &m->b, &form))','if (false)','host')
fault('form-unshifted-ahead-of-pending-learn','game-core/src/gc_phases.c','gc_rq_push(run, gc_rp(RP_REWARD_FORM, idx, form, 0));','gc_rq_unshift(run, gc_rp(RP_REWARD_FORM, idx, form, 0));','host')
fault('re-evaluated-cached-target','game-core/src/gc_reward_lifecycle.c','    m->b.form = (uint8_t)form;','    uint16_t latest;if(!pr_learned_form_target(&run->bt,&m->b,&latest))return;form=latest;\n    m->b.form = (uint8_t)form;','host')
fault('unknown-passive-context-accepted','battle-core/src/pokemon.c','if (gm == GAME_MODES_CHALLENGE && (!bt->challenge_flags_known\n        || (bt->challenge_flags & (1u << CHALLENGES_PASSIVES))))','if (gm == GAME_MODES_CHALLENGE && (bt->challenge_flags & (1u << CHALLENGES_PASSIVES)))','host')
fault('missing-Daily-mode-context-accepted','battle-core/src/phases.c','if (bt->game_mode < GAME_MODES_CLASSIC || bt->game_mode > GAME_MODES_CHALLENGE)','if (false)','host')
flags=['-O2','-std=c99','-ffp-contract=off',*['-I'+str(r/x) for x in ['battle-core/include','battle-core/gen/out','battle-core/build','battle-core/tests','game-core/include','game-core/src','run-core/include','run-core/gen/out','reward-core/include','reward-core/gen/out','ai-core/include','ai-core/gen/out']]]
with tempfile.TemporaryDirectory(prefix='keldeo-guards-',dir=r/'work') as tmp:
 d=Path(tmp)
 for name,file,changed,driver in faults:
  src=d/Path(file).name;src.write_text(changed);obj=d/'fault.o';exe=d/'fault'
  subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
  if driver=='host':
   unit=r/'game-core/tests/keldeo_host_test.c';libs=[r/'game-core/build'/x for x in ['libgc.a','reward-core/libreward.a','ai-core/libai.a','run-core/librun.a','run-core/battle-core/libprcore.a']]
  else:unit=r/('battle-core/tests/mimicry-test.c' if driver=='mimicry' else 'battle-core/tests/keldeo-forms-test.c');libs=[r/'battle-core/build/libprcore.a']
  subprocess.run(['gcc',*flags,unit,obj,*libs,'-lm','-o',exe],check=True,capture_output=True)
  p=subprocess.run([exe,*([traces[driver]] if driver!='host' else [])],env=env,capture_output=True)
  assert p.returncode==1,(name,p.returncode,p.stdout[-500:],p.stderr[-500:]);print(name,'caught',flush=True)
 bad=[];data=raw.read_bytes();first,rest=data.split(b'\n',1)
 for name,changed in [('version',first.replace(b'KELDEO 5 ',b'KELDEO 6 ')+b'\n'+rest),('small',b'KELDEO 5 1 777\n'+rest),('footer',data.rsplit(b'KELDEO_COMPLETE ',1)[0]+b'KELDEO_COMPLETE 1\n'),('trailing',data+b'JUNK\n')]:bad.append(('raw',name,changed))
 for name,prefix,col,value in [('fractional-id',b'C ',1,b'0.5'),('actor',b'C ',2,b'8'),('mode',b'C ',3,b'2'),('nan-flags',b'C ',6,b'NaN'),('direct',b'C ',7,b'2'),('modal',b'C ',8,b'2'),('target',b'C ',9,b'3'),('tag-count',b'IT ',1,b'4')]:
  a=data.index(b'\n'+prefix)+1;b=data.index(b'\n',a);row=data[a:b].split();row[col]=value;bad.append(('raw',name,data[:a]+b' '.join(row)+data[b:]))
 for kind,trace,marker in [('mimicry',mimicry,b'MIMICRY_COMPLETE '),('learn',learn,b'KELDEO_LEARN_COMPLETE ')]:
  block=trace.read_bytes();bad.extend([(kind,'trailing',block+b'JUNK\n'),(kind,'footer',block.rsplit(marker,1)[0]+marker+b'1\n')])
 block=learn.read_bytes()
 for name,value in [('negative-profile',b'-1'),('overflow-profile',b'18446744073709551616'),('fractional-profile',b'0.5')]:
  a=block.index(b'\nD ')+1;b=block.index(b'\n',a);row=block[a:b].split();row[1]=value;bad.append(('learn',name,block[:a]+b' '.join(row)+block[b:]))
 for kind,name,changed in bad:
  f=d/'bad.txt';f.write_bytes(changed);p=subprocess.run([runners[kind],f],env=env,capture_output=True);assert p.returncode==2,(kind,name,p.returncode);print(kind,name,'rejected',flush=True)
 print(f'{len(faults)} compiled production faults caught; {len(bad)} malformed captures rejected')
