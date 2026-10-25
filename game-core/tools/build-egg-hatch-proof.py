#!/usr/bin/env python3
"""Encode native EggLapsePhase generation observations, without modeling results."""
from pathlib import Path
import json,sys
x=json.loads(Path(sys.argv[1]).read_text());assert x['version']==2 and len(x['rows'])==2284
rows=['EGGHATCH 2 2284']
for i,r in enumerate(x['rows']):
 assert r['i']==i
 e,p=r['egg'],r['pokemon']
 v=[i,e['id'],e['species'],e['sourceType'],int(e['isShiny']),e['variantTier'],e['eggMoveIndex'],int(e['overrideHiddenAbility']),p['id'],p['species'],p['form'],p['gender'],p['ability'],p['nature'],int(p['shiny']),p['variant'],p['tera'],*p['ivs']]
 assert len(v)==23;rows.append('C '+' '.join(map(str,v)))
rows.append('EGGHATCH_COMPLETE 2284');Path(sys.argv[2]).write_text('\n'.join(rows)+'\n')
