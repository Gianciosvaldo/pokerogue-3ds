#!/usr/bin/env python3
"""Joined constructor replay, strict capture controls and compiled fault injection."""
from pathlib import Path
import copy,gzip,json,os,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2];GC=ROOT/'game-core'
pin='8555c08c823b856cbec4eb99ca84ea52a955836d'
def execute(args):return subprocess.run(list(map(str,args)),cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
def run(args):
 r=execute(args)
 if r.returncode:raise RuntimeError(str(args)+'\n'+r.stdout[-5000:])
 return r.stdout
flags=['-O1','-std=c99','-ffp-contract=off',*['-I'+str(ROOT/p)for p in ['game-core/include','game-core/src','game-core/tests','game-core/build','run-core/include','ai-core/include','reward-core/include','reward-core/gen/out','battle-core/include','battle-core/gen/out']]]
libs=[GC/p for p in ['build/libgc.a','build/reward-core/libreward.a','build/ai-core/libai.a','build/run-core/librun.a','build/run-core/battle-core/libprcore.a']]
cc=os.environ.get('CC','cc');builder=[ 'python3',GC/'tools/build-constructor-finish-proof.py'];test=GC/'tests/constructor_finish_host_test.c'
print(run(['make','-C',GC,'constructor-finish-host-test']),flush=True)
with tempfile.TemporaryDirectory(prefix='constructor-finish-')as td:
 temp=Path(td);capture=temp/'native.json';raw=gzip.decompress((GC/'tests/fixtures/constructor-finish.native.json.gz').read_bytes());capture.write_bytes(raw);inc=temp/'native.inc'
 run([*builder,capture,inc]);assert inc.read_bytes()==(GC/'build/constructor-finish.native.inc').read_bytes()
 if os.environ.get('CONSTRUCTOR_FINISH_SANITIZE')=='1':
  san=['-g','-fsanitize=address,undefined','-fno-sanitize-recover=all'];objects=[]
  for file in ['run-core/src/pokemongen.c','game-core/src/gc_moveset.c','game-core/src/gc_movegen.c','game-core/src/gc_mon.c','game-core/src/gc_encounter.c']:
   obj=temp/(Path(file).name+'.o');run([cc,*flags,*san,'-c',ROOT/file,'-o',obj]);objects.append(obj)
  exe=temp/'san';run([cc,*flags,*san,test,*objects,*libs,'-Wl,--wrap=pr_rng_seed_int','-lm','-o',exe]);print(run([exe]),flush=True);print('Scoped ASan/UBSan/LSan: base/finish, move adapter/generator, mon projection and encounter owner; supporting archives unsanitized');raise SystemExit(0)
 faults=[
 ('run-core/src/pokemongen.c','const uint32_t id2 = (uint32_t)floor(pr_rng_real_in_range(finish_rng, 0, 4294967296.0));','const uint32_t id2 = 0;','omit post-moveset boss IV roll'),
 ('run-core/src/pokemongen.c','out->ivs[i] = (int)js_round((hi - lo) * 0.75 + lo);','out->ivs[i] = (int)js_round((hi - lo) * 0.5 + lo);','wrong boss IV interpolation'),
 ('run-core/src/pokemongen.c','out->rival_fight = out->rival_slot = 0;\n\n    double chance = BASE_HIDDEN_ABILITY_RATE;\n    if (ctx->hidden_ability_charm_stacks > 0) chance *= js_pow(2, -1 - ctx->hidden_ability_charm_stacks);','out->rival_fight = out->rival_slot = 0;\n\n    double chance = BASE_HIDDEN_ABILITY_RATE;\n    if (ctx->hidden_ability_charm_stacks > 0) chance *= 1;','ignore Ability Charm in base rolls'),
 ('game-core/src/gc_moveset.c','|| m->b.passive_flag) return false;','|| (m->b.is_boss && m->b.fusion_species) || m->b.passive_flag) return false;','restore fused boss fallback'),
 ('game-core/src/gc_movegen.c','gc_mon_set_move(m, i, c.moves[i]);','gc_mon_set_move(m, i, c.moves[c.n_moves-1-i]);','reverse generated move order'),
 ('game-core/src/gc_mon.c','for (int i = 0; i < 6; i++) p->ivs[i] = (uint8_t)g->ivs[i];','for (int i = 0; i < 6; i++) p->ivs[i] = (uint8_t)g->ivs[5-i];','reverse generated IV projection'),
 ('game-core/src/gc_encounter.c','m->b.pp_up[k] = pp_up[k];','m->b.pp_up[k] = 1;','corrupt PP metadata during refresh'),
 ('game-core/src/gc_encounter.c','m->b.max_pp_override[k] = max_pp[k];','m->b.max_pp_override[k] = 5;','corrupt max PP override during refresh'),
 ('game-core/src/gc_encounter.c','if (e->kind == PR_POOL_BERRY) {','if (false && e->kind == PR_POOL_BERRY) {','omit threshold berry draws'),
 ('game-core/src/gc_encounter.c','pr_update_passive(&run->bt, &run->enemy[i].b);\n        pr_calculate_stats(&run->bt, &run->enemy[i].b);','pr_update_passive(&run->bt, &run->enemy[i].b);','lose deferred stat calculation'),
 ]
 for i,(file,old,new,label)in enumerate(faults):
  folder=temp/f'fault-{i}';folder.mkdir();source=(ROOT/file).read_text();assert source.count(old)==1,(label,source.count(old));p=folder/Path(file).name;p.write_text(source.replace(old,new));obj=folder/'fault.o';exe=folder/'fault';run([cc,*flags,'-c',p,'-o',obj]);run([cc,*flags,test,obj,*libs,'-Wl,--wrap=pr_rng_seed_int','-lm','-o',exe]);r=execute([exe]);assert r.returncode!=0 and 'constructor finish case'in r.stdout,'surviving '+label;print('Caught compiled fault:',label,flush=True);shutil.rmtree(folder)
 data=json.loads(raw)
 malformed=[
 ('missing case',lambda d:d['rows'].pop()),
 ('duplicate case',lambda d:d['rows'].__setitem__(-1,copy.deepcopy(d['rows'][-2]))),
 ('wrong mode',lambda d:d['rows'][-1].__setitem__('mode',0)),
 ('boolean level',lambda d:d['rows'][-1].__setitem__('level',True)),
 ('invalid finish RNG',lambda d:d['rows'][-1].__setitem__('after','!rnd,1,nan,0,0')),
 ('wrong frame identity',lambda d:d['rows'][-1]['final'].__setitem__('species',19)),
 ('short stats',lambda d:d['rows'][-1]['refreshed']['stats'].pop()),
 ('undefined HP',lambda d:d['rows'][-1]['refreshed'].__setitem__('hp',None)),
 ('invalid IV',lambda d:d['rows'][-1]['final']['ivs'].__setitem__(0,32)),
 ('bad PP metadata',lambda d:d['rows'][-1]['generation']['moves'][0].__setitem__('used',1)),
 ('changed original case',lambda d:d['rows'][0]['generation']['moves'].reverse()),
 ('unexpected field',lambda d:d.__setitem__('extra',1)),
 ]
 for label,edit in malformed:
  d=copy.deepcopy(data);edit(d);capture.write_text(json.dumps(d));r=execute([*builder,capture,inc]);assert r.returncode!=0,'accepted malformed '+label;print('Rejected malformed:',label,flush=True)
 forged=[
 ('final IV',lambda d:d['rows'][-1]['final']['ivs'].__setitem__(0,(d['rows'][-1]['final']['ivs'][0]+1)%32)),
 ('deferred stat',lambda d:d['rows'][-1]['refreshed']['stats'].__setitem__(1,d['rows'][-1]['refreshed']['stats'][1]+1)),
 ('finish RNG',lambda d:d['rows'][-1].__setitem__('after','!rnd,1,0.1,0.2,0.3')),
 ('threshold RNG',lambda d:d['rows'][-1].__setitem__('poolAfter','!rnd,1,0.1,0.2,0.3')),
 ('selected move',lambda d:d['rows'][-1]['generation']['moves'][0].__setitem__('move',150)),
 ]
 harness=test.read_text().replace('#include "../src/internal.h"','#include "internal.h"').replace('#include "constructor-finish.native.inc"','#include "'+str(inc)+'"')
 for label,edit in forged:
  d=copy.deepcopy(data);edit(d);capture.write_text(json.dumps(d));run([*builder,capture,inc]);p=temp/'forged.c';p.write_text(harness);exe=temp/'forged';run([cc,*flags,p,*libs,'-Wl,--wrap=pr_rng_seed_int','-lm','-o',exe]);r=execute([exe]);assert r.returncode!=0 and 'constructor finish case'in r.stdout,'surviving forged '+label;print('Rejected forged:',label,flush=True);exe.unlink()
 print(f'Constructor finish proof: {len(faults)} compiled faults, {len(malformed)} malformed captures, {len(forged)} forged observations rejected')
