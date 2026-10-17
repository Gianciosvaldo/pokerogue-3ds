#!/usr/bin/env python3
"""Compiled native-turn semantic faults; established comparisons remain intact."""
from pathlib import Path
import argparse,os,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('trace',type=Path);a=p.parse_args();a.trace=a.trace.resolve();r=Path(__file__).resolve().parents[2];env=os.environ.copy();assert env.get('TURNTEST_LOCALES')
exe=r/'battle-core/build/bad-dreams-doubles-test';assert subprocess.run([exe,a.trace],env=env).returncode==0
s=(r/'battle-core/src/ab_attrs.c').read_text();start=s.index('    case PR_AC_PostTurnHurtIfSleepingAbAttr: {');end=s.index('    case PR_AC_FetchBallAbAttr:',start);scope=s[start:end]
faults=[
 ('double-refusal','        bool eligible=false;','        if(bt->double_battle)return -1;\n        bool eligible=false;'),
 ('victim-blocker','pr_ab_params bp={.pokemon=p,','pr_ab_params bp={.pokemon=v,'),
 ('round-up','to_dmg_value(pr_get_stat(v,STAT_HP,true)/8.0)','ceil(pr_get_stat(v,STAT_HP,true)/8.0)'),
 ('one-victim','pr_queue_msg(bt,c->e,pr_msg1("abilityTriggers:badDreams",pr_arg_pokemon(bt,"pokemonName",v)));','pr_queue_msg(bt,c->e,pr_msg1("abilityTriggers:badDreams",pr_arg_pokemon(bt,"pokemonName",v)));break;'),
 ('victim-guard-in-loop','            bool cancelled=false;','            if(pr_has_ability_with_attr(bt,v,PR_AC_BlockNonDirectDamageAbAttr))continue;\n            bool cancelled=false;'),
 ('no-comatose','v->status!=STATUS_EFFECT_SLEEP && !pr_has_ability_active(bt,v,ABILITY_ID_COMATOSE)','v->status!=STATUS_EFFECT_SLEEP'),
 ('field-order','        pr_speed_iter it;\n        pr_speed_iter_init(bt,&it,p->is_player?ARENA_TAG_SIDE_ENEMY:ARENA_TAG_SIDE_PLAYER);\n        for(pr_pokemon *v;(v=pr_speed_iter_next(bt,&it));){','        for(int j=0;j<4;j++){pr_pokemon *v=bt->field[j];'),
]
flags=['-O2','-std=c99','-ffp-contract=off','-I'+str(r/'battle-core/include'),'-I'+str(r/'battle-core/gen/out'),'-I'+str(r/'battle-core/tests')]
with tempfile.TemporaryDirectory(prefix='bad-dreams-doubles-guards-',dir=r/'work') as tmp:
 tmp=Path(tmp)
 for name,old,new in faults:
  assert scope.count(old)==1,(name,scope.count(old));src=tmp/(name+'.c');obj=tmp/(name+'.o');binary=tmp/name;src.write_text(s[:start]+scope.replace(old,new)+s[end:])
  subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
  subprocess.run(['gcc',*flags,r/'battle-core/build/bad-dreams-doubles-replay.c',obj,r/'battle-core/build/libprcore.a','-lm','-o',binary],check=True,capture_output=True)
  result=subprocess.run([binary,a.trace],env=env,capture_output=True);assert result.returncode==1,(name,result.returncode,result.stdout[-500:],result.stderr[-500:]);print(name,'caught',flush=True)
 rows=a.trace.read_text().splitlines();bad={'missing-footer':rows[:-1],'extra-tail':rows+['JUNK'],'footer':rows[:-1]+['# DOUBLES_COMPLETE 1 0'],'version':[rows[0].replace(' 1 ',' 2 ',1)]+rows[1:],'too-short':[rows[0].replace('8192','8191')]+rows[1:]}
 for name,lines in bad.items():
  path=tmp/'bad.txt';path.write_text('\n'.join(lines)+'\n');result=subprocess.run([exe,path],env=env,capture_output=True);assert result.returncode==2,(name,result.returncode);print(name,'rejected',flush=True)
 print(f'{len(faults)} runtime faults caught; {len(bad)} malformed fixtures rejected')
