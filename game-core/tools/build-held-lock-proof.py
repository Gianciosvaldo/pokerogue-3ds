#!/usr/bin/env python3
"""Transport native factory/serialization/filter observations without an effect oracle."""
import json,sys
r=json.load(open(sys.argv[1]));assert r['version']==1 and r['forms']==199 and len(r['rows'])==2632
keys=('FORM_CHANGE_ITEM','RARE_FORM_CHANGE_ITEM','BASE_STAT_BOOSTER','EVOLUTION_TRACKER_GIMMIGHOUL','MYSTERY_ENCOUNTER_SHUCKLE_JUICE','MYSTERY_ENCOUNTER_OLD_GATEAU','MYSTERY_ENCOUNTER_MACHO_BRACE')
seen=set();out=['HELDLOCK 1 2632']
for i,x in enumerate(r['rows']):
 assert x['key'] in keys and 0<=x['owner']<4 and x['active'] in (0,1)
 tag=(x['key'],x['arg'],x['owner'],x['active']);assert tag not in seen;seen.add(tag)
 out.append('C '+' '.join(map(str,[i,x['key'],x['arg'],x['owner'],x['active'],int(x['locked']),int(x['restoredLocked']),int(x['result']),x['visible']])))
out.append('END 2632');open(sys.argv[2],'w').write('\n'.join(out)+'\n')
