#!/usr/bin/env python3
"""Actual Full-party battler-tag source callbacks, strict capture transport and faults."""
from pathlib import Path
import argparse,gzip,json,os,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2];BC=ROOT/'battle-core';GC=ROOT/'game-core'
p=argparse.ArgumentParser();p.add_argument('--replay-only',action='store_true');args=p.parse_args()
def run(command,code=0,env=None):
 r=subprocess.run(list(map(str,command)),cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 if r.returncode!=code:raise RuntimeError(str(command)+'\n'+r.stdout[-5000:])
 return r.stdout
cc=os.environ.get('CC','cc');flags=['-O1','-std=c99','-ffp-contract=off','-I'+str(BC/'include'),'-I'+str(BC/'gen/out'),'-I'+str(BC/'build')]
runner=BC/'tests/tag-party-sources-test.c'
print(run(['make','-C',BC,'build/tag-party-sources-test']),flush=True)
with tempfile.TemporaryDirectory(prefix='tag-party-sources-')as directory:
 out=Path(directory);raw=gzip.decompress((BC/'tests/fixtures/tag-party-sources.native.txt.gz').read_bytes());capture=out/'native.txt';capture.write_bytes(raw)
 locales=json.loads(gzip.decompress((BC/'tests/fixtures/tag-party-sources.locales.json.gz').read_bytes()));assert set(locales)=={'battle','pokemon-info','battler-tags','arena-tag','ability-trigger','move-trigger','pokemon','ability','move'}
 (out/'locales').mkdir()
 for name,content in locales.items():
  assert type(content)==str;json.loads(content);(out/'locales'/f'{name}.json').write_text(content)
 env={**os.environ,'TURNTEST_LOCALES':str(out/'locales')};print(run([BC/'build/tag-party-sources-test',capture],env=env),flush=True)
 if os.environ.get('TAG_PARTY_SOURCES_SANITIZE')=='1':
  san=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'];objects=[]
  for file in ['battler_tags.c']:
   obj=out/(file+'.o');run([cc,*flags,*san,'-c',BC/'src'/file,'-o',obj]);objects.append(obj)
  exe=out/'san-test';run([cc,*flags,*san,runner,*objects,BC/'build/libprcore.a','-lm','-o',exe]);print(run([exe,capture],env=env),flush=True)
 if args.replay_only:raise SystemExit(0)
 faults=[
 ('battler_tags.c','case BATTLER_TAG_TYPE_OCTOLOCK: {                     /* TrappedTag.canAdd */\n        pr_pokemon *source = tag_source(bt, t);\n        if (!source) return false;','case BATTLER_TAG_TYPE_OCTOLOCK: {                     /* TrappedTag.canAdd */\n        pr_pokemon *source = pr_field_pokemon_by_id(bt, t->source_id);\n        if (!source) return false;','Trapped and Octolock search field only'),
 ('battler_tags.c','pr_pokemon *source = tag_source(bt, t);\n        if (!source) return;','pr_pokemon *source = pr_field_pokemon_by_id(bt, t->source_id);\n        if (!source) return;','Seed searches field only'),
 ('battler_tags.c','pr_pokemon *source = pr_pokemon_by_id(bt, t->source_id);\n        if (!source) {','pr_pokemon *source = pr_field_pokemon_by_id(bt, t->source_id);\n        if (!source) {','Substitute searches field only'),
 ('history.c','if (id < 0) return NULL;','if (id <= 0) return NULL;','reject valid ID zero'),
 ('battler_tags.c','if (!source && t->source_id >= 0 && (!bt->n_party[0] || !bt->n_party[1]))','if (!source && t->source_id >= 0)','flag known absent party caster'),
 ('battler_tags.c','if (!source && t->source_id >= 0 && (!bt->n_party[0] || !bt->n_party[1]))','if (false)','lose incomplete party guard'),
 ('battler_tags.c','that incomplete doll as a normal C zero-HP Substitute. */\n            pr_note_unported(bt, PR_FEAT_PARTY);','that incomplete doll as a normal C zero-HP Substitute. */\n            if (t->source_id >= 0) pr_note_unported(bt, PR_FEAT_PARTY);','admit undefined-source doll'),
 ('battler_tags.c','t->hp = floor(pr_get_stat(source, STAT_HP, true) / 4.0);','t->hp = floor(pr_get_stat(p, STAT_HP, true) / 4.0);','use recipient HP instead of caster HP'),
 ('battler_tags.c','t->source_index = (int8_t)pr_battler_index(bt, source);','t->source_index = (int8_t)pr_battler_index(bt, p);','seed caches recipient instead of caster'),
 ('battler_tags.c','pr_pokemon *source = pr_pokemon_by_id(bt, t->source_id);\n    if (!source &&','pr_pokemon *source = pr_pokemon_by_id(bt, t->source_id);\n    if (source && source->hp == 0) source = NULL;\n    if (!source &&','reject fainted party caster'),
 ]
 for n,(file,old,new,label)in enumerate(faults):
  folder=out/f'fault-{n}';folder.mkdir();source=(BC/'src'/file).read_text();assert source.count(old)==1,(label,source.count(old));src=folder/file;src.write_text(source.replace(old,new));obj=folder/'fault.o';exe=folder/'test'
  run([cc,*flags,'-c',src,'-o',obj]);run([cc,*flags,runner,obj,BC/'build/libprcore.a','-lm','-o',exe]);run([exe,capture],1,env);print('Caught compiled fault:',label,flush=True);shutil.rmtree(folder)
 lines=raw.decode().splitlines(keepends=True);bad=[]
 def malformed(label,edit):
  data=lines.copy();edit(data);bad.append((label,''.join(data)))
 def first(data,prefix):return next(i for i,v in enumerate(data)if v.startswith(prefix))
 malformed('wrong count',lambda d:d.__setitem__(0,'TAG_PARTY_SOURCES 1 767\n'))
 malformed('missing seed',lambda d:d.pop(1))
 malformed('wrong metadata',lambda d:d.__setitem__(2,'C 0 1 0 0 0 0 30\n'))
 malformed('invalid RNG',lambda d:d.__setitem__(first(d,'R '),'R 1 nan 0 0\n'))
 malformed('short mon vector',lambda d:d.__setitem__(first(d,'I 0 '),'I 0 19\n'))
 malformed('duplicate case',lambda d:d.__setitem__(first(d,'C 1 '),d[first(d,'C 1 ')].replace('C 1 ','C 0 ',1)))
 malformed('wrong input tag',lambda d:d.__setitem__(first(d,'ITAG '),'ITAG 0 1 73 104 -1 0 0\n'))
 malformed('unknown phase',lambda d:d.__setitem__(next(i for i,v in enumerate(d)if v.startswith('Q ')and ' MessagePhase 'in v),d[next(i for i,v in enumerate(d)if v.startswith('Q ')and ' MessagePhase 'in v)].replace('MessagePhase','MadeUpPhase')))
 malformed('invalid nullable tag',lambda d:d.__setitem__(first(d,'OTAG '),'OTAG 0 1 73 104 nan u u\n'))
 malformed('missing footer',lambda d:d.pop())
 bad.append(('trailing data',raw.decode()+'garbage\n'))
 for label,text in bad:
  altered=out/'malformed.txt';altered.write_text(text);run([BC/'build/tag-party-sources-test',altered],2,env);altered.unlink()
 forged=[]
 def forge(label,edit):
  data=lines.copy();edit(data);forged.append((label,''.join(data)))
 def alter_vector(data,prefix,column,value):
  i=first(data,prefix);v=data[i].split();v[column]=str(value);data[i]=' '.join(v)+'\n'
 forge('after HP',lambda d:alter_vector(d,'O 0 ',8,66))
 forge('return value',lambda d:alter_vector(d,'RET ',1,0))
 forge('global RNG',lambda d:alter_vector(d,'RO ',1,2))
 def anim_key(data):
  i=first(data,'ANIM ');v=data[i].split();v[1]='5';data[i]=' '.join(v)+'\n'
 forge('animation recipient',anim_key)
 for label,text in forged:
  altered=out/'forged.txt';altered.write_text(text);run([BC/'build/tag-party-sources-test',altered],1,env);altered.unlink();print('Rejected forged observation:',label,flush=True)
 print(f'Full-party tag sources proof: {len(faults)} compiled faults, {len(bad)} malformed captures, {len(forged)} forged observations rejected')
