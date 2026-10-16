#!/usr/bin/env python3
"""Compile Commander, tag, queue and save faults and reject malformed native observations."""
from pathlib import Path
import argparse,os,subprocess
p=argparse.ArgumentParser();p.add_argument('trace',type=Path);a=p.parse_args()
root=Path(__file__).resolve().parents[2];core=root/'battle-core';out=core/'build/commander-guards';out.mkdir(parents=True,exist_ok=True)
assert os.environ.get('TURNTEST_LOCALES')
code={name:(core/'src'/f'{name}.c').read_text() for name in ['ab_attrs','battler_tags','move_effects','engine','commands','switching','phases']}
faults={}
def change(name,file,old,new):
 s=code[file]
 if file=='ab_attrs':
  begin=s.index('    case PR_AC_CommanderAbAttr: {');end=s.index('    case PR_AC_PokemonTypeChangeAbAttr:',begin);leaf=s[begin:end]
  assert leaf.count(old)==1,(name,leaf.count(old));faults[name]=(file,s[:begin]+leaf.replace(old,new)+s[end:])
 else:
  assert s.count(old)==1,(name,s.count(old));faults[name]=(file,s.replace(old,new))
change('missing-commander','ab_attrs','    case PR_AC_CommanderAbAttr: {','    case PR_AC_CommanderAbAttr: return 0; {')
change('allow-singles','ab_attrs','GATE(bt->double_battle && ally && ally->hp > 0','GATE(ally && ally->hp > 0')
change('allow-fainted-ally','ab_attrs','ally && ally->hp > 0 && ally->on_field','ally && ally->on_field')
change('allow-absent-ally','ab_attrs','ally->hp > 0 && ally->on_field && !ally->switch_out_status','ally->hp > 0 && !ally->switch_out_status')
change('allow-switching-ally','ab_attrs','ally->hp > 0 && ally->on_field && !ally->switch_out_status','ally->hp > 0 && ally->on_field')
change('allow-nondondozo','ab_attrs','ally->species == SPECIES_ID_DONDOZO && !pr_get_battler_tag(ally, BATTLER_TAG_TYPE_COMMANDED)','!pr_get_battler_tag(ally, BATTLER_TAG_TYPE_COMMANDED)')
change('reject-one-hp-user','ab_attrs','GATE(bt->double_battle && ally && ally->hp > 0','GATE(p->hp > 1 && bt->double_battle && ally && ally->hp > 0')
change('simulate-side-effects','ab_attrs','        if (!pp->simulated) {\n            pr_lapse_tags(bt, c->e, (pr_pokemon *)p, BATTLER_TAG_LAPSE_TYPE_MOVE_EFFECT);','        if (true) {\n            pr_lapse_tags(bt, c->e, (pr_pokemon *)p, BATTLER_TAG_LAPSE_TYPE_MOVE_EFFECT);')
change('skip-semi-lapse','ab_attrs','            pr_lapse_tags(bt, c->e, (pr_pokemon *)p, BATTLER_TAG_LAPSE_TYPE_MOVE_EFFECT);','')
change('missing-apply-animation','ab_attrs','            pr_unshift_pokemon_anim_for(bt, c->e, p, POKEMON_ANIM_TYPE_COMMANDER_APPLY);','')
change('missing-cancellation','ab_attrs','            pr_remove_move_phase(bt, c->e, p);','')
change('missing-tag','battler_tags','    case BATTLER_TAG_TYPE_COMMANDED: /* CommandedTag */','')
change('wrong-tag-turn','battler_tags','    case BATTLER_TAG_TYPE_COMMANDED:\n        t.turn_count = 0;','    case BATTLER_TAG_TYPE_COMMANDED:\n        t.turn_count = 1;')
change('wrong-tag-move','battler_tags','    case BATTLER_TAG_TYPE_COMMANDED:\n        t.turn_count = 0;\n        t.source_move = MOVE_ID_NONE;','    case BATTLER_TAG_TYPE_COMMANDED:\n        t.turn_count = 0;\n        t.source_move = source_move;')
change('uncached-form','battler_tags','        if (source) t->commanded_form_key = (uint16_t)(pr_pokemon_base_form(source) - pr_forms + 1);','')
change('half-commander-boost','battler_tags','pr_unshift_stat_stage_change(bt, e, pr_battler_index(bt, p), stats, 5, 2, p);','pr_unshift_stat_stage_change(bt, e, pr_battler_index(bt, p), stats, 5, 1, p);')
change('missing-remove-animation','battler_tags','            pr_unshift_pokemon_anim_for(bt, e, p, POKEMON_ANIM_TYPE_COMMANDER_REMOVE);','            (void)p;')
change('always-remove-animation','battler_tags','if (source && active_on_field(source))\n            pr_unshift_pokemon_anim_for','if (source)\n            pr_unshift_pokemon_anim_for')
change('order-parent-hp-gate','move_effects','    case PR_AC_OrderUpStatBoostAttr: {\n        /* This override','    case PR_AC_OrderUpStatBoostAttr: {\n        if (!effect_can_apply(ef, user, target)) return;\n        /* This override')
change('live-order-form','move_effects','        const char *key = pr_commanded_form_key(tag);','        const pr_pokemon *src = pr_pokemon_by_id(bt, tag->source_id);\n        const char *key = src ? pr_pokemon_base_form(src)->form_key : "curly";')
change('wrong-droopy-stat','move_effects','!strcmp(key, "droopy") ? STAT_DEF','!strcmp(key, "droopy") ? STAT_SPDEF')
change('wrong-stretchy-stat','move_effects','!strcmp(key, "stretchy") ? STAT_SPD','!strcmp(key, "stretchy") ? STAT_SPATK')
change('missing-removal-reorder','engine','bool pr_remove_move_phase(pr_battle *bt, pr_engine *e, const pr_pokemon *p)\n{\n    dyn_reorder(bt, e, PH_MOVE);','bool pr_remove_move_phase(pr_battle *bt, pr_engine *e, const pr_pokemon *p)\n{')
change('cancel-by-slot','engine','        if (e->pool[ph].pokemon != p) continue;','        if (e->pool[ph].battler != pr_battler_index(bt, p)) continue;')
change('reject-undefined-group','engine','    if (missing && present) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return; }','    if (missing) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return; }')
change('ignore-commanded-trapping','commands','    if (commanded(bt, p)) return true;','')
change('post-summon-placeholder','switching','        pr_apply_ab_attrs(bt, PR_AC_CommanderAbAttr, &pp);','        (void)pp;pr_hook_ab(bt, member, PR_AC_CommanderAbAttr);')
change('post-summon-field-order','switching',
 '    pr_speed_iter_init(bt, &allies, p->is_player ? ARENA_TAG_SIDE_PLAYER : ARENA_TAG_SIDE_ENEMY);\n    while ((member = pr_speed_iter_next(bt, &allies))) {',
 '    for(int k=0;k<(bt->double_battle?2:1);k++){member=bt->field[(p->is_player?0:2)+k];if(!member||member->hp<=0)continue;')
