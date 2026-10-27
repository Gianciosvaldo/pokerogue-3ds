#!/usr/bin/env python3
"""Retained native capture transport, actual wild owner, compiled failure controls."""
from pathlib import Path
import copy,gzip,json,os,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2];GC=ROOT/'game-core'
def command(args):
 p=subprocess.run(list(map(str,args)),cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 if p.returncode:raise RuntimeError(str(args)+'\n'+p.stdout[-5000:])
 return p.stdout
def execute(args):return subprocess.run(list(map(str,args)),cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
flags=['-O1','-std=gnu11','-ffp-contract=off',*['-I'+str(ROOT/p)for p in ['game-core/include','game-core/src','run-core/include','ai-core/include','reward-core/include','reward-core/gen/out','battle-core/include','battle-core/gen/out']]]
libs=[GC/p for p in ['build/libgc.a','build/reward-core/libreward.a','build/ai-core/libai.a','build/run-core/librun.a','build/run-core/battle-core/libprcore.a']]
wrap=['-Wl,--wrap=pr_calculate_stats','-lm']
with tempfile.TemporaryDirectory(prefix='wild-movesets-proof-')as td:
 temp=Path(td);paths=[]
 for n in ['native','legacy-native','legacy-inputs']:
  p=temp/(n+'.json');p.write_bytes(gzip.decompress((GC/('tests/fixtures/wild-movesets.'+n+'.json.gz')).read_bytes()));paths.append(p)
 builder=['python3',GC/'tools/build-wild-movesets-proof.py'];inc=temp/'native.inc'
 command([*builder,*paths,inc]);assert inc.read_bytes()==(GC/'tests/fixtures/wild-movesets.native.inc').read_bytes()
 print(command(['make','-C',GC,'wild-movesets-host-test','build/enemy_moveset_test']),flush=True)
 print(command([GC/'build/enemy_moveset_test']),flush=True)
 cc=os.environ.get('CC','cc')
 if os.environ.get('WILD_MOVESETS_SANITIZE')=='1':
  san=flags+['-g','-fsanitize=address,undefined','-fno-sanitize-recover=all'];objects=[]
  for file in ['gc_moveset.c','gc_movegen.c','gc_encounter.c']:
   obj=temp/(file+'.o');command([cc,*san,'-c',GC/'src'/file,'-o',obj]);objects.append(obj)
  exe=temp/'san';command([cc,*san,GC/'tests/wild_movesets_host_test.c',*objects,*libs,*wrap,'-o',exe]);print(command([exe]),flush=True)
  print('Scoped ASan/UBSan/LSan: moveset adapter, generator and encounter owner; supporting archives unsanitized')
 else:
  faults=[
   ('gc_moveset.c','return gc_generate_moveset(run, &run->run.rng, m);','return false;','disconnect weighted owner'),
   ('gc_movegen.c','if (m->b.has_trainer || m->trainer_slot || run->run.has_challenges)','if (m->b.has_trainer || m->trainer_slot || m->b.fusion_species || run->run.has_challenges)','restore fusion rejection'),
   ('gc_movegen.c','    c->bt.weather = run->bt.weather;','    c->bt = run->bt; c->bt.weather = run->bt.weather;','use previous battle modifiers'),
   ('gc_movegen.c','    c->p = m->b;','    c->p = m->b; c->p.level = (uint16_t)c->p.level;','narrow wide level'),
   ('gc_movegen.c','gc_level_moves(m, 1, true, true, false, rows, 512)','gc_level_moves(m, 1, false, false, false, rows, 512)','omit evolution pool rows'),
   ('gc_movegen.c','gc_mg_signatures[i].species == c->p.species','gc_mg_signatures[i].species == (c->p.fusion_species ? c->p.fusion_species : c->p.species)','force secondary signature'),
   ('gc_movegen.c','if (c.bt.n_unported) { *rng = before; return false; }','if (false) { *rng = before; return false; }','ignore unported operation'),
   ('gc_movegen.c','if (c.bt.n_unported) { *rng = before; return false; }','if (c.bt.n_unported) { return false; }','consume RNG on rejected attempt'),
   ('gc_movegen.c','gc_mon_set_move(m, i, c.moves[i]);','gc_mon_set_move(m, i, c.moves[c.n_moves-1-i]);','reverse selected move order'),
   ('gc_movegen.c','m->b.pp_up[i] = (int8_t)pp_up[i];','m->b.pp_up[i] = (int8_t)pp_up[i];m->b.pp_used[i]=1;','lose fresh move PP'),
   ('gc_movegen.c','    memset(m->b.moves, 0, sizeof m->b.moves);','    pr_rng_seed_int(rng, 100, 0);\n    memset(m->b.moves, 0, sizeof m->b.moves);','add extra generator draw'),
  ]
  for file in sorted({f[0]for f in faults}):
   obj=temp/'normal.o';exe=temp/'normal';command([cc,*flags,'-c',GC/'src'/file,'-o',obj]);command([cc,*flags,GC/'tests/wild_movesets_host_test.c',obj,*libs,*wrap,'-o',exe]);r=execute([exe]);assert r.returncode==0,r.stdout
  for file,old,new,label in faults:
   s=(GC/'src'/file).read_text();assert s.count(old)==1,(label,s.count(old));p=temp/file;p.write_text(s.replace(old,new));obj=temp/'fault.o';exe=temp/'fault'
   command([cc,*flags,'-c',p,'-o',obj]);command([cc,*flags,GC/'tests/wild_movesets_host_test.c',obj,*libs,*wrap,'-o',exe]);r=execute([exe]);assert r.returncode!=0,('undetected compiled fault',label)
   print('Caught compiled fault:',label,flush=True)
  data=json.loads(paths[0].read_text());bad=[]
  def malformed(label,edit):
   d=copy.deepcopy(data);edit(d);bad.append((label,json.dumps(d)))
  malformed('missing case',lambda d:d['rows'].pop())
  malformed('duplicate case',lambda d:d['rows'].__setitem__(1,d['rows'][0]))
  malformed('boolean version',lambda d:d.__setitem__('version',True))
  malformed('wrong mode',lambda d:d['rows'][0].__setitem__('mode',True))
  malformed('missing RNG',lambda d:d['rows'][0]['generation'].pop('after'))
  malformed('nonfinite stat',lambda d:d['rows'][0]['generation']['input']['stats'].__setitem__(0,float('nan')))
  malformed('boolean IV',lambda d:d['rows'][0]['generation']['input']['ivs'].__setitem__(0,True))
  malformed('missing PP metadata',lambda d:d['rows'][0]['generation']['moves'][0].pop('max'))
  malformed('truncated pool row',lambda d:d['rows'][0]['generation']['pool'][0].pop())
  bad.append(('duplicate key',paths[0].read_text().replace('"version":1','"version":1,"version":1',1)))
  for label,payload in bad:
   p=temp/'bad.json';p.write_text(payload);r=execute([*builder,p,*paths[1:],temp/'bad.inc']);assert r.returncode!=0,('accepted malformed capture',label)
   print('Rejected malformed capture:',label,flush=True)
  for source,label,edit in [
   (1,'missing retained native case',lambda d:d['rows'].pop()),
   (1,'changed retained input',lambda d:d['rows'][0].__setitem__('seed',0)),
   (2,'changed original adapter seed',lambda d:d[0].__setitem__('seed',1)),
  ]:
   d=json.loads(paths[source].read_text());edit(d);p=temp/'bad-retained.json';p.write_text(json.dumps(d));inputs=paths.copy();inputs[source]=p
   r=execute([*builder,*inputs,temp/'bad-retained.inc']);assert r.returncode!=0,('accepted malformed retained corpus',label)
   print('Rejected malformed retained corpus:',label,flush=True)
  for key in ['move','pool','RNG','ability']:
   d=copy.deepcopy(data);g=d['rows'][-1]['generation']
   if key=='move':g['moves'][0]['move']=1 if g['moves'][0]['move']!=1 else 2
   elif key=='pool':g['pool'][0][1]=1 if g['pool'][0][1]!=1 else 2
   elif key=='ability':g['input']['ability']+=1
   else:
    v=g['after'].split(',');v[2]=str((float(v[2])+.125)%1);g['after']=d['rows'][-1]['after']=','.join(v)
   p=temp/'forged.json';p.write_text(json.dumps(d));forged=temp/'forged.inc';command([*builder,p,*paths[1:],forged]);test=temp/'forged.c';test.write_text((GC/'tests/wild_movesets_host_test.c').read_text().replace('#include "fixtures/wild-movesets.native.inc"','#include "'+str(forged)+'"'));exe=temp/'forged'
   command([cc,*flags,test,*libs,*wrap,'-o',exe]);r=execute([exe]);assert r.returncode!=0,('accepted forged output',key)
   print('Rejected forged output:',key,flush=True)
  print('Wild movesets: 11 compiled faults, 13 malformed captures/corpora, 4 forged outputs rejected')
