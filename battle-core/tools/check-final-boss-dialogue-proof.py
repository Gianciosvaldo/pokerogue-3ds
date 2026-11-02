#!/usr/bin/env python3
"""Strict native dialogue prefix replay, forged captures and compiled faults."""
from pathlib import Path
import argparse,gzip,os,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2];CORE=ROOT/'battle-core'
p=argparse.ArgumentParser();p.add_argument('--runner',type=Path);p.add_argument('--library',type=Path);p.add_argument('--replay-only',action='store_true');a=p.parse_args()
lib=(a.library or CORE/'build/libprcore.a').resolve();runner=(a.runner or CORE/'build/final-boss-dialogue-test').resolve()
if not a.runner:subprocess.run(['make','-s','-C',str(CORE),'build/final-boss-dialogue-test'],check=True)
capture=gzip.decompress((CORE/'tests/fixtures/final-boss-dialogue.native.txt.gz').read_bytes());locale=CORE/'tests/fixtures/final-boss-dialogue.en.json'
assert capture.startswith(b'FINAL_BOSS_DIALOGUE 1 640\n') and capture.endswith(b'COMPLETE 640\n') and capture.count(b'\nC ')==640
with tempfile.TemporaryDirectory(prefix='final-boss-dialogue-') as d:
 d=Path(d);trace=d/'native.txt';trace.write_bytes(capture)
 def run(binary,path):return subprocess.run([str(binary),str(path),str(locale)],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
 result=run(runner,trace);assert result.returncode==0,result.stderr.decode();print(result.stdout.decode(),end='',flush=True)
 if a.replay_only:raise SystemExit(0)
 lines=capture.splitlines(keepends=True)
 mutations={'truncated':b''.join(lines[:-2]),'extra-tail':capture+b'EXTRA\n','wrong-count':capture.replace(b'1 640\n',b'1 639\n',1),'reordered-context':capture.replace(b'C 0 0 199',b'C 0 1 199',1),'nonfinite':capture.replace(b'R 1 ',b'R nan ',1),'wrong-footer':capture.replace(b'COMPLETE 640',b'COMPLETE 639')}
 for marker,label,value in [(b'K ','key',b'77726f6e67'),(b'N ','speaker',b'4d6577'),(b'T ','localized-text',b'4e6f7065')]:
  copy=lines[:];i=next(i for i,l in enumerate(copy) if l.startswith(marker));copy[i]=marker+value+b'\n';mutations['forged-'+label]=b''.join(copy)
 start=next(i for i,l in enumerate(lines) if l==b'D 1\n')
 for offset,label,token,value in [(4,'dialogue-hp',1,b'1'),(5,'global-rng',1,b'17'),(6,'battle-rng',1,b'17')]:
  copy=lines[:];parts=copy[start+offset].split();parts[token]=value;copy[start+offset]=b' '.join(parts)+b'\n';mutations['forged-'+label]=b''.join(copy)
 for label,content in mutations.items():
  path=d/(label+'.txt');path.write_bytes(content);result=run(runner,path);assert result.returncode in (1,2),(label,result.returncode);print('caught capture',label,flush=True)
 source=(CORE/'src/phases.c').read_text();event='pr_event_add(bt, e, PR_EV_DIALOGUE, f->battler, -1, p->species, 0, &dialogue);';gate='if (is_classic_final_boss(bt) && !player) {'
 faults={
  'omit-dialogue':(event,'(void)dialogue;'),
  'ordinary-text':(event,event.replace('PR_EV_DIALOGUE','PR_EV_TEXT')),
  'wrong-speaker':(event,event.replace('p->species','SPECIES_ID_MEW')),
  'wrong-battler':(event,event.replace('f->battler','0')),
  'wrong-key':('pr_msg dialogue = pr_msg0("battleSpecDialogue:secondStageWin");','pr_msg dialogue = pr_msg0("battleSpecDialogue:firstStageWin");'),
  'all-wave-200-modes':(gate,'if (bt->wave_index == 200 && !player) {'),
  'exclude-challenge':(gate,'if (bt->game_mode == GAME_MODES_CLASSIC && bt->wave_index == 200 && !player) {'),
  'admit-other-waves':(gate,'if ((bt->game_mode == GAME_MODES_CLASSIC || bt->game_mode == GAME_MODES_CHALLENGE) && !player) {'),
  'admit-player':(gate,'if (is_classic_final_boss(bt)) {'),
  'admit-first-form':('''        if (p->form > 0) {
            /* ui.showDialogue''','''        if (p->form >= 0) {
            /* ui.showDialogue'''),
  'skip-faint-callback':(event,event+' return;'),
  'changed-before-dialogue':(event,'p->hp = 1; '+event),
  'extra-global-roll':(event,'(void)pr_rng_seed_int(&bt->rng,100,0); '+event),
  'extra-battle-roll':(event,'(void)pr_rng_seed_int(&bt->battle_rng,100,0); '+event),
 }
 cc=os.environ.get('CC','cc');flags=['-O1','-std=c99','-ffp-contract=off','-Wl,--strip-debug','-I'+str(CORE/'include'),'-I'+str(CORE/'gen/out')]
 for label,(old,new) in [('pristine',(None,None)),*faults.items()]:
  altered=source
  if old is not None:assert source.count(old)==1,(label,'anchor');altered=source.replace(old,new)
  path=d/'phases.c';path.write_text(altered);obj=d/'phases.o';binary=d/'fault'
  subprocess.run([cc,*flags,'-c',str(path),'-o',str(obj)],check=True)
  subprocess.run([cc,*flags,str(CORE/'tests/final-boss-dialogue-test.c'),str(obj),str(lib),'-lm','-o',str(binary)],check=True)
  result=run(binary,trace);assert result.returncode==(0 if label=='pristine' else 1),(label,result.returncode,result.stderr.decode());print('compiled',label,'passed' if label=='pristine' else 'caught',flush=True)
 print(f'Dialogue prefix proof: {len(faults)} compiled faults, one pristine control, {len(mutations)} malformed/forged captures')
