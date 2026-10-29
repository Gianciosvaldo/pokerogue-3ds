#!/usr/bin/env python3
"""Strict native arena-key matrix, constructor transport and failure controls."""
from pathlib import Path
import argparse,gzip,json,os,subprocess,tempfile,shutil
ROOT=Path(__file__).resolve().parents[2];BC=ROOT/'battle-core'
p=argparse.ArgumentParser();p.add_argument('--replay-only',action='store_true');args=p.parse_args()
def run(command,code=0,env=None):
 r=subprocess.run(list(map(str,command)),cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 if r.returncode!=code:raise RuntimeError(str(command)+'\n'+r.stdout[-5000:])
 return r.stdout
cc=os.environ.get('CC','cc');flags=['-O1','-std=c99','-ffp-contract=off','-I'+str(BC/'include'),'-I'+str(BC/'gen/out'),'-I'+str(BC/'tests')]
runner=BC/'tests/arena-capacity-test.c'
print(run(['make','-C',BC,'build/arena-capacity-test']),flush=True)
with tempfile.TemporaryDirectory(prefix='arena-capacity-')as directory:
 out=Path(directory);raw=gzip.decompress((BC/'tests/fixtures/arena-capacity.native.txt.gz').read_bytes());capture=out/'native.txt';capture.write_bytes(raw)
 locales=json.loads(gzip.decompress((BC/'tests/fixtures/arena-capacity.locales.json.gz').read_bytes()));assert set(locales)=={'battle','pokemon-info','battler-tags','arena-tag','ability-trigger','move-trigger','pokemon','ability','move'}
 (out/'locales').mkdir()
 for name,content in locales.items():
  assert type(content)==str;json.loads(content);(out/'locales'/f'{name}.json').write_text(content)
 env={**os.environ,'TURNTEST_LOCALES':str(out/'locales')};print(run([BC/'build/arena-capacity-test',capture],env=env),flush=True)
 # Transport fixture must remain a literal projection of the native constructor state.
 lines=raw.decode().splitlines(keepends=True);start=lines.index('TAGS 48\n');tags=[list(map(int,row.split()[1:]))for row in lines[start+1:start+49]]
 generated='/* Native public addTag constructor state, captured before the first action. */\nstatic const pr_arena_tag native_tags[]={\n'
 for t,side,turn,maxd,move,source,layers,count,rem,heal in tags:
  generated+=f' {{.type={t},.side={side},.turn_count={turn},.max_duration={maxd},.source_move={move},.source_id={source},.layers={layers},.source_count={count}}},\n'
 generated+='};\n';assert generated==(ROOT/'game-core/tests/fixtures/arena-capacity.native.inc').read_text()
 if os.environ.get('ARENA_CAPACITY_SANITIZE')=='1':
  san=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'];objects=[]
  for file in ['arena_tags.c','move_effects.c']:
   obj=out/(file+'.o');run([cc,*flags,*san,'-c',BC/'src'/file,'-o',obj]);objects.append(obj)
  exe=out/'san-test';run([cc,*flags,*san,runner,*objects,BC/'build/libprcore.a','-lm','-o',exe]);print(run([exe,capture],env=env),flush=True)
 if args.replay_only:raise SystemExit(0)
 faults=[
 ('arena_tags.c','bt->n_arena_tags >= PR_MAX_ARENA_TAGS','bt->n_arena_tags >= 24','restore old capacity'),
 ('arena_tags.c','if (bt->n_arena_tags >= PR_MAX_ARENA_TAGS) {\n        pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT);','if (bt->n_arena_tags >= PR_MAX_ARENA_TAGS) {','hide invalid-import overflow'),
 ('arena_tags.c','int i = find_on_side(bt, type, side);\n    if (i >= 0)', 'int i = find_on_side(bt, type, ARENA_TAG_SIDE_BOTH);\n    if (i >= 0)','collapse side-specific keys'),
 ('arena_tags.c','if (t->layers >= pr_arena_max_layers(t->type)) return;','return;','drop hazard overlaps'),
 ('arena_tags.c','if (!match || !pr_arena_tag_on_side(t, side)) {','if (!match) {','remove wrong side'),
 ('arena_tags.c','bt->arena_tags[i] = bt->arena_tags[keep[i]]','bt->arena_tags[i] = bt->arena_tags[i]','lose retained tag compaction'),
 ('arena_tags.c','t->turn_count = t->max_duration = (int16_t)turn_count;','t->turn_count = t->max_duration = (int16_t)(turn_count+1);','change constructor durations'),
 ('move_effects.c','t->turn_count, t->source_move, t->type, t->layers','t->turn_count, t->source_move, t->type, 1','lose swapped hazard layers'),
 ('move_effects.c','t->source_id,\n                        t->turn_count','-1,\n                        t->turn_count','lose swapped source identity'),
 ('move_effects.c','t->source_id, sides[1 - s], true','t->source_id, sides[s], true','keep swapped tag on original side'),
 ('move_effects.c','t->type, t->turn_count, t->source_move, t->source_id','t->type, 1, t->source_move, t->source_id','lose swapped duration'),
 ]
 for n,(file,old,new,label)in enumerate(faults):
  folder=out/f'fault-{n}';folder.mkdir();source=(BC/'src'/file).read_text();assert source.count(old)==1,(label,source.count(old));src=folder/file;src.write_text(source.replace(old,new));obj=folder/'fault.o';exe=folder/'test'
  run([cc,*flags,'-c',src,'-o',obj]);run([cc,*flags,runner,obj,BC/'build/libprcore.a','-lm','-o',exe]);result=run([exe,capture],1,env)
  if n==0:print(result.splitlines()[-1],flush=True)
  print('Caught compiled fault:',label,flush=True);shutil.rmtree(folder)
 bad=[]
 def first(data,prefix):return next(i for i,v in enumerate(data)if v.startswith(prefix))
 def malformed(label,edit):
  data=lines.copy();edit(data);bad.append((label,''.join(data)))
 malformed('wrong count',lambda d:d.__setitem__(0,'ARENA_CAPACITY 1 31\n'))
 malformed('missing seed',lambda d:d.pop(1))
 malformed('short metadata',lambda d:d.__setitem__(2,'C 0 0 0\n'))
 malformed('invalid RNG',lambda d:d.__setitem__(3,'R 1 nan 0 0\n'))
 malformed('short mon',lambda d:d.__setitem__(first(d,'I 0 '),'I 0 19\n'))
 malformed('wrong operation count',lambda d:d.__setitem__(first(d,'OPS '),'OPS 48\n'))
 malformed('invalid side',lambda d:d.__setitem__(first(d,'ADD '),'ADD 1 3 101 1\n'))
 malformed('invalid tag type',lambda d:d.__setitem__(first(d,'TAG '),'TAG 99 0 5 5 150 101 1 0 0 0\n'))
 malformed('unknown phase',lambda d:d.__setitem__(next(i for i,v in enumerate(d)if v.startswith('Q ')and ' MessagePhase 'in v),d[next(i for i,v in enumerate(d)if v.startswith('Q ')and ' MessagePhase 'in v)].replace('MessagePhase','MadeUpPhase')))
 malformed('missing footer',lambda d:d.pop())
 bad.append(('trailing data',raw.decode()+'garbage\n'))
 for label,value in bad:
  altered=out/'malformed.txt';altered.write_text(value);run([BC/'build/arena-capacity-test',altered],2,env);altered.unlink()
 def forged(label,prefix,column,value):
  data=lines.copy();i=first(data,prefix);v=data[i].split();v[column]=str(value);data[i]=' '.join(v)+'\n';altered=out/'forged.txt';altered.write_text(''.join(data));run([BC/'build/arena-capacity-test',altered],1,env);altered.unlink();print('Rejected forged observation:',label,flush=True)
 forged('after HP','O 0 ',8,66);forged('tag side','TAG ',2,2);forged('global RNG','RO ',1,2);forged('tag source','TAG ',6,104)
 print(f'Arena capacity proof: {len(faults)} compiled faults, {len(bad)} malformed captures, 4 forged observations rejected')
