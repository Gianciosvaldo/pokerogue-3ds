#!/usr/bin/env python3
"""Actual Pending healing maximum HP callbacks, strict capture transport and faults."""
from pathlib import Path
import argparse,gzip,json,os,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2];BC=ROOT/'battle-core';GC=ROOT/'game-core'
p=argparse.ArgumentParser();p.add_argument('--replay-only',action='store_true');args=p.parse_args()
def run(command,code=0,env=None):
 r=subprocess.run(list(map(str,command)),cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 if r.returncode!=code:raise RuntimeError(str(command)+'\n'+r.stdout[-5000:])
 return r.stdout
cc=os.environ.get('CC','cc');flags=['-O1','-std=c99','-ffp-contract=off','-I'+str(BC/'include'),'-I'+str(BC/'gen/out'),'-I'+str(BC/'build')]
runner=BC/'tests/pending-heal-test.c'
print(run(['make','-C',BC,'build/pending-heal-test']),flush=True)
with tempfile.TemporaryDirectory(prefix='pending-max-hp-')as directory:
 out=Path(directory);raw=gzip.decompress((BC/'tests/fixtures/pending-max-hp.native.txt.gz').read_bytes());capture=out/'native.txt';capture.write_bytes(raw)
 locales=json.loads(gzip.decompress((BC/'tests/fixtures/pending-max-hp.locales.json.gz').read_bytes()));assert set(locales)=={'battle','pokemon-info','battler-tags','arena-tag','ability-trigger','move-trigger','pokemon','ability','move','status-effect'}
 (out/'locales').mkdir()
 for name,content in locales.items():
  assert type(content)==str;json.loads(content);(out/'locales'/f'{name}.json').write_text(content)
 env={**os.environ,'TURNTEST_LOCALES':str(out/'locales')};print(run([BC/'build/pending-heal-test',capture],env=env),flush=True)
 if os.environ.get('PENDING_MAX_HP_SANITIZE')=='1':
  san=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'];objects=[]
  for file in ['arena_tags.c']:
   obj=out/(file+'.o');run([cc,*flags,*san,'-c',BC/'src'/file,'-o',obj]);objects.append(obj)
  exe=out/'san-test';run([cc,*flags,*san,runner,*objects,BC/'build/libprcore.a','-lm','-o',exe]);print(run([exe,capture],env=env),flush=True)
 if args.replay_only:raise SystemExit(0)
 faults=[
 ('arena_tags.c','bool can_apply = p->hp < pr_get_stat(p, STAT_HP, true) || p->status;','bool can_apply = p->hp < pr_get_stat(p, STAT_HP, false) || p->status;','eligibility reads temporary HP'),
 ('arena_tags.c','pr_unshift_heal_msg(bt, e, b, pr_get_stat(p, STAT_HP, true),','pr_unshift_heal_msg(bt, e, b, pr_get_stat(p, STAT_HP, false),','queued heal reads temporary HP'),
 ('arena_tags.c','if (simulated) return true;','if (false) return true;','simulation consumes effect'),
 ('arena_tags.c','bool valid_source = pr_pokemon_by_id(bt, h.source_id) != NULL;','bool valid_source = true;','accept invalid source'),
 ('arena_tags.c','!can_apply && h.restore_pp && k < PR_N_MOVES(p)','!can_apply && false && h.restore_pp && k < PR_N_MOVES(p)','ignore PP-only need'),
 ('arena_tags.c','(h.restore_pp ? PR_HEAL_RESTORE_PP : 0)','0','lose PP restore'),
 ('arena_tags.c','PR_HEAL_NO_FULL_HP_MESSAGE | PR_HEAL_SKIP_ANIM | PR_HEAL_STATUS','PR_HEAL_NO_FULL_HP_MESSAGE | PR_HEAL_SKIP_ANIM','lose status restore'),
 ]
 for n,(file,old,new,label)in enumerate(faults):
  folder=out/f'fault-{n}';folder.mkdir();source=(BC/'src'/file).read_text();assert source.count(old)==1,(label,source.count(old));src=folder/file;src.write_text(source.replace(old,new));obj=folder/'fault.o';exe=folder/'test'
  run([cc,*flags,'-c',src,'-o',obj]);run([cc,*flags,runner,obj,BC/'build/libprcore.a','-lm','-o',exe]);run([exe,capture],1,env);print('Caught compiled fault:',label,flush=True);shutil.rmtree(folder)
 # A comparator reading temporary HP must also fail, rather than masking the owner.
 source=runner.read_text();old='v[1]=pr_get_stat(p,STAT_HP,true)';assert source.count(old)==1;src=out/'snapshot-fault.c';src.write_text(source.replace(old,'v[1]=pr_get_stat(p,STAT_HP,false)'));exe=out/'snapshot-fault';run([cc,*flags,'-I'+str(BC/'tests'),src,BC/'build/libprcore.a','-lm','-o',exe]);run([exe,capture],1,env);exe.unlink();print('Caught compiled fault: snapshot reads temporary HP',flush=True)
 lines=raw.decode().splitlines(keepends=True);assert lines[0]=='PENDING 4 8192 777 4\n';bad=[]
 def malformed(label,edit):
  data=lines.copy();edit(data);bad.append((label,''.join(data)))
 def first(data,prefix):return next(i for i,v in enumerate(data)if v.startswith(prefix))
 malformed('wrong count',lambda d:d.__setitem__(0,'PENDING 4 8191 777 4\n'))
 malformed('missing wave',lambda d:d.pop(2))
 malformed('invalid mode',lambda d:d.__setitem__(1,'C 0 0 4 0 1\n'))
 malformed('invalid RNG',lambda d:d.__setitem__(first(d,'R '),'R 1 nan 0 0\n'))
 malformed('short mon snapshot',lambda d:d.__setitem__(first(d,'I '),'I 1 19\n'))
 malformed('wrong override',lambda d:d.__setitem__(first(d,'X '),'X 1\n'))
 malformed('fractional override',lambda d:d.__setitem__(first(d,'X '),'X 0.5\n'))
 malformed('invalid heal move',lambda d:d.__setitem__(first(d,'H '),'H 0 102 65535 0 00\n'))
 malformed('invalid phase',lambda d:d.__setitem__(first(d,'P '),'P 9 10 0 1 1 0 00\n'))
 malformed('missing footer',lambda d:d.pop())
 bad.append(('trailing data',raw.decode()+'garbage\n'))
 for label,text in bad:
  altered=out/'malformed.txt';altered.write_text(text);run([BC/'build/pending-heal-test',altered],2,env);altered.unlink()
 forged=[]
 def forge(label,edit):
  data=lines.copy();edit(data);forged.append((label,''.join(data)))
 def alter_vector(data,prefix,column,value):
  i=first(data,prefix);v=data[i].split();v[column]=str(value);data[i]=' '.join(v)+'\n'
 forge('after HP',lambda d:alter_vector(d,'O ',1,66))
 forge('heal amount',lambda d:alter_vector(d,'P ',2,77))
 forge('global RNG',lambda d:alter_vector(d,'RO ',1,2))
 forge('heal recipient',lambda d:alter_vector(d,'P ',1,1))
 for label,text in forged:
  altered=out/'forged.txt';altered.write_text(text);run([BC/'build/pending-heal-test',altered],1,env);altered.unlink();print('Rejected forged observation:',label,flush=True)
 print(f'Pending healing maximum HP proof: {len(faults)+1} compiled faults, {len(bad)} malformed captures, {len(forged)} forged observations rejected')
