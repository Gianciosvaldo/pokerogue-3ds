#!/usr/bin/env python3
"""Actual Maximum HP move owners callbacks, strict capture transport and faults."""
from pathlib import Path
import argparse,gzip,json,os,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2];BC=ROOT/'battle-core';GC=ROOT/'game-core'
p=argparse.ArgumentParser();p.add_argument('--replay-only',action='store_true');args=p.parse_args()
def run(command,code=0,env=None):
 r=subprocess.run(list(map(str,command)),cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 if r.returncode!=code:raise RuntimeError(str(command)+'\n'+r.stdout[-5000:])
 return r.stdout
cc=os.environ.get('CC','cc');flags=['-O1','-std=c99','-ffp-contract=off','-I'+str(BC/'include'),'-I'+str(BC/'gen/out'),'-I'+str(BC/'build')]
runner=BC/'tests/max-hp-moves-test.c'
print(run(['make','-C',BC,'build/max-hp-moves-test']),flush=True)
with tempfile.TemporaryDirectory(prefix='max-hp-moves-')as directory:
 out=Path(directory);raw=gzip.decompress((BC/'tests/fixtures/max-hp-moves.native.txt.gz').read_bytes());capture=out/'native.txt';capture.write_bytes(raw)
 locales=json.loads(gzip.decompress((BC/'tests/fixtures/max-hp-moves.locales.json.gz').read_bytes()));assert set(locales)=={'battle','pokemon-info','battler-tags','arena-tag','ability-trigger','move-trigger','pokemon','ability','move','status-effect'}
 (out/'locales').mkdir()
 for name,content in locales.items():
  assert type(content)==str;json.loads(content);(out/'locales'/f'{name}.json').write_text(content)
 env={**os.environ,'TURNTEST_LOCALES':str(out/'locales')};print(run([BC/'build/max-hp-moves-test',capture],env=env),flush=True)
 if os.environ.get('MAX_HP_MOVES_SANITIZE')=='1':
  san=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'];objects=[]
  for file in ['move_effects.c']:
   obj=out/(file+'.o');run([cc,*flags,*san,'-c',BC/'src'/file,'-o',obj]);objects.append(obj)
  exe=out/'san-test';run([cc,*flags,*san,runner,*objects,BC/'build/libprcore.a','-lm','-o',exe]);print(run([exe,capture],env=env),flush=True)
 if args.replay_only:raise SystemExit(0)
 faults=[
 ('move_effects.c','double base = x->useHp ? pr_get_stat(user, STAT_HP, true) : user->total_damage_dealt;','double base = x->useHp ? pr_get_stat(user, STAT_HP, false) : user->total_damage_dealt;','recoil reads temporary HP'),
 ('move_effects.c','double damage = floor(pr_get_stat(ally, STAT_HP, true) / 16.0);','double damage = floor(pr_get_stat(ally, STAT_HP, false) / 16.0);','Flame Burst reads temporary HP'),
 ('move_effects.c','\n        double d = floor(pr_get_stat(user, STAT_HP, true) / 2.0);','\n        double d = floor(pr_get_stat(user, STAT_HP, false) / 2.0);','Steel Beam reads temporary HP'),
 ('move_effects.c','pr_unshift_heal(bt, e, pr_battler_index(bt, user), pr_get_stat(user, STAT_HP, true), 0);','pr_unshift_heal(bt, e, pr_battler_index(bt, user), pr_get_stat(user, STAT_HP, false), 0);','Rest reads temporary HP'),
 ('move_effects.c','\n            double d = floor(pr_get_stat(user, STAT_HP, true) / 2.0);','\n            double d = floor(pr_get_stat(user, STAT_HP, false) / 2.0);','Curse reads temporary HP'),
 ('move_effects.c','js_round(pr_get_stat(on, STAT_HP, true) * ratio)','js_round(pr_get_stat(on, STAT_HP, false) * ratio)','healing reads temporary HP'),
 ('move_effects.c','user->damage_taken += (int32_t)recoil;','user->damage_taken += 0;','lose recoil bookkeeping'),
 ('move_effects.c','PR_HEAL_MESSAGE | (show_anim ? 0 : PR_HEAL_SKIP_ANIM)','(show_anim ? 0 : PR_HEAL_SKIP_ANIM)','lose healing message'),
 ('move_effects.c','P(RestAttr, a)->duration','(P(RestAttr, a)->duration + 1)','change Rest sleep duration'),
 ]
 for n,(file,old,new,label)in enumerate(faults):
  folder=out/f'fault-{n}';folder.mkdir();source=(BC/'src'/file).read_text();assert source.count(old)==1,(label,source.count(old));src=folder/file;src.write_text(source.replace(old,new));obj=folder/'fault.o';exe=folder/'test'
  run([cc,*flags,'-c',src,'-o',obj]);run([cc,*flags,runner,obj,BC/'build/libprcore.a','-lm','-o',exe]);run([exe,capture],1,env);print('Caught compiled fault:',label,flush=True);shutil.rmtree(folder)
 lines=raw.decode().splitlines(keepends=True);bad=[]
 def malformed(label,edit):
  data=lines.copy();edit(data);bad.append((label,''.join(data)))
 def first(data,prefix):return next(i for i,v in enumerate(data)if v.startswith(prefix))
 malformed('wrong count',lambda d:d.__setitem__(0,'MAX_HP_MOVES 1 1343\n'))
 malformed('missing seed',lambda d:d.pop(1))
 malformed('wrong metadata',lambda d:d.__setitem__(2,'C 0 1 1 19 1 0 0\n'))
 malformed('invalid RNG',lambda d:d.__setitem__(3,'R 1 nan 0 0\n'))
 malformed('short mon vector',lambda d:d.__setitem__(first(d,'I 0 '),'I 0 19\n'))
 malformed('duplicate case',lambda d:d.__setitem__(first(d,'C 1 '),d[first(d,'C 1 ')].replace('C 1 ','C 0 ',1)))
 malformed('negative turn data',lambda d:d.__setitem__(first(d,'TD '),'TD 0 -1 0 0 0\n'))
 malformed('unknown phase',lambda d:d.__setitem__(next(i for i,v in enumerate(d)if v.startswith('Q ')and ' MessagePhase 'in v),d[next(i for i,v in enumerate(d)if v.startswith('Q ')and ' MessagePhase 'in v)].replace('MessagePhase','MadeUpPhase')))
 malformed('invalid payload',lambda d:d.__setitem__(first(d,'PAY '),'PAY 0 0 MadeUpPhase 0\n'))
 malformed('missing footer',lambda d:d.pop())
 bad.append(('trailing data',raw.decode()+'garbage\n'))
 for label,text in bad:
  altered=out/'malformed.txt';altered.write_text(text);run([BC/'build/max-hp-moves-test',altered],2,env);altered.unlink()
 forged=[]
 def forge(label,edit):
  data=lines.copy();edit(data);forged.append((label,''.join(data)))
 def alter_vector(data,prefix,column,value):
  i=first(data,prefix);v=data[i].split();v[column]=str(value);data[i]=' '.join(v)+'\n'
 forge('after HP',lambda d:alter_vector(d,'O 0 ',8,66))
 forge('turn damage',lambda d:alter_vector(d,'TD 0 ',2,77))
 forge('global RNG',lambda d:alter_vector(d,'RO ',1,2))
 def heal_amount(data):
  i=next(i for i,v in enumerate(data)if v.startswith('PAY ')and ' PokemonHealPhase 'in v);v=data[i].split();v[4]='77';data[i]=' '.join(v)+'\n'
 forge('queued heal amount',heal_amount)
 for label,text in forged:
  altered=out/'forged.txt';altered.write_text(text);run([BC/'build/max-hp-moves-test',altered],1,env);altered.unlink();print('Rejected forged observation:',label,flush=True)
 print(f'Maximum HP move owners proof: {len(faults)} compiled faults, {len(bad)} malformed captures, {len(forged)} forged observations rejected')
