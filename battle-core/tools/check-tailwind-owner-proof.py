#!/usr/bin/env python3
"""Actual Court Change/Tailwind callbacks, strict capture transport and faults."""
from pathlib import Path
import argparse,gzip,json,os,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2];BC=ROOT/'battle-core';GC=ROOT/'game-core'
p=argparse.ArgumentParser();p.add_argument('--replay-only',action='store_true');args=p.parse_args()
def run(command,code=0,env=None):
 r=subprocess.run(list(map(str,command)),cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 if r.returncode!=code:raise RuntimeError(str(command)+'\n'+r.stdout[-5000:])
 return r.stdout
cc=os.environ.get('CC','cc');flags=['-O1','-std=c99','-ffp-contract=off','-I'+str(BC/'include'),'-I'+str(BC/'gen/out')]
runner=BC/'tests/tailwind-owner-test.c'
print(run(['make','-C',BC,'build/tailwind-owner-test']),flush=True)
with tempfile.TemporaryDirectory(prefix='tailwind-owner-')as directory:
 out=Path(directory);raw=gzip.decompress((BC/'tests/fixtures/tailwind-owner.native.txt.gz').read_bytes());capture=out/'native.txt';capture.write_bytes(raw)
 locales=json.loads(gzip.decompress((BC/'tests/fixtures/tailwind-owner.locales.json.gz').read_bytes()));assert set(locales)=={'battle','pokemon-info','battler-tags','arena-tag','ability-trigger','move-trigger','pokemon','ability','move'}
 (out/'locales').mkdir()
 for name,content in locales.items():
  assert type(content)==str;json.loads(content);(out/'locales'/f'{name}.json').write_text(content)
 env={**os.environ,'TURNTEST_LOCALES':str(out/'locales')};print(run([BC/'build/tailwind-owner-test',capture],env=env),flush=True)
 if args.replay_only:raise SystemExit(0)
 faults=[
 ('arena_tags.c','pr_pokemon *source = pr_pokemon_by_id(bt, t->source_id);','pr_pokemon *source = pr_field_pokemon_by_id(bt, t->source_id);','lose off-field source'),
 ('arena_tags.c','t->source_id >= 0 && (!bt->n_party[0] || !bt->n_party[1])','t->source_id >= 0','flag genuinely missing retained source'),
 ('arena_tags.c','t->source_id >= 0 && (!bt->n_party[0] || !bt->n_party[1])','false','admit missing party context'),
 ('arena_tags.c','t->source_id >= 0 && (!bt->n_party[0] || !bt->n_party[1])','(!bt->n_party[0] || !bt->n_party[1])','flag an undefined source'),
 ('arena_tags.c','source->is_player ? ARENA_TAG_SIDE_PLAYER : ARENA_TAG_SIDE_ENEMY','t->side','use swapped arena side instead of source side'),
 ('arena_tags.c','&& !pr_get_battler_tag(p, BATTLER_TAG_TYPE_CHARGED)','&& true','recharge already charged recipients'),
 ('arena_tags.c','&atk, 1, 1, p)','&atk, 1, 2, p)','double Wind Rider stages'),
 ('arena_tags.c','&atk, 1, 1, p)','&atk, 1, 1, source)','use retained caster as stat source'),
 ('arena_tags.c','pr_queue_ability_display(bt, e, p, false, true);','pr_queue_ability_display(bt, e, p, true, true);','display passive ability instead of primary'),
 ('arena_tags.c','pr_speed_iter it;\n    pr_speed_iter_init(bt, &it, source->is_player','pr_rng_seed_int(&bt->rng,100,0);pr_speed_iter it;\n    pr_speed_iter_init(bt, &it, source->is_player','consume unrelated global RNG'),
 ]
 for n,(file,old,new,label)in enumerate(faults):
  folder=out/f'fault-{n}';folder.mkdir();source=(BC/'src'/file).read_text();assert source.count(old)==1,(label,source.count(old));src=folder/file;src.write_text(source.replace(old,new));obj=folder/'fault.o';exe=folder/'test'
  run([cc,*flags,'-c',src,'-o',obj]);run([cc,*flags,runner,obj,BC/'build/libprcore.a','-lm','-o',exe]);run([exe,capture],1,env);print('Caught compiled fault:',label,flush=True);shutil.rmtree(folder)
 lines=raw.decode().splitlines(keepends=True);bad=[]
 def malformed(label,edit):
  data=lines.copy();edit(data);bad.append((label,''.join(data)))
 def first(data,prefix):return next(i for i,v in enumerate(data)if v.startswith(prefix))
 malformed('wrong count',lambda d:d.__setitem__(0,'TAILWIND_OWNER 1 2047\n'))
 malformed('missing seed',lambda d:d.pop(1))
 malformed('wrong input shape',lambda d:d.__setitem__(2,d[2].replace('C 0 0 0 0','C 0 0 0 1',1)))
 malformed('short party',lambda d:d.__setitem__(3,'P 0 1 2 3 4\n'))
 malformed('invalid RNG',lambda d:d.__setitem__(4,'R 1 nan 0 0\n'))
 malformed('short mon vector',lambda d:d.__setitem__(first(d,'I 0 '),'I 0 19\n'))
 malformed('duplicate case number',lambda d:d.__setitem__(first(d,'C 1 '),d[first(d,'C 1 ')].replace('C 1 ','C 0 ',1)))
 malformed('missing initial tag record',lambda d:d.pop(first(d,'ITAGS ')))
 malformed('unknown phase',lambda d:d.__setitem__(next(i for i,v in enumerate(d)if v.startswith('Q ')and ' MessagePhase 'in v),d[next(i for i,v in enumerate(d)if v.startswith('Q ')and ' MessagePhase 'in v)].replace('MessagePhase','MadeUpPhase')))
 malformed('missing footer',lambda d:d.pop())
 bad.append(('trailing data',raw.decode()+'garbage\n'))
 for label,text in bad:
  altered=out/'malformed.txt';altered.write_text(text);run([BC/'build/tailwind-owner-test',altered],2,env);altered.unlink()
 forged=[]
 def forge(label,edit):
  data=lines.copy();edit(data);forged.append((label,''.join(data)))
 def alter_vector(data,prefix,column,value):
  i=first(data,prefix);v=data[i].split();v[column]=str(value);data[i]=' '.join(v)+'\n'
 forge('after HP',lambda d:alter_vector(d,'O 0 ',8,66))
 forge('tag side',lambda d:alter_vector(d,'TAG ',2,2))
 forge('global RNG',lambda d:alter_vector(d,'RO ',1,2))
 def stat_source(data):
  i=next(i for i,v in enumerate(data)if v.startswith('Q D ')and ' StatStageChangePhase 'in v);v=data[i].split();v[-1]='0'if v[-1]!='0'else'1';data[i]=' '.join(v)+'\n'
 forge('queued stat source',stat_source)
 for label,text in forged:
  altered=out/'forged.txt';altered.write_text(text);run([BC/'build/tailwind-owner-test',altered],1,env);altered.unlink();print('Rejected forged observation:',label,flush=True)
 print(f'Tailwind proof: {len(faults)} compiled faults, {len(bad)} malformed captures, {len(forged)} forged observations rejected')
