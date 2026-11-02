#!/usr/bin/env python3
"""Strict native FaintPhase prefix replay and compiled behavior faults."""
from pathlib import Path
import argparse,gzip,os,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2];CORE=ROOT/'battle-core'
p=argparse.ArgumentParser();p.add_argument('--runner',type=Path);p.add_argument('--library',type=Path);p.add_argument('--replay-only',action='store_true');a=p.parse_args()
lib=(a.library or CORE/'build/libprcore.a').resolve();runner=(a.runner or CORE/'build/final-boss-faint-test').resolve()
if not a.runner:subprocess.run(['make','-s','-C',str(CORE),'build/final-boss-faint-test'],check=True)
capture=gzip.decompress((CORE/'tests/fixtures/final-boss-faint.native.txt.gz').read_bytes())
assert capture.startswith(b'FINAL_BOSS_FAINT 1 480\n') and capture.endswith(b'COMPLETE 480\n') and capture.count(b'\nC ')==480
with tempfile.TemporaryDirectory(prefix='final-boss-faint-') as d:
 d=Path(d);trace=d/'native.txt';trace.write_bytes(capture)
 def run(binary,path):return subprocess.run([str(binary),str(path)],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
 result=run(runner,trace);assert result.returncode==0,result.stderr.decode();print(result.stdout.decode(),end='')
 if a.replay_only:raise SystemExit(0)
 lines=capture.splitlines(keepends=True)
 mutations={'truncated':b''.join(lines[:-2]),'extra-tail':capture+b'EXTRA\n','wrong-count':capture.replace(b'1 480\n',b'1 479\n',1),'reordered-context':capture.replace(b'C 0 0 199',b'C 0 1 199',1),'nonfinite':capture.replace(b'R 1 ',b'R nan ',1),'wrong-footer':capture.replace(b'COMPLETE 480',b'COMPLETE 479')}
 start=next(i for i,l in enumerate(lines) if l.startswith(b'C 36 '));called=next(i for i in range(start,len(lines)) if lines[i].startswith(b'CALLED '))
 for offset,name,token,value in [(1,'forged-hp',1,b'0'),(2,'forged-global-rng',1,b'17'),(3,'forged-battle-rng',1,b'17'),(4,'forged-queue-battler',4,b'0')]:
  copy=lines[:];i=called+offset;parts=copy[i].split();parts[token]=value;copy[i]=b' '.join(parts)+b'\n';mutations[name]=b''.join(copy)
 copy=lines[:];i=next(i for i,l in enumerate(copy) if l.startswith(b'CALLED '));copy[i]=b'CALLED 0\n';mutations['forged-ordinary-admission']=b''.join(copy)
 for name,content in mutations.items():
  path=d/(name+'.txt');path.write_bytes(content);result=run(runner,path);assert result.returncode in (1,2),(name,result.returncode);print('caught capture',name)
 source=(CORE/'src/phases.c').read_text()
 gate='if (is_classic_final_boss(bt) && !player) {'
 prefix='''    reset_summon_data(p);
    if (!f->prevent_instant_revive) {'''
 faults={
  'all-wave-200-modes':(gate,'if (bt->wave_index == 200 && !player) {'),
  'classic-only':(gate,'if (bt->game_mode == GAME_MODES_CLASSIC && bt->wave_index == 200 && !player) {'),
  'wrong-wave':(gate,'if ((bt->game_mode == GAME_MODES_CLASSIC || bt->game_mode == GAME_MODES_CHALLENGE) && !player) {'),
  'cancel-player':(gate,'if (is_classic_final_boss(bt)) {'),
  'keep-fainted-hp':('''            p->hp++;
            int damage''','''            (void)p;
            int damage'''),
  'wrong-damage-amount':('e->pool[damage].anim_amount = 0;','e->pool[damage].anim_amount = 1;'),
  'wrong-damage-result':('e->pool[damage].anim_result = HIT_RESULT_INDIRECT;','e->pool[damage].anim_result = HIT_RESULT_EFFECTIVE;'),
  'wrong-recipient':('int damage = pr_phase_new(e, PH_DAMAGE_ANIM, f->battler);','int damage = pr_phase_new(e, PH_DAMAGE_ANIM, 0);'),
  'wrong-queue-level':('''            pr_phase_unshift(bt, e, damage);
            return;''','''            pr_phase_push(bt, e, damage);
            return;'''),
  'continue-ordinary-faint':('''            pr_phase_unshift(bt, e, damage);
            return;''','''            pr_phase_unshift(bt, e, damage);
            (void)p;'''),
  'skip-summon-reset':(prefix,'''    (void)p;
    if (!f->prevent_instant_revive) {'''),
  'skip-participant':('''if (q && (is_active(q) || q->hp <= 0)) add_participant(bt, q->id);''','''if (q && is_active(q) && q->hp > 0) { (void)q; }'''),
  'extra-global-roll':('''            p->hp++;
            int damage''','''            p->hp++;
            (void)pr_rng_seed_int(&bt->rng, 100, 0);
            int damage'''),
  'extra-battle-roll':('''            p->hp++;
            int damage''','''            p->hp++;
            (void)pr_rng_seed_int(&bt->battle_rng, 100, 0);
            int damage'''),
 }
 cc=os.environ.get('CC','cc');flags=['-O1','-std=c99','-ffp-contract=off','-Wl,--strip-debug','-I'+str(CORE/'include'),'-I'+str(CORE/'gen/out')]
 for name,(old,new) in [('pristine',(None,None)),*faults.items()]:
  altered=source
  if old is not None:assert source.count(old)==1,(name,'anchor');altered=source.replace(old,new)
  path=d/'phases.c';path.write_text(altered);obj=d/'phases.o';binary=d/'fault'
  subprocess.run([cc,*flags,'-c',str(path),'-o',str(obj)],check=True)
  subprocess.run([cc,*flags,str(CORE/'tests/final-boss-faint-test.c'),str(obj),str(lib),'-lm','-o',str(binary)],check=True)
  result=run(binary,trace)
  assert result.returncode==(0 if name=='pristine' else 1),(name,result.returncode,result.stderr.decode())
  print('compiled',name,'passed' if name=='pristine' else 'caught')
 print(f'Final boss faint prefix proof: {len(faults)} compiled faults, one pristine control, {len(mutations)} malformed/forged captures')
