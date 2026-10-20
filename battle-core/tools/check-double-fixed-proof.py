#!/usr/bin/env python3
"""Retain fixed-rival counterexamples, including Challenge and both player genders."""
from pathlib import Path
import os,subprocess,sys,tempfile
r=Path(__file__).resolve().parents[2];b=r/'battle-core';trace=Path(sys.argv[1]).resolve();env=os.environ.copy();assert env.get('TURNTEST_LOCALES');runner=b/'build/double-generation-test';subprocess.run([runner,trace],env=env,check=True)
s=(r/'run-core/src/newbattle.c').read_text();faults=[('challenge-rival-omitted','mode != GAME_MODES_CLASSIC && mode != GAME_MODES_CHALLENGE','mode != GAME_MODES_CLASSIC'),('rival-player-gender-omitted','fixed_rival >= 0 && !ctx->player_female ? TRAINER_VARIANT_FEMALE : TRAINER_VARIANT_DEFAULT','TRAINER_VARIANT_DEFAULT')]
flags=['-O2','-std=c99','-ffp-contract=off',*['-I'+str(b/p) for p in ['include','gen/out','build']],'-I'+str(r/'run-core/include')]
with tempfile.TemporaryDirectory(prefix='fixed-double-guards-',dir=r/'work') as tmp:
 d=Path(tmp)
 for name,old,new in faults:
  assert s.count(old)==1;src=d/'newbattle.c';src.write_text(s.replace(old,new,1));obj=d/'newbattle.o';exe=d/'fault'
  subprocess.run(['gcc',*flags,'-c',src,'-o',obj],capture_output=True,check=True)
  subprocess.run(['gcc',*flags,b/'tests/double-generation-test.c',obj,b/'build/double-chance-run/librun.a',b/'build/libprcore.a','-lm','-o',exe],capture_output=True,check=True)
  p=subprocess.run([exe,trace],env=env,capture_output=True);assert p.returncode==1,(name,p.returncode,p.stderr[-300:]);print(name,'caught',flush=True)
 data=trace.read_bytes();bad=d/'bad.txt';a=data.index(b'\nC ')+1;z=data.index(b'\n',a);row=data[a:z].split();row[9]=b'2';bad.write_bytes(data[:a]+b' '.join(row)+data[z:]);p=subprocess.run([runner,bad],env=env,capture_output=True);assert p.returncode==2
 print('2 fixed-rival faults caught; malformed player gender rejected',flush=True)
