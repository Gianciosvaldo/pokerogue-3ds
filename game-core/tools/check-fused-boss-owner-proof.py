#!/usr/bin/env python3
"""Actual fused-boss moveset owner, retained source transport and fault controls."""
from pathlib import Path
import copy,gzip,json,os,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2];GC=ROOT/'game-core'
def execute(args):return subprocess.run(list(map(str,args)),cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
def command(args):
 r=execute(args)
 if r.returncode:raise RuntimeError(str(args)+'\n'+r.stdout[-5000:])
 return r.stdout
flags=['-O1','-std=gnu11','-ffp-contract=off',*['-I'+str(ROOT/p)for p in ['game-core/include','game-core/src','game-core/tests','run-core/include','ai-core/include','reward-core/include','reward-core/gen/out','battle-core/include','battle-core/gen/out']]]
libs=[GC/p for p in ['build/libgc.a','build/reward-core/libreward.a','build/ai-core/libai.a','build/run-core/librun.a','build/run-core/battle-core/libprcore.a']]
wrap=['-Wl,--wrap=pr_calculate_stats','-lm'];cc=os.environ.get('CC','cc');builder=['python3',GC/'tools/build-fused-boss-owner-proof.py'];test=GC/'tests/fused_boss_owner_host_test.c'
with tempfile.TemporaryDirectory(prefix='fused-boss-owner-')as td:
 temp=Path(td);paths=[]
 for n in ['focused','constructors']:
  p=temp/(n+'.json');p.write_bytes(gzip.decompress((GC/('tests/fixtures/fusion-power.'+n+'.json.gz')).read_bytes()));paths.append(p)
 inc=temp/'native.inc';command([*builder,*paths,inc]);assert inc.read_bytes()==(GC/'tests/fixtures/fused-boss-owner.native.inc').read_bytes()
 print(command(['make','-C',GC,'fused-boss-owner-host-test','fusion-power-host-test']),flush=True)
 if os.environ.get('FUSED_BOSS_OWNER_SANITIZE')=='1':
  san=flags+['-g','-fsanitize=address,undefined','-fno-sanitize-recover=all'];objects=[]
  for file in ['gc_moveset.c','gc_movegen.c','gc_encounter.c']:
   obj=temp/(file+'.o');command([cc,*san,'-c',GC/'src'/file,'-o',obj]);objects.append(obj)
  exe=temp/'san';command([cc,*san,test,*objects,*libs,*wrap,'-o',exe]);print(command([exe]),flush=True)
  print('Scoped ASan/UBSan/LSan: moveset adapter, generator and encounter owner; supporting archives unsanitized');raise SystemExit(0)
 faults=[
  ('gc_moveset.c','|| m->b.passive_flag) return false;','|| (m->b.is_boss && m->b.fusion_species) || m->b.passive_flag) return false;','restore public fused boss rejection'),
  ('gc_movegen.c','if (m->b.fusion_species && (m->b.passive_flag || (!c.boss && c.passive)','if (m->b.fusion_species && (c.boss || m->b.passive_flag || (!c.boss && c.passive)','restore private fused boss rejection'),
  ('gc_movegen.c','c->boss = m->b.is_boss;','c->boss = false;','lose boss filters and weights'),
  ('gc_movegen.c','c->passive = pr_has_passive(&c->bt, &c->p) ? c->p.passive : 0;','c->passive = 0;','lose natural species passive'),
  ('gc_movegen.c','c->single = !run->double_battle;','c->single = true;','use singles denylist in doubles'),
  ('gc_movegen.c','    c->bt.weather = run->bt.weather;','    c->bt = run->bt; c->bt.weather = run->bt.weather;','use previous battle modifiers'),
  ('gc_movegen.c','gc_level_moves(m, 1, true, true, false, rows, 512)','gc_level_moves(m, 1, false, false, false, rows, 512)','omit evolution pool rows'),
  ('gc_movegen.c','if (c.boss) filter_superceded(&pool);','if (false) filter_superceded(&pool);','omit boss superseded filter'),
  ('gc_movegen.c','if (c.boss) weight_multiplier += gc_mg_const.boss_extra_weight_multiplier;','if (false) weight_multiplier += gc_mg_const.boss_extra_weight_multiplier;','omit boss weight exponent'),
  ('gc_movegen.c','if (c.bt.n_unported) { *rng = before; return false; }','if (false) { *rng = before; return false; }','publish unported result'),
  ('gc_movegen.c','if (c.bt.n_unported) { *rng = before; return false; }','if (c.bt.n_unported) { return false; }','consume RNG on rejection'),
  ('gc_movegen.c','gc_mon_set_move(m, i, c.moves[i]);','gc_mon_set_move(m, i, c.moves[c.n_moves-1-i]);','reverse selected move order'),
  ('gc_movegen.c','m->b.pp_up[i] = (int8_t)pp_up[i];','m->b.pp_up[i] = (int8_t)pp_up[i];m->b.pp_used[i]=1;','lose fresh move PP'),
 ]
 for file,old,new,label in faults:
  source=(GC/'src'/file).read_text();assert source.count(old)==1,(label,source.count(old));p=temp/file;p.write_text(source.replace(old,new));obj=temp/'fault.o';exe=temp/'fault'
  command([cc,*flags,'-c',p,'-o',obj]);command([cc,*flags,test,obj,*libs,*wrap,'-o',exe]);r=execute([exe]);assert r.returncode!=0 and 'fused boss owner case'in r.stdout,'surviving '+label;print('caught compiled fault: '+label,flush=True)
 data=json.loads(paths[0].read_text())
 for label,edit in [('missing case',lambda d:d['rows'].pop()),('invalid RNG',lambda d:d['rows'][0]['generation'].__setitem__('after','!rnd,1,1.1,0,0')),('invalid move',lambda d:d['rows'][0]['generation']['moves'][0].__setitem__('move',0)),('invalid fresh PP',lambda d:d['rows'][0]['generation']['moves'][0].__setitem__('used',1)),('invalid max override',lambda d:d['rows'][0]['generation']['moves'][0].__setitem__('max',0))]:
  d=copy.deepcopy(data);edit(d);p=temp/'bad.json';p.write_text(json.dumps(d));r=execute([*builder,p,paths[1],inc]);assert r.returncode!=0,'accepted malformed '+label;print('rejected malformed '+label,flush=True)
 for key in ['move','order','PP','RNG']:
  d=copy.deepcopy(data);g=d['rows'][0]['generation']
  if key=='move':g['moves'][0]['move']=1 if g['moves'][0]['move']!=1 else 2
  elif key=='order':g['moves'].reverse()
  elif key=='PP':g['moves'][0]['up']=1
  else:
   v=g['after'].split(',');v[2]=str((float(v[2])+.125)%1);g['after']=','.join(v)
  p=temp/'forged.json';p.write_text(json.dumps(d));command([*builder,p,paths[1],inc]);c=temp/'forged.c';c.write_text(test.read_text().replace('#include "../src/internal.h"','#include "internal.h"').replace('#include "fixtures/fused-boss-owner.native.inc"','#include "'+str(inc)+'"'));exe=temp/'forged';command([cc,*flags,c,*libs,*wrap,'-o',exe]);r=execute([exe]);assert r.returncode!=0 and 'fused boss owner case'in r.stdout,'surviving forged '+key;print('caught forged '+key,flush=True)
 print('fused boss owner: 13 compiled faults, 5 malformed captures, 4 forged observations caught')