change('uncached-animation-pokemon','phases','        PH(ph)->pokemon = (pr_pokemon *)p;','        PH(ph)->pokemon = NULL;')
flags=['-O2','-std=c99','-ffp-contract=off','-I'+str(core/'include'),'-I'+str(core/'gen/out')]
for name,(file,source) in faults.items():
 src,obj,exe=(out/(name+suffix) for suffix in ['.c','.o','']);src.write_text(source)
 subprocess.run(['gcc',*flags,'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run(['gcc',*flags,str(core/'tests/commander-test.c'),str(obj),str(core/'build/libprcore.a'),'-lm','-o',str(exe)],check=True)
 r=subprocess.run([str(exe),str(a.trace.resolve())],capture_output=True,text=True);(out/(name+'.log')).write_text(r.stdout+r.stderr);assert r.returncode in (1,-11,-6),(name,r.returncode);print(name,'caught',r.returncode,flush=True)
# Each malformed file retains the complete native stream, and is removed after
# checking to avoid retaining many identical hundreds-of-megabytes scratch copies.
bad=[('missing-footer',None,None,None),('extra-tail',None,None,None),('wrong-version',None,None,None),('wrong-footer',None,None,None)]
bad += [('fractional-case', 'C ', 1, '0.5'), ('invalid-mode', 'C ', 2, '4'), ('invalid-holder', 'C ', 3, '12'), ('invalid-target', 'C ', 4, '-1'), ('invalid-double', 'C ', 5, '2'), ('invalid-party', 'C ', 6, '7'), ('invalid-simulation', 'C ', 8, '2'), ('invalid-lapse', 'C ', 9, '9'), ('invalid-order-holder', 'C ', 10, '12'), ('invalid-source', 'C ', 11, '4294967296'), ('invalid-gas', 'G ', 1, '3'), ('invalid-slot', 'I ', 1, '12'), ('invalid-hp', 'I ', 8, '-1'), ('nonfinite-hp', 'I ', 8, 'nan'), ('invalid-semis', 'IX ', 2, '16'), ('invalid-presence', 'IX ', 3, '2'), ('invalid-physical-field', 'IX ', 8, '2'), ('invalid-trick-room', 'G ', 3, '2'), ('overflow-tag-actor', 'IX ', 1, '4294967296'), ('invalid-result', 'RESULT ', 1, '2'), ('invalid-trapped', 'TRAPPED ', 1, '2'), ('invalid-jobs', 'JOBS ', 1, '6'), ('invalid-timing', 'J ', 5, '3'), ('invalid-queue-count', 'QCOUNT ', 1, '129'), ('invalid-level-count', 'QCOUNT ', 2, '0'), ('invalid-deferred', 'QCOUNT ', 3, '2'), ('queue-overflow', 'Q ', 2, '4294967296'), ('queue-fraction', 'Q ', 5, '0.5'), ('queue-invalid-person', 'Q ', 6, '12'), ('queue-nonfinite', 'Q ', 7, 'nan')]
for name,prefix,column,value in bad:
 path=out/(name+'.txt');changed=False
 with a.trace.open() as src,path.open('w') as dst:
  for i,line in enumerate(src):
   if name=='missing-footer' and line.startswith('COMMANDER_COMPLETE '):changed=True;continue
   if name=='wrong-footer' and line.startswith('COMMANDER_COMPLETE '):line='COMMANDER_COMPLETE 1\n';changed=True
   if name=='wrong-version' and i==0:line='COMMANDER 2 16896 777\n';changed=True
   if prefix and not changed and line.startswith(prefix):
    parts=line.split();parts[column]=value;line=' '.join(parts)+'\n';changed=True
   dst.write(line)
  if name=='extra-tail':dst.write('JUNK\n');changed=True
 assert changed,name
 r=subprocess.run([str(core/'build/commander-test'),str(path)],capture_output=True,text=True);path.unlink();assert r.returncode==2,(name,r.returncode);print(name,'rejected',flush=True)
print(f'{len(faults)} compiled faults caught; {len(bad)} malformed captures rejected')
