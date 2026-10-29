#!/usr/bin/env python3
"""Actual Substitute maximum HP callbacks, strict capture transport and faults."""
from pathlib import Path
import argparse,gzip,json,os,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2];BC=ROOT/'battle-core';GC=ROOT/'game-core'
p=argparse.ArgumentParser();p.add_argument('--replay-only',action='store_true');args=p.parse_args()
def run(command,code=0,env=None):
 r=subprocess.run(list(map(str,command)),cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 if r.returncode!=code:raise RuntimeError(str(command)+'\n'+r.stdout[-5000:])
 return r.stdout
cc=os.environ.get('CC','cc');flags=['-O1','-std=c99','-ffp-contract=off','-I'+str(BC/'include'),'-I'+str(BC/'gen/out'),'-I'+str(BC/'build')]
runner=BC/'tests/substitute-max-hp-test.c'
print(run(['make','-C',BC,'build/substitute-max-hp-test']),flush=True)
with tempfile.TemporaryDirectory(prefix='substitute-max-hp-')as directory:
 out=Path(directory);raw=gzip.decompress((BC/'tests/fixtures/substitute-max-hp.native.txt.gz').read_bytes());capture=out/'native.txt';capture.write_bytes(raw)
 locales=json.loads(gzip.decompress((BC/'tests/fixtures/substitute-max-hp.locales.json.gz').read_bytes()));assert set(locales)=={'battle','pokemon-info','battler-tags','arena-tag','ability-trigger','move-trigger','pokemon','ability','move'}
 (out/'locales').mkdir()
 for name,content in locales.items():
  assert type(content)==str;json.loads(content);(out/'locales'/f'{name}.json').write_text(content)
 env={**os.environ,'TURNTEST_LOCALES':str(out/'locales')};print(run([BC/'build/substitute-max-hp-test',capture],env=env),flush=True)
 if os.environ.get('SUBSTITUTE_MAX_HP_SANITIZE')=='1':
  san=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'];objects=[]
  for file in ['battler_tags.c','move_effects.c']:
   obj=out/(file+'.o');run([cc,*flags,*san,'-c',BC/'src'/file,'-o',obj]);objects.append(obj)
  exe=out/'san-test';run([cc,*flags,*san,runner,*objects,BC/'build/libprcore.a','-lm','-o',exe]);print(run([exe,capture],env=env),flush=True)
 if args.replay_only:raise SystemExit(0)
 faults=[
 ('move_effects.c','double cost = pr_get_stat(user, STAT_HP, true) * x->hpCost;','double cost = pr_get_stat(user, STAT_HP, false) * x->hpCost;','cost reads temporary HP'),
 ('battler_tags.c','t->hp = floor(pr_get_stat(source, STAT_HP, true) / 4.0);','t->hp = floor(pr_get_stat(source, STAT_HP, false) / 4.0);','doll reads temporary HP'),
 ('move_effects.c','cost = x->roundUp ? ceil(cost) : (floor(cost) < 1 ? 1 : floor(cost));','cost = floor(cost);','lose cost rounding and minimum'),
 ('battler_tags.c','t->hp = floor(pr_get_stat(source, STAT_HP, true) / 4.0);','t->hp = ceil(pr_get_stat(source, STAT_HP, true) / 4.0);','round doll HP up'),
 ('battler_tags.c','t->source_move == MOVE_ID_SHED_TAIL ? "battlerTags:shedTailOnAdd" : "battlerTags:substituteOnAdd"','"battlerTags:substituteOnAdd"','lose Shed Tail text'),
 ('battler_tags.c','pr_unshift_pokemon_anim(bt, e, pr_battler_index(bt, p), POKEMON_ANIM_TYPE_SUBSTITUTE_ADD);','pr_unshift_pokemon_anim(bt, e, pr_battler_index(bt, p), POKEMON_ANIM_TYPE_SUBSTITUTE_REMOVE);','wrong substitute animation'),
 ('move_effects.c','pr_add_tag(bt, e, user, BATTLER_TAG_TYPE_SUBSTITUTE, 0, move, user->id);','pr_add_tag(bt, e, user, BATTLER_TAG_TYPE_SUBSTITUTE, 0, MOVE_ID_SUBSTITUTE, user->id);','lose source move identity'),
 ]
 for n,(file,old,new,label)in enumerate(faults):
  folder=out/f'fault-{n}';folder.mkdir();source=(BC/'src'/file).read_text();assert source.count(old)==1,(label,source.count(old));src=folder/file;src.write_text(source.replace(old,new));obj=folder/'fault.o';exe=folder/'test'
  run([cc,*flags,'-c',src,'-o',obj]);run([cc,*flags,runner,obj,BC/'build/libprcore.a','-lm','-o',exe]);run([exe,capture],1,env);print('Caught compiled fault:',label,flush=True);shutil.rmtree(folder)
 lines=raw.decode().splitlines(keepends=True);bad=[]
 def malformed(label,edit):
  data=lines.copy();edit(data);bad.append((label,''.join(data)))
 def first(data,prefix):return next(i for i,v in enumerate(data)if v.startswith(prefix))
 malformed('wrong count',lambda d:d.__setitem__(0,'SUBSTITUTE_MAX_HP 1 575\n'))
 malformed('missing seed',lambda d:d.pop(1))
 malformed('wrong metadata',lambda d:d.__setitem__(2,'C 0 1 1 19 1 0 0\n'))
 malformed('invalid RNG',lambda d:d.__setitem__(3,'R 1 nan 0 0\n'))
 malformed('short mon vector',lambda d:d.__setitem__(first(d,'I 0 '),'I 0 19\n'))
 malformed('duplicate case',lambda d:d.__setitem__(first(d,'C 1 '),d[first(d,'C 1 ')].replace('C 1 ','C 0 ',1)))
 malformed('invalid tag payload',lambda d:d.__setitem__(first(d,'SUB '),'SUB 0 1 -1 0 164 101\n'))
 malformed('unknown phase',lambda d:d.__setitem__(next(i for i,v in enumerate(d)if v.startswith('Q ')and ' MessagePhase 'in v),d[next(i for i,v in enumerate(d)if v.startswith('Q ')and ' MessagePhase 'in v)].replace('MessagePhase','MadeUpPhase')))
 malformed('invalid animation',lambda d:d.__setitem__(first(d,'P '),'P 0 99\n'))
 malformed('missing footer',lambda d:d.pop())
 bad.append(('trailing data',raw.decode()+'garbage\n'))
 for label,text in bad:
  altered=out/'malformed.txt';altered.write_text(text);run([BC/'build/substitute-max-hp-test',altered],2,env);altered.unlink()
 forged=[]
 def forge(label,edit):
  data=lines.copy();edit(data);forged.append((label,''.join(data)))
 def alter_vector(data,prefix,column,value):
  i=first(data,prefix);v=data[i].split();v[column]=str(value);data[i]=' '.join(v)+'\n'
 forge('after HP',lambda d:alter_vector(d,'O 0 ',8,66))
 forge('doll HP',lambda d:alter_vector(d,'SUB 0 ',3,3))
 forge('global RNG',lambda d:alter_vector(d,'RO ',1,2))
 forge('damage amount',lambda d:alter_vector(d,'D ',2,77))
 for label,text in forged:
  altered=out/'forged.txt';altered.write_text(text);run([BC/'build/substitute-max-hp-test',altered],1,env);altered.unlink();print('Rejected forged observation:',label,flush=True)
 print(f'Substitute maximum HP proof: {len(faults)} compiled faults, {len(bad)} malformed captures, {len(forged)} forged observations rejected')
