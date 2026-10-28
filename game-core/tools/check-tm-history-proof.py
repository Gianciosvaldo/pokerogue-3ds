#!/usr/bin/env python3
"""Complete native TM callbacks, projection/memory, compatible saves and faults."""
from pathlib import Path
import copy,gzip,hashlib,json,os,struct,subprocess,tempfile,zlib
ROOT=Path(__file__).resolve().parents[2];GC=ROOT/'game-core';RC=ROOT/'reward-core'
def execute(args):return subprocess.run(list(map(str,args)),cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
def command(args):
 r=execute(args)
 if r.returncode:raise RuntimeError(str(args)+'\n'+r.stdout[-5000:])
 return r.stdout
flags=['-O1','-std=c99','-ffp-contract=off',*['-I'+str(ROOT/p)for p in ['game-core/include','game-core/src','game-core/tests','run-core/include','ai-core/include','reward-core/include','reward-core/gen/out','battle-core/include','battle-core/gen/out']]]
libs=[GC/p for p in ['build/libgc.a','build/reward-core/libreward.a','build/ai-core/libai.a','build/run-core/librun.a','build/run-core/battle-core/libprcore.a']]
cc=os.environ.get('CC','cc');builder=['python3',GC/'tools/build-tm-history-proof.py'];test=GC/'tests/tm_history_host_test.c'
def hash_name(s):
 h=2166136261
 for c in s.encode():h=((h^c)*16777619)&0xffffffff
 return h
def change_save(raw,edit):
 assert raw[:8]==b'PR3DSRUN'and len(raw)==20+struct.unpack_from('<I',raw,12)[0]
 sections=[];pos=20
 while pos<len(raw):
  name,size=struct.unpack_from('<II',raw,pos);pos+=8;data=raw[pos:pos+size];pos+=size
  if name==hash_name('party'):
   n=struct.unpack_from('<H',data)[0];fields=[list(struct.unpack_from('<IH',data,2+6*k))for k in range(n)];at=2+6*n;count=struct.unpack_from('<H',data,at)[0];at+=2;rows=[]
   for _ in range(count):
    row=[]
    for _,length in fields:row.append(data[at:at+length]);at+=length
    rows.append(row)
   tail=data[at:];edit(fields,rows)
   data=struct.pack('<H',len(fields))+b''.join(struct.pack('<IH',*v)for v in fields)+struct.pack('<H',count)+b''.join(b''.join(row)for row in rows)+tail
  sections.append(struct.pack('<II',name,len(data))+data)
 body=b''.join(sections);header=bytearray(raw[:20]);struct.pack_into('<II',header,12,len(body),zlib.crc32(body));return bytes(header)+body
def resize(name,size):
 def edit(fields,rows):
  k=next(i for i,f in enumerate(fields)if f[0]==hash_name(name));fields[k][1]=size
  for row in rows:row[k]=row[k][:size].ljust(size,b'\0')
 return edit
def remove(name):
 def edit(fields,rows):
  k=next(i for i,f in enumerate(fields)if f[0]==hash_name(name));fields.pop(k)
  for row in rows:row.pop(k)
 return edit
def count_value(value):
 def edit(fields,rows):
  k=next(i for i,f in enumerate(fields)if f[0]==hash_name('n_used_tms'))
  for row in rows:row[k]=value.to_bytes(fields[k][1],'little')
 return edit
def duplicate(fields,rows):
 k=next(i for i,f in enumerate(fields)if f[0]==hash_name('n_used_tms'));fields.append(fields[k].copy())
 for row in rows:row.append(row[k])
with tempfile.TemporaryDirectory(prefix='tm-history-')as td:
 temp=Path(td);raw=gzip.decompress((GC/'tests/fixtures/tm-history.native.json.gz').read_bytes());capture=temp/'native.json';capture.write_bytes(raw);inc=temp/'native.inc';command([*builder,capture,inc]);assert inc.read_bytes()==(GC/'tests/fixtures/tm-history.native.inc').read_bytes()
 legacy=gzip.decompress((GC/'tests/fixtures/tm-history.legacy-64.gcs.gz').read_bytes());assert hashlib.sha256(legacy).hexdigest()=='13160c63bc64d6794f9f446da14e65a43e435b4e9aa852b948c70b9e14a17948'
 legacy_inc='/* Authentic HEAD5c07dea 64-TM command save; SHA256 13160c63bc64d6794f9f446da14e65a43e435b4e9aa852b948c70b9e14a17948. */\nstatic const unsigned char native_legacy_tm_save[]={'+','.join(map(str,legacy))+'};\n';assert legacy_inc==(GC/'tests/fixtures/tm-history.legacy-64.inc').read_text()
 print(command(['make','-C',GC,'tm-history-host-test']),flush=True)
 if os.environ.get('TM_HISTORY_SANITIZE')=='1':
  san=flags+['-g','-fsanitize=address,undefined','-fno-sanitize-recover=all'];objects=[]
  for source in [GC/'src/gc_mon.c',GC/'src/gc_phases.c',GC/'src/gc_reward.c',GC/'src/gc_save.c',RC/'src/memory.c']:
   obj=temp/(source.stem+'.o');command([cc,*san,'-c',source,'-o',obj]);objects.append(obj)
  exe=temp/'san';command([cc,*san,test,*objects,*libs,'-lm','-o',exe]);print(command([exe]),flush=True);print('Scoped ASan/UBSan/LSan: TM callback, reward projection, saves and memory query; supporting archives unsanitized');raise SystemExit(0)
 faults=[
  (GC/'src/gc_mon.c','if(!found){','if(!found&&m->n_used_tms<64){','restore 64-entry cutoff'),
  (GC/'src/gc_mon.c','if(!found){','if(true){','record duplicate TM'),
  (GC/'src/gc_mon.c','m->used_tms[m->n_used_tms++]=(uint16_t)move;','{m->used_tms[m->n_used_tms++]=(uint16_t)move;m->n_used_tms=(uint8_t)m->n_used_tms;}','narrow history count to byte'),
  (GC/'src/gc_phases.c','gc_rq_remove(run,RP_SELECT_MODIFIER);','pr_rng_seed_int(&run->run.rng,100,0);gc_rq_remove(run,RP_SELECT_MODIFIER);','consume learning RNG'),
  (GC/'src/gc_reward.c','r->n_used_tms = m->n_used_tms;','r->n_used_tms = (uint8_t)m->n_used_tms;','narrow projection count'),
  (GC/'src/gc_reward.c','r->n_used_tms = m->n_used_tms;','r->n_used_tms = m->n_used_tms < 64 ? m->n_used_tms : 64;','truncate reward projection'),
  (RC/'src/memory.c','i < m->n_used_tms && n < MEMORY_MAX','i < m->n_used_tms && i < 64 && n < MEMORY_MAX','truncate memory tail'),
  (GC/'src/gc_save.c','&& ss[k] == 64 * sizeof(uint16_t) && f[i].size','&& false && ss[k] == 64 * sizeof(uint16_t) && f[i].size','omit legacy array widening'),
  (GC/'src/gc_save.c','&& ss[k] == 1 && f[i].size == sizeof(uint16_t);','&& false && ss[k] == 1 && f[i].size == sizeof(uint16_t);','omit legacy count widening'),
  (GC/'src/gc_save.c','memcpy(dst + f[i].off, src + at[i], 64 * sizeof(uint16_t));','memcpy(dst + f[i].off, src + at[i], 32 * sizeof(uint16_t));','lose legacy history tail'),
 ]
 for source,old,new,label in faults:
  s=source.read_text();assert s.count(old)==1,(label,s.count(old));p=temp/source.name;p.write_text(s.replace(old,new));obj=temp/'fault.o';exe=temp/'fault';command([cc,*flags,'-c',p,'-o',obj]);command([cc,*flags,test,obj,*libs,'-lm','-o',exe]);r=execute([exe]);assert r.returncode!=0 and 'TM history step'in r.stdout,'surviving '+label;print('caught compiled fault: '+label,flush=True)
 current=temp/'current.gcs';print(command([GC/'build/tm_history_host_test','--dump-save',current]),flush=True);new=current.read_bytes()
 save_cases=[('new byte count',new,resize('n_used_tms',1)),('new legacy-size array',new,resize('used_tms',128)),('new partial array',new,remove('used_tms')),('new partial count',new,remove('n_used_tms')),('duplicate count',new,duplicate),('oversized new count',new,count_value(323)),('wrong array width',new,resize('used_tms',642)),('wrong count width',new,resize('n_used_tms',3)),('old widened count only',legacy,resize('n_used_tms',2)),('old widened array only',legacy,resize('used_tms',644)),('oversized old count',legacy,count_value(65))]
 for label,s,edit in save_cases:
  p=temp/'bad.gcs';p.write_bytes(change_save(s,edit));print(command([GC/'build/tm_history_host_test','--reject-save',p]),end='');print('rejected malformed save: '+label,flush=True)
 data=json.loads(raw)
 bad=[('missing callback',lambda d:d['rows'].pop()),('wrong first move',lambda d:d['rows'][0].__setitem__('move',1)),('boolean implementation',lambda d:d['implemented'].__setitem__(0,1)),('missing catalog id',lambda d:d['catalog'].pop()),('boolean catalog id',lambda d:d['catalog'].__setitem__(0,True)),('bad RNG',lambda d:d['rows'][-1]['after'].__setitem__('global','!rnd,1,1.1,0,0')),('bad PP',lambda d:d['rows'][-1]['after']['moves'][0].__setitem__('up',4)),('broken trace',lambda d:d['rows'][1]['before'].__setitem__('used',[])),('bad memory source',lambda d:d['rows'][-1]['after']['memory'][0].__setitem__(2,14)),('unknown root field',lambda d:d.__setitem__('extra',0))]
 for label,edit in bad:
  d=copy.deepcopy(data);edit(d);p=temp/'bad.json';p.write_text(json.dumps(d));r=execute([*builder,p,inc]);assert r.returncode!=0,'accepted malformed capture '+label;print('rejected malformed capture: '+label,flush=True)
 for key in ['history order','memory order','PP','RNG']:
  d=copy.deepcopy(data);f=d['rows'][-1]['after']
  if key=='history order':f['used'].reverse()
  elif key=='memory order':f['memory'].reverse()
  elif key=='PP':f['moves'][0]['used']=1
  else:
   v=f['global'].split(',');v[2]=str((float(v[2])+.125)%1);f['global']=','.join(v)
  p=temp/'forged.json';p.write_text(json.dumps(d));command([*builder,p,inc]);c=temp/'forged.c';c.write_text(test.read_text().replace('#include "../src/internal.h"','#include "internal.h"').replace('#include "fixtures/tm-history.native.inc"','#include "'+str(inc)+'"'));exe=temp/'forged';command([cc,*flags,c,*libs,'-lm','-o',exe]);r=execute([exe]);assert r.returncode!=0 and 'TM history step'in r.stdout,'surviving forged '+key;print('caught forged '+key,flush=True)
 print('TM history: 10 compiled faults, 11 malformed saves, 10 malformed captures, 4 forged observations caught')
