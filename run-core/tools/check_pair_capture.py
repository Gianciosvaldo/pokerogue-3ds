#!/usr/bin/env python3
"""Verify complete native pair-factory observations and their C projection."""
import argparse,json,pathlib
p=argparse.ArgumentParser();p.add_argument('capture',type=pathlib.Path);p.add_argument('--write',action='store_true');a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[1];frozen=root/'tests/fixtures/pair-trainers.native.json';inc=root/'tests/pair-trainers.native.inc'
rows=json.loads(a.capture.read_text())
if not isinstance(rows,list) or len(rows)!=640:raise SystemExit('expected all 640 native pair records')
arr=lambda xs:'{'+','.join(map(str,xs))+'}'
out=['/* Actual pinned pair-factory constructor entry observations, including 320 retained normal-kind cases. */','typedef struct {int type,key,wave,level,kind,species,slot;pr_rng before,entry;} pair_case;','static const pair_case pair_native[]={']
for index,c in enumerate(rows):
 if c['index']!=index or c['type'] not in (58,36) or c['key'] not in (0,1) or c['kind'] not in (0,1):raise SystemExit('pair identity')
 o=c['observed']
 if o['level']!=c['level'] or o['slot']!=c['key']+1 or len(c['before'])!=4 or len(o['rng'])!=4:raise SystemExit('constructor shape')
 out.append(arr([c['type'],c['key'],c['wave'],c['level'],c['kind'],o['species'],o['slot'],arr(c['before']+[0]),arr(o['rng']+[0])])+',')
out.append('};');text='\n'.join(out)+'\n'
if a.write:frozen.write_bytes(a.capture.read_bytes());inc.write_text(text)
elif frozen.read_bytes()!=a.capture.read_bytes() or inc.read_text()!=text:raise SystemExit('native pair capture or C projection differs from frozen observations')
print('Pair factories: complete 640-record native capture and C projection match')
