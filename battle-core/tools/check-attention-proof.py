#!/usr/bin/env python3
"""Compile attention/redirection faults and reject malformed native observations."""
from pathlib import Path
import argparse,os,subprocess
p=argparse.ArgumentParser();p.add_argument('trace',type=Path);a=p.parse_args()
root=Path(__file__).resolve().parents[2];core=root/'battle-core';out=core/'build/attention-guards';out.mkdir(parents=True,exist_ok=True)
assert os.environ.get('TURNTEST_LOCALES')
code={name:(core/'src'/f'{name}.c').read_text() for name in ['battler_tags','phases','engine']}
faults={}
def change(name,file,old,new):
 s=code[file];assert s.count(old)==1,(name,s.count(old));faults[name]=(file,s.replace(old,new))
change('missing-tag','battler_tags','    case BATTLER_TAG_TYPE_CENTER_OF_ATTENTION: /* CenterOfAttentionTag */','')
change('wrong-turn-count','battler_tags','    case BATTLER_TAG_TYPE_CENTER_OF_ATTENTION:\n        t.turn_count = 1;','    case BATTLER_TAG_TYPE_CENTER_OF_ATTENTION:\n        t.turn_count = 2;')
change('wrong-source-id','battler_tags','    case BATTLER_TAG_TYPE_CENTER_OF_ATTENTION:\n        t.turn_count = 1;','    case BATTLER_TAG_TYPE_CENTER_OF_ATTENTION:\n        t.source_id = source_id;\n        t.turn_count = 1;')
change('allow-team-overlap','battler_tags','if (bt->field[k] && pr_get_battler_tag(bt->field[k], t->type))','if (false)')
change('allow-fainted-overlap','battler_tags','if (bt->field[k] && pr_get_battler_tag(bt->field[k], t->type))','if (bt->field[k] && bt->field[k]->hp > 0 && pr_get_battler_tag(bt->field[k], t->type))')
change('allow-switching-overlap','battler_tags','if (bt->field[k] && pr_get_battler_tag(bt->field[k], t->type))','if (bt->field[k] && !bt->field[k]->switch_out_status && pr_get_battler_tag(bt->field[k], t->type))')
change('missing-add-text','battler_tags','        say(bt, e, "battlerTags:centerOfAttentionOnAdd", p);','')
change('skip-expiry','battler_tags','    return --t->turn_count > 0;                   /* BattlerTag.lapse */','    if(t->type==BATTLER_TAG_TYPE_CENTER_OF_ATTENTION)return true;\n    return --t->turn_count > 0;')
change('redirect-spread','phases','    if (!tl || tl->n != 1) return;','    if (!tl || !tl->n) return;')
change('redirect-inactive','phases','!is_active_on_field(o) || !pr_is_opponent(bt, m->pokemon, o)','!is_active(o) || !pr_is_opponent(bt, m->pokemon, o)')
change('redirect-allies','phases','!is_active_on_field(o) || !pr_is_opponent(bt, m->pokemon, o)','!is_active_on_field(o)')
change('skip-attention','phases','if (tag && (tag->source_move != MOVE_ID_RAGE_POWDER','if (false && tag && (tag->source_move != MOVE_ID_RAGE_POWDER')
change('ignore-grass','phases','!pr_is_of_type(m->pokemon, POKEMON_TYPE_GRASS)','true')
change('ignore-overcoat','phases','!pr_has_ability_active(bt, m->pokemon, ABILITY_ID_OVERCOAT)','true')
change('ignore-powder-distinction','phases','tag->source_move != MOVE_ID_RAGE_POWDER','false')
change('wrong-ability-priority','phases','            by_ability = false;','            by_ability = true;')
change('wrong-cached-ability','engine','        if (b < 0) e->pool[ph].anim = -1;','')
flags=['-O2','-std=c99','-ffp-contract=off','-I'+str(core/'include'),'-I'+str(core/'gen/out')]
for name,(file,source) in faults.items():
 src,obj,exe=(out/(name+suffix) for suffix in ['.c','.o','']);src.write_text(source)
 subprocess.run(['gcc',*flags,'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run(['gcc',*flags,str(core/'tests/attention-test.c'),str(obj),str(core/'build/libprcore.a'),'-lm','-o',str(exe)],check=True)
 r=subprocess.run([str(exe),str(a.trace.resolve())],capture_output=True,text=True);(out/(name+'.log')).write_text(r.stdout+r.stderr);assert r.returncode==1,(name,r.returncode);print(name,'caught',flush=True)
rows=a.trace.read_text().splitlines();bad={'missing-footer':rows[:-1],'extra-tail':rows+['JUNK'],'wrong-version':['ATTENTION 2 16768 777']+rows[1:],'wrong-footer':rows[:-1]+['ATTENTION_COMPLETE 1']}
for name,prefix,col,value in [('fractional-case', 'C ', 1, '0.5'), ('invalid-mode', 'C ', 2, '4'), ('invalid-holder', 'C ', 3, '12'), ('invalid-target', 'C ', 4, '-1'), ('invalid-double', 'C ', 5, '2'), ('invalid-party', 'C ', 6, '7'), ('invalid-move', 'C ', 8, '921'), ('invalid-lapse', 'C ', 9, '9'), ('invalid-gas', 'G ', 1, '3'), ('overflow-owner', 'G ', 2, '4294967296'), ('invalid-trick', 'G ', 3, '2'), ('invalid-slot', 'I ', 1, '12'), ('invalid-hp', 'I ', 8, '-1'), ('nonfinite-hp', 'I ', 8, 'nan'), ('invalid-protect', 'IX ', 2, '2'), ('invalid-presence', 'IX ', 3, '2'), ('invalid-result', 'RESULT ', 1, '2'), ('invalid-target-count', 'TARGETS ', 1, '5'), ('invalid-queue-count', 'QCOUNT ', 1, '129'), ('invalid-level-count', 'QCOUNT ', 2, '0'), ('invalid-deferred', 'QCOUNT ', 3, '2'), ('queue-overflow', 'Q ', 2, '4294967296'), ('queue-fraction', 'Q ', 5, '0.5'), ('queue-invalid-person', 'Q ', 6, '12'), ('queue-nonfinite', 'Q ', 7, 'nan')]:
 j=next(i for i,row in enumerate(rows) if row.startswith(prefix));parts=rows[j].split();parts[col]=value;lines=rows.copy();lines[j]=' '.join(parts);bad[name]=lines
for name,lines in bad.items():
 path=out/(name+'.txt');path.write_text('\n'.join(lines)+'\n');r=subprocess.run([str(core/'build/attention-test'),str(path)],capture_output=True,text=True);assert r.returncode==2,(name,r.returncode);print(name,'rejected',flush=True)
print(f'{len(faults)} compiled faults caught; {len(bad)} malformed captures rejected')
