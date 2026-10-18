#!/usr/bin/env python3
"""Compile actual Helping Hand faults and reject malformed observed traces."""
from pathlib import Path
import argparse,os,subprocess,re
p=argparse.ArgumentParser();p.add_argument('api',type=Path);p.add_argument('turns',type=Path);a=p.parse_args();r=Path(__file__).resolve().parents[2];out=r/'battle-core/build/helping-hand-guards';out.mkdir(parents=True,exist_ok=True);env=os.environ.copy();assert env.get('TURNTEST_LOCALES')
mutations={
 'missing-registration':('battler_tags.c','    case BATTLER_TAG_TYPE_HELPING_HAND: /* HelpingHandTag */\n','',False),
 'wrong-duration':('battler_tags.c','case BATTLER_TAG_TYPE_HELPING_HAND:\n        t.turn_count=1;','case BATTLER_TAG_TYPE_HELPING_HAND:\n        t.turn_count=2;',False),
 'wrong-source-move':('battler_tags.c','t.source_move=MOVE_ID_HELPING_HAND;','t.source_move=MOVE_ID_TACKLE;',False),
 'lost-source':('battler_tags.c','t.source_move=MOVE_ID_HELPING_HAND;\n        t.source_id=source_id;','t.source_move=MOVE_ID_HELPING_HAND;\n        t.source_id=-1;',False),
 'missing-message':('battler_tags.c','if(source)pr_queue_msg(bt,e,pr_msg2("battlerTags:helpingHandOnAdd",','if(false)pr_queue_msg(bt,e,pr_msg2("battlerTags:helpingHandOnAdd",',False),
 'wrong-recipient-affix':('battler_tags.c','pr_arg_pokemon(bt,"pokemonNameWithAffix",source),pr_arg_pokemon(bt,"pokemonName",p)','pr_arg_pokemon(bt,"pokemonNameWithAffix",source),pr_arg_pokemon_name(bt,"pokemonName",p)',False),
 'wrong-source-link':('battler_tags.c','return is_trap(type) || type == BATTLER_TAG_TYPE_INFATUATED;','return type==BATTLER_TAG_TYPE_HELPING_HAND || is_trap(type) || type == BATTLER_TAG_TYPE_INFATUATED;',False),
 'missing-expiry':('battler_tags.c','return (pr_battler_tags_info[type].lapse_mask >> lapse_type) & 1u;','return type==BATTLER_TAG_TYPE_HELPING_HAND?false:(pr_battler_tags_info[type].lapse_mask >> lapse_type) & 1u;',False),
 'missing-damage-boost':('damage.c','if (pr_has_battler_tag_class(bt, src, PR_TAGCLASS_HELPING_HAND)) power *= 1.5;','if (pr_has_battler_tag_class(bt, src, PR_TAGCLASS_HELPING_HAND)) power *= 1.0;',True),
}
for name,(file,old,new,turns) in mutations.items():
 s=(r/'battle-core/src'/file).read_text();assert s.count(old)==1,(name,s.count(old));src=out/(name+'.c');obj=out/(name+'.o');exe=out/name;src.write_text(s.replace(old,new));flags=['-O2','-std=c99','-ffp-contract=off','-I'+str(r/'battle-core/include'),'-I'+str(r/'battle-core/gen/out')];subprocess.run(['gcc',*flags,'-c',str(src),'-o',str(obj)],check=True);subprocess.run(['gcc',*flags,str(r/'battle-core/tests'/('doublestest.c' if turns else 'helping-hand-test.c')),str(obj),str(r/'battle-core/build/libprcore.a'),'-lm','-o',str(exe)],check=True);x=subprocess.run([str(exe),str((a.turns if turns else a.api).resolve())],env=env,capture_output=True,text=True);(out/(name+'.log')).write_text(x.stdout+x.stderr);assert x.returncode==1,(name,x.returncode)
 if turns:
  m=re.search(r'^covered \d+: pass \d+, fail (\d+)',x.stdout,re.M);assert m and int(m[1])>0,(name,'no covered gameplay mismatch')
 print(name,'caught',flush=True)
rows=a.api.read_text().splitlines();bad={'missing-footer':rows[:-1],'extra-tail':rows+['JUNK'],'wrong-version':[rows[0].replace(' 1 ',' 2 ',1)]+rows[1:],'wrong-footer':rows[:-1]+['HELPINGHAND_COMPLETE 1']}
for name,prefix,col,value in [('fractional-case','C ',1,'0.5'),('invalid-target','C ',2,'5'),('overflow-source','C ',3,'4294967296'),('invalid-op-count','C ',7,'0'),('invalid-level','MON ',4,'256'),('invalid-nature','MON ',5,'25'),('unknown-op','OP ',1,'9'),('invalid-tag-count','OP ',3,'2'),('invalid-duration','T ',1,'2'),('invalid-lapse','T ',6,'9')]:
 i=next(i for i,v in enumerate(rows) if v.startswith(prefix));v=rows[i].split();v[col]=value;line=rows.copy();line[i]=' '.join(v);bad[name]=line
for name,line in bad.items():
 path=out/(name+'.txt');path.write_text('\n'.join(line)+'\n');x=subprocess.run([str(r/'battle-core/build/helping-hand-test'),str(path)],env=env,capture_output=True,text=True);assert x.returncode==2,(name,x.returncode);print(name,'rejected',flush=True)
print(f'{len(mutations)} compiled mutants caught; {len(bad)} malformed captures rejected')
