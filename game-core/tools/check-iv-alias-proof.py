#!/usr/bin/env python3
"""Check native owner observations, compiled regression faults and capture guards."""
from pathlib import Path
import copy,gzip,json,os,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2];GC=ROOT/'game-core'
def run(args,good=True):
 p=subprocess.run(list(map(str,args)),cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 if (p.returncode==0)!=good:raise RuntimeError(str(args)+'\n'+p.stdout[-4000:])
 return p.stdout
cc=os.environ.get('CC','cc');flags=['-O1','-std=c99','-ffp-contract=off',*['-I'+str(ROOT/p)for p in ['game-core/include','game-core/src','run-core/include','ai-core/include','reward-core/include','reward-core/gen/out','battle-core/include','battle-core/gen/out']]]
libs=[GC/p for p in ['build/libgc.a','build/reward-core/libreward.a','build/ai-core/libai.a','build/run-core/librun.a','build/run-core/battle-core/libprcore.a']]
source=GC/'tests/iv_alias_host_test.c';fixture=GC/'tests/fixtures/iv-alias.native.json.gz';builder=GC/'tools/build-iv-alias-proof.py'
print(run(['make','-C',GC,'iv-alias-host-test']),flush=True)
with tempfile.TemporaryDirectory(prefix='iv-alias-proof-')as td:
 temp=Path(td);inc=temp/'native.inc';run(['python3',builder,fixture,inc]);assert inc.read_bytes()==(GC/'tests/fixtures/iv-alias.native.inc').read_bytes()
 faults=[
 ('gc_mon.c','    if(!share_ivs(run,m,copy))return;','    ;','omit constructor array sharing'),
 ('gc_training.c','            gc_mon_publish_ivs(run, m);','            ;','omit reward array write'),
 ('gc_mon.c','members[i]->iv_alias == source->iv_alias','members[i]->b.id == source->b.id','key IV arrays by Pokemon ID'),
 ('gc_mon.c','if (!source->iv_alias) return;','if (!source->iv_alias || !gc_int(run)->training.away) return;','skip returning retained member'),
 ('gc_mon.c','members[i]->iv_alias == source->iv_alias','members[i]->used_tm_alias == source->iv_alias','mix independent array namespaces'),
 ('gc_mon.c','if (!source->iv_alias) return;',';','publish unshared reloaded IVs'),
 ('gc_mon.c','memcpy(members[i]->b.ivs, source->b.ivs, sizeof source->b.ivs);','{memcpy(members[i]->b.ivs, source->b.ivs, sizeof source->b.ivs);gc_mon_recalc(run,members[i]);}','recalculate sibling stats during array write'),
 ('gc_save.c','SF(gc_mon, used_tms), SF(gc_mon, n_used_tms),','SF(gc_mon, used_tms), SF(gc_mon, n_used_tms), SF(gc_mon, iv_alias),','persist live IV array identity'),
 ]
 for n,(file,old,new,label)in enumerate(faults):
  folder=temp/f'fault-{n}';folder.mkdir();s=(GC/'src'/file).read_text();assert s.count(old)==1,(label,s.count(old));src=folder/file;src.write_text(s.replace(old,new));obj=folder/'fault.o';exe=folder/'test'
  run([cc,*flags,'-c',src,'-o',obj]);run([cc,*flags,source,obj,*libs,'-lm','-o',exe]);run([exe],False);print('Caught compiled fault:',label,flush=True);shutil.rmtree(folder)
 j=json.loads(gzip.decompress(fixture.read_bytes()));bad=[]
 def malformed(label,edit):
  d=copy.deepcopy(j);edit(d);bad.append((label,d))
 malformed('missing case',lambda d:d['rows'].pop())
 malformed('wrong reload basis',lambda d:d['rows'][0].__setitem__('reload',True))
 malformed('missing input species',lambda d:d['rows'][0]['before']['party'][0].pop('species'))
 malformed('negative HP',lambda d:d['rows'][0]['after']['party'][0].__setitem__('hp',-1))
 malformed('invalid RNG',lambda d:d['rows'][0]['away'].__setitem__('global','!rnd,-1,0,0,0'))
 malformed('short stats',lambda d:d['rows'][0]['after']['party'][0]['stats'].pop())
 malformed('boolean version',lambda d:d.__setitem__('version',True))
 malformed('out of range IV',lambda d:d['rows'][0]['after']['party'][0]['ivs'].__setitem__(0,32))
 malformed('missing profile entry',lambda d:d['rows'][0]['after']['profile'].pop())
 for label,d in bad:
  src=temp/'bad.json.gz';src.write_bytes(gzip.compress(json.dumps(d,separators=(',',':')).encode()));run(['python3',builder,src,temp/'bad.inc'],False)
 src=temp/'bad.json.gz';src.write_bytes(gzip.compress(gzip.decompress(fixture.read_bytes()).replace(b'"version":1',b'"version":1,"version":1',1)));run(['python3',builder,src,temp/'bad.inc'],False)
 for n,label in enumerate(['HP','global RNG','profile IV','sibling IV']):
  d=copy.deepcopy(j);f=d['rows'][0]['after']
  if n==0:f['party'][0]['hp']-=1
  elif n==1:
   v=f['global'].split(',');v[1]=str(int(v[1])+1);f['global']=','.join(v)
  elif n==2:f['profile'][3]['dex']['ivs'][0]+=1
  else:f['party'][1]['ivs'][0]+=1
  folder=temp/f'forged-{n}';folder.mkdir();src=folder/'native.json.gz';src.write_bytes(gzip.compress(json.dumps(d,separators=(',',':')).encode()));inc=folder/'native.inc';run(['python3',builder,src,inc]);test=folder/'test.c';test.write_text(source.read_text().replace('"../src/internal.h"',json.dumps(str(GC/'src/internal.h'))).replace('"fixtures/iv-alias.native.inc"',json.dumps(str(inc))));exe=folder/'test';run([cc,*flags,test,*libs,'-lm','-o',exe]);run([exe],False);print('Rejected forged observation:',label,flush=True);shutil.rmtree(folder)
 print(f'IV alias proof: {len(faults)} compiled faults, {len(bad)+1} malformed captures and 4 forged observations rejected')
