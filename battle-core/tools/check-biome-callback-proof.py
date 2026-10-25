#!/usr/bin/env python3
"""Compiled source-contract faults for biome attributes and the owning prefix."""
from pathlib import Path
import os,sys,subprocess,tempfile
r=Path(__file__).resolve().parents[2];raw=Path(sys.argv[1]).resolve();prefix=Path(sys.argv[2]).resolve();env=os.environ.copy();assert env.get('TURNTEST_LOCALES');runner=r/'battle-core/build/biome-callback-test'
for trace in [raw,prefix]:subprocess.run([runner,trace],check=True,env=env)
source=(r/'battle-core/src/ab_attrs.c').read_text();a=source.index('    case PR_AC_PostBiomeChangeWeatherChangeAbAttr: {');b=source.index('    case PR_AC_PostSummonWeatherChangeAbAttr:',a);body=source[a:b];faults=[]
def fault(name,old,new):
 assert body.count(old)==1,(name,body.count(old));faults.append((name,'ab_attrs.c',source[:a]+body.replace(old,new,1)+source[b:],raw))
fault('missing-old-immutable-gate','pr_weather_immutable(bt->weather) && bt->weather != w','bt->weather != w')
fault('wrong-old-replaceable-gate','pr_weather_immutable(bt->weather) && bt->weather != w','!pr_weather_immutable(bt->weather) && bt->weather != w')
fault('missing-weather-inequality','pr_weather_immutable(bt->weather) && bt->weather != w','pr_weather_immutable(bt->weather)')
fault('forbidden-extra-replaceability-gate','pr_weather_immutable(bt->weather) && bt->weather != w','pr_weather_immutable(bt->weather) && pr_weather_immutable(w) && bt->weather != w')
fault('missing-weather-simulation-gate','''        if (!pp->simulated) {
            if (!c->e) return -1;
            pr_try_set_weather''','''        if (true) {
            if (!c->e) return -1;
            pr_try_set_weather''')
fault('missing-terrain-simulation-gate','''        if (!pp->simulated) {
            if (!c->e) return -1;
            pr_try_set_terrain''','''        if (true) {
            if (!c->e) return -1;
            pr_try_set_terrain''')
fault('missing-terrain-inequality','GATE(bt->terrain != t);','GATE(true);')
fault('discarded-failed-weather-activation','pr_try_set_weather(bt, c->e, w, p);','return pr_try_set_weather(bt, c->e, w, p);')
fault('missing-weather-effect','pr_try_set_weather(bt, c->e, w, p);','(void)w;')
fault('missing-terrain-effect','pr_try_set_terrain(bt, c->e, t, false, p);','(void)t;')
fault('missing-terrain-animation','t, false, p','t, true, p')
# Accepted PostBiome weather changes are immutable and keep duration zero;
# changing only their duration owner is not an observable fault in this matrix.
fault('wrong-terrain-duration-owner','t, false, p);','t, false, NULL);')
for family in ['Weather','Terrain']:
 a=source.index('    case PR_AC_PostSummon'+family+'ChangeAbAttr: {');b=source.index('\n    case PR_AC_',a+1);part=source[a:b];needle='        GATE(';assert needle in part;changed=part.replace(needle,'        if (!c->e) return -1;\n'+needle,1);faults.append(('premature-simulation-engine-'+family,'ab_attrs.c',source[:a]+changed+source[b:],raw))
engine=(r/'battle-core/src/engine.c').read_text();a=engine.index('void pr_new_biome_encounter_callbacks(');b=engine.index('\n/* Fresh encounter',a);part=engine[a:b]
def owner_fault(name,old,new):
 assert part.count(old)==1,(name,part.count(old));faults.append((name,'engine.c',engine[:a]+part.replace(old,new,1)+engine[b:],prefix))
owner_fault('missing-battle-wave-reset','pr_reset_battle_data(p);','/* missing reset */')
owner_fault('only-reset-on-field','pr_reset_battle_data(p);','if(p->on_field)pr_reset_battle_data(p);')
owner_fault('only-first-player','k < bt->n_party[0]','k < 1')
owner_fault('reversed-player-order','int k = 0; k < bt->n_party[0]; k++','int k = bt->n_party[0]-1; k >= 0; k--')
owner_fault('dispatch-off-field','if (p->on_field) {','if (true) {')
owner_fault('erased-summon-state','pr_reset_battle_data(p);','pr_reset_battle_data(p);pr_reset_summon_data(p);')
owner_fault('wrong-summon-family','PR_AC_PostBiomeChangeAbAttr','PR_AC_PostSummonAbAttr')
flags=['-O2','-std=c99','-ffp-contract=off',*['-I'+str(r/'battle-core'/p) for p in ['include','gen/out','build']]]
with tempfile.TemporaryDirectory(prefix='biome-guards-',dir=r/'work') as tmp:
 d=Path(tmp)
 for name,filename,changed,trace in faults:
  src=d/filename;src.write_text(changed);obj=d/'fault.o';exe=d/'fault'
  subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
  subprocess.run(['gcc',*flags,r/'battle-core/tests/biome-callback-test.c',obj,r/'battle-core/build/libprcore.a','-lm','-o',exe],check=True,capture_output=True)
  p=subprocess.run([exe,trace],env=env,capture_output=True);assert p.returncode==1,(name,p.returncode,p.stdout[-500:],p.stderr[-500:]);print(name,'caught',flush=True)
 data=raw.read_bytes();first,rest=data.split(b'\n',1);bad=d/'bad.txt';count=0
 def reject(name,changed):
  global count
  bad.write_bytes(changed);p=subprocess.run([runner,bad],env=env,capture_output=True);assert p.returncode==2,(name,p.returncode);count+=1;print(name,'rejected',flush=True)
 reject('version',first.replace(b'BIOME_CALLBACK 1 ',b'BIOME_CALLBACK 3 ')+b'\n'+rest)
 reject('small',b'BIOME_CALLBACK 1 26879 777\n'+rest)
 reject('footer',data.rsplit(b'BIOME_CALLBACK_COMPLETE ',1)[0]+b'BIOME_CALLBACK_COMPLETE 1\n')
 reject('trailing',data+b'JUNK\n')
 for name,tag,col,value in [('fractional-case',b'C ',1,b'0.5'),('actor',b'C ',2,b'8'),('simulation',b'C ',3,b'2'),('gain',b'C ',4,b'2'),('weather',b'ENV ',1,b'10'),('duration',b'ENV ',2,b'12'),('terrain',b'ENV ',4,b'5'),('dynamic-animation',b'D ',8,b'65536')]:
  a=data.index(b'\n'+tag)+1;b=data.index(b'\n',a);row=data[a:b].split();row[col]=value;reject(name,data[:a]+b' '.join(row)+data[b:])
 data=prefix.read_bytes()
 for name,col,value in [('berry-count',4,b'3'),('berry-fraction',5,b'0.5'),('endured',3,b'2')]:
  a=data.index(b'\nID ')+1;b=data.index(b'\n',a);row=data[a:b].split();row[col]=value;reject(name,data[:a]+b' '.join(row)+data[b:])
 print(f'{len(faults)} compiled production faults caught; {count} malformed captures rejected')
