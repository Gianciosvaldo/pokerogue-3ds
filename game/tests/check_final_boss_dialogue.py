#!/usr/bin/env python3
"""Compile the real converter and dialogue-player failure controls."""
from pathlib import Path
import argparse,os,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2];APP=ROOT/'game';TESTS=APP/'tests'
p=argparse.ArgumentParser();p.add_argument('--library',type=Path);a=p.parse_args();lib=(a.library or TESTS/'build/ptx_core.a').resolve()
if not lib.exists():subprocess.run(['make','-C',str(TESTS),'build/ptx_core.a'],check=True)
cc=os.environ.get('CC','cc')
includes=['game/tests','game/include','game/menus','game/gen','game-core/include','run-core/include','reward-core/include','ai-core/include','ai-core/gen/out','battle-core/include','battle-core/gen/out','assets-pipeline/app-src/include']
flags=['-O1','-std=gnu11','-ffp-contract=off','-ffunction-sections','-fdata-sections','-Wl,--strip-debug',*['-I'+str(ROOT/x) for x in includes]]
converter=(APP/'source/battle_events.c').read_text();player=(APP/'source/battle_view.c').read_text()
controls={
 'converter':[
  ('premature-catchup-faint','p->hp <= 0 && !seen[k].fainted && !final_boss_pending','p->hp <= 0 && !seen[k].fainted'),
  ('plain-message','.type = BEV_DIALOGUE, .battler = b, .a = ev->id','.type = BEV_MESSAGE, .battler = b, .a = ev->id'),
  ('live-speaker','i18n_species_name(ev->id)','i18n_species_name(bt->field[b]?bt->field[b]->species:SPECIES_ID_PIKACHU)'),
  ('faint-before-dialogue','PUT(.type = BEV_DIALOGUE, .battler = b, .a = ev->id','PUT(.type = BEV_FAINT, .battler = b); PUT(.type = BEV_DIALOGUE, .battler = b, .a = ev->id'),
  ('missing-file-stops-fallback','if (!json) { *len = 2; return "{}"; }','if (!json) return NULL;'),
  ('wrong-dialogue-text','be_format_message(&ev->msg, g_txt[nt], sizeof g_txt[nt]);','snprintf(g_txt[nt],sizeof g_txt[nt],"wrong");'),
 ],
 'player':[
  ('no-prompt','B.speaker = e->name && *e->name ? e->name : NULL;\n    msgbox_show(&B.msg, e->text ? e->text : "", -1, true);','B.speaker = e->name && *e->name ? e->name : NULL;\n    msgbox_show(&B.msg, e->text ? e->text : "", -1, false);'),
  ('auto-dismiss','if (msgbox_done(&B.msg)) {\n      B.speaker = NULL;','if (msgbox_done(&B.msg) || B.msg.waiting) {\n      B.speaker = NULL;'),
  ('missing-speaker','B.speaker = e->name && *e->name ? e->name : NULL;','B.speaker = NULL;'),
  ('premature-sprite-hide','static bool step_dialogue(const bv_event *e, float dt) {','static bool step_dialogue(const bv_event *e, float dt) { B.s[1].present=false;'),
 ]}
with tempfile.TemporaryDirectory(prefix='app-final-boss-dialogue-') as td:
 temp=Path(td)
 for kind,source in [('converter',converter),('player',player)]:
  for label,old,new in [('pristine',None,None),*controls[kind]]:
   altered=source
   if old is not None:assert source.count(old)==1,(kind,label,'anchor');altered=source.replace(old,new)
   path=temp/'owner.c';path.write_text(altered);binary=temp/'test'
   if kind=='converter':args=[TESTS/'final_boss_dialogue_events_test.c',TESTS/'battle_setup.c',path,lib]
   else:
    test=temp/'playback.c';test.write_text((TESTS/'final_boss_dialogue_playback_test.c').read_text().replace('#include "../source/battle_view.c"','#include "'+str(path)+'"'))
    args=[test,APP/'source/ui_widgets.c',APP/'source/tween.c']
   built=subprocess.run([cc,*flags,*map(str,args),'-Wl,--gc-sections','-lm','-o',str(binary)],capture_output=True,text=True);assert built.returncode==0,(kind,label,built.stderr)
   result=subprocess.run([str(binary)],cwd=TESTS,capture_output=True,text=True)
   assert (result.returncode==0)==(label=='pristine'),(kind,label,result.returncode,result.stdout,result.stderr)
   print(kind,label,'passed' if label=='pristine' else 'caught',flush=True)
 print('App dialogue: ten compiled faults caught, two pristine controls passed')
