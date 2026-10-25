#!/usr/bin/env python3
"""Retain pinned charge/carry frames and the explicit requested cleanup outcome."""
from pathlib import Path
import json,itertools,re,sys
j=json.loads(Path(sys.argv[1]).read_text());assert j['version']==1 and len(j['rows'])==28
ids=j['chargingIds'];assert ids==sorted(set(ids)) and len(ids)==17
moves=[91,19,566,291,340,467,507]
assert [(r['move'],r['slot'],r['target'])for r in j['rows']]==list(itertools.product(moves,[0,1],[2,3]))
lines=['/* Generated from actual pinned frames; cleanup is the requested policy override. */',f'static const int native_charging_ids[{len(ids)}]={{'+','.join(map(str,ids))+'};']
def tm(m):return '{'+','.join(map(str,[m['move'],m['mode'],m.get('result',255),len(m['targets']),'{'+','.join(map(str,m['targets']))+'}']))+'}'
def frame(f):
 assert len(f['stages'])==7 and len(f['history'])==2
 tags=[]
 for t in f['tags']:
  assert re.fullmatch('[A-Z_]+',t['type']);tags.append('{'+f"BATTLER_TAG_TYPE_{t['type']},{t['move']},{int(t['ownSource'])},{t['turns']}"+'}')
 return '{'+','.join([str(len(f['queue'])),'{'+','.join(tm(m)for m in f['queue'])+'}',str(len(tags)),'{'+','.join(tags)+'}',str(int(f['visible'])),str(f['pp']),'{'+','.join(map(str,f['stages']))+'}',str(len(f['history'])),'{'+','.join(tm(m)for m in f['history'])+'}'])+'}'
lines.append('static const native_charge_case native_charge_cases[28]={')
for r in j['rows']:
 assert r['charged']==r['ended']==r['nextWave']
 assert not r['cleaned']['queue'] and not r['cleaned']['tags'] and r['cleaned']['visible']
 assert all(r['cleaned'][k]==r['charged'][k]for k in ['stages','pp','history'])
 lines.append('{'+f"{r['move']},{r['slot']},{r['target']},"+frame(r['ended'])+','+frame(r['cleaned'])+'},')
lines.append('};');Path(sys.argv[2]).write_text('\n'.join(lines)+'\n')
