#!/usr/bin/env python3
"""Copy actual native party-action/state/profile observations, never calculate results."""
import collections
import json
import sys

PIN = '8555c08c823b856cbec4eb99ca84ea52a955836d'
facts = json.load(open(sys.argv[1]))
assert facts['pin'] == PIN and len(facts['rules']) == 416
rules = facts['rules']
del facts
datasets = [(sys.argv[2], 63232), (sys.argv[3], 9984), (sys.argv[4], 1664)]

def state(s):
    return [s[k] for k in ('species','form','level','nature','gender','abilityIndex','baseAbility','ability','hp')] + s['stats'] + s['ivs'] + [s[k] for k in ('tera','teraType','auto','onField')] + [s.get('fusionSpecies',0),s.get('fusionForm',0),s.get('fusionAbilityIndex',-1)]

def dex(d):
    assert set(d) == {'seenAttr','caughtAttr','natureAttr','seenCount','caughtCount','hatchedCount','ivs','ribbons'}
    assert set(d['ribbons']) == {'payload'} and len(d['ivs']) == 6
    return [int(d['seenAttr']),int(d['caughtAttr']),d['natureAttr'],d['seenCount'],d['caughtCount'],d['hatchedCount'],*d['ivs'],int(d['ribbons']['payload'])]

def starter(s):
    assert set(s) == {'moveset','eggMoves','candyCount','friendship','abilityAttr','passiveAttr','valueReduction','classicWinCount'}
    assert s['moveset'] is None  # actual source default, represented by four empty C slots
    return [s[k] for k in ('abilityAttr','passiveAttr','valueReduction','candyCount','friendship','eggMoves','classicWinCount')] + [0]*4

with open(sys.argv[5], 'w') as output:
    def row(tag, values):
        assert all(isinstance(v,(int,bool)) for v in values)
        output.write(tag+' '+' '.join(str(int(v)) for v in values)+'\n')
    output.write('FORMPHASE 1 416 74880\n')
    for i,r in enumerate(rules):
        # Input construction facts are independent of the production C tables.
        companion = -1
        if r['species'] == 800 and r['item'] == 114:
            companion = 159 if 'dawn-wings' in (r['preKey'],r['toKey']) else 158
        row('R',[i,r['species'],r['from'],r['to'],r['item'],r['active'],companion])
    cid = 0
    for path,count in datasets:
        r = json.load(open(path))
        assert r['version'] == 1 and r['pin'] == PIN and len(r['records']) == count
        assert len(r['ids']) == len(set(r['ids'])) == 3 and all(0 < n <= 0xffffffff for n in r['ids'])
        row('G',[count,*r['ids']])
        seen,groups = set(),collections.Counter()
        for v in r['records']:
            b=v['before']; key=(v['group'],v['rule'],v['owner'],b['level'],b['nature'],v.get('inputAbilityIndex',0),v.get('inputTera',False), b.get('fusionSpecies',0))
            assert key not in seen;seen.add(key);groups[v['group']]+=1
            assert v['owner'] in (0,2) and b['species'] == rules[v['rule']]['species'] and b['form'] == rules[v['rule']]['from']
            row('C',[cid,v['group'],v['rule'],v['owner'],v['selected'],v['active']]);cid+=1
            row('B',state(b));row('A',state(v['after']))
            for tag,old,new,convert in [('D',v['dexBefore'],v['dexAfter'],dex),('S',v['starterBefore'],v['starterAfter'],starter)]:
                assert old.keys() == new.keys()
                row(tag,[len(old)])
                for sp in sorted(old,key=int):
                    row('L',[int(sp),*convert(old[sp])]);row('V',[int(sp),*convert(new[sp])])
            output.write('T '+(v['message'].encode().hex() or '-')+'\n')
        assert groups == ({0:832,1:62400} if count==63232 else {2:9984} if count==9984 else {3:1664})
        del r
    assert cid == 74880
    output.write('END 416 74880\n')
