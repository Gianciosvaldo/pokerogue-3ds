#!/usr/bin/env python3
"""Pinned Tera shard consumable replay and behavior-changing fault controls."""
from pathlib import Path
import os,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2];GC=ROOT/'game-core'
def execute(args):return subprocess.run(list(map(str,args)),cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
def run(args):
 r=execute(args)
 if r.returncode:raise RuntimeError(str(args)+'\n'+r.stdout[-4000:])
 return r.stdout
flags=['-O1','-std=c99',*['-I'+str(ROOT/p)for p in ['game-core/include','game-core/src','run-core/include','ai-core/include','reward-core/include','reward-core/gen/out','battle-core/include','battle-core/gen/out']]]
libs=[GC/p for p in ['build/libgc.a','build/reward-core/libreward.a','build/ai-core/libai.a','build/run-core/librun.a','build/run-core/battle-core/libprcore.a']]
cc=os.environ.get('CC','cc');test=GC/'tests/tera_shard_host_test.c';capture=GC/'build/tera-shard.native.txt'
print(run(['make','-C',GC,'tera-shard-host-test']),flush=True)
source=(GC/'src/gc_reward.c').read_text()
with tempfile.TemporaryDirectory(prefix='tera-shards-')as td:
 temp=Path(td)
 if os.environ.get('TERA_SHARDS_SANITIZE')=='1':
  san=['-g','-fsanitize=address,undefined','-fno-sanitize-recover=all'];obj=temp/'reward.o';exe=temp/'san'
  run([cc,*flags,*san,'-c',GC/'src/gc_reward.c','-o',obj]);run([cc,*flags,*san,test,obj,*libs,'-lm','-o',exe]);print(run([exe,capture]),flush=True);raise SystemExit(0)
 faults=[
 ('if (item == PR_ITEM_TERA_SHARD) {','if (false && item == PR_ITEM_TERA_SHARD) {','restore inert inventory fallback'),
 ('for (int i=0;i<2;i++)','for (int i=0;i<1;i++)','ignore fusion exclusions'),
 ('if (species[i]==SPECIES_ID_TERAPAGOS || species[i]==SPECIES_ID_OGERPON || species[i]==SPECIES_ID_SHEDINJA) return false;','if (false) return false;','admit excluded primary species'),
 ('run->party[party].b.tera_type=(int8_t)o->arg;','run->party[0].b.tera_type=(int8_t)o->arg;','change display ordinal instead of selected storage member'),
 ('run->party[party].b.tera_type=(int8_t)o->arg;','run->party[party].b.tera_type=POKEMON_TYPE_NORMAL;','ignore shard type'),
 ('if (!mon) return false;','if (!mon || mon->b.hp <= 0) return false;','reject fainted recipients'),
 ]
 for old,new,label in faults:
  assert source.count(old)==1,(label,source.count(old));p=temp/'fault.c';p.write_text(source.replace(old,new));obj=temp/'fault.o';exe=temp/'fault'
  run([cc,*flags,'-c',p,'-o',obj]);run([cc,*flags,test,obj,*libs,'-lm','-o',exe]);r=execute([exe,capture]);assert r.returncode!=0 and 'Tera shard case'in r.stdout,('surviving '+label,r.returncode,r.stdout[-1500:]);print('Caught compiled fault:',label,flush=True);exe.unlink()
 print('Tera shards: all six compiled faults rejected')
