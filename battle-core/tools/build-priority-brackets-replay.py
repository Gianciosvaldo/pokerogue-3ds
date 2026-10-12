#!/usr/bin/env python3
"""Add actual source bracket probes to an owning copy; comparison remains unchanged."""
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text()
def replace(a,b):
 global s
 assert s.count(a)==1,a
 s=s.replace(a,b,1)
replace('    int picks[8], n_picks;', '    uint16_t bracket_moves[2];\n    uint8_t bracket_mask;\n    int picks[8], n_picks;')
replace('        } else if (!strncmp(l, "PRE ", 4)) {', '        } else if (!strncmp(l, "BRACKET ", 8)) {\n            int b = (int)inum(l, "b"), move = (int)inum(l, "move");\n            if ((b != 0 && b != 2) || move <= 0 || move >= PR_MOVE_COUNT || (cs->bracket_mask & (1u << (b / 2)))) exit(2);\n            cs->bracket_moves[b / 2] = (uint16_t)move; cs->bracket_mask |= 1u << (b / 2);\n            (void)need(l, "value");\n        } else if (!strncmp(l, "PRE ", 4)) {')
replace('\n}\n\nstatic void dump_trace_queue', '\n    if (dump_before) {\n        if (cs->bracket_mask != 3) exit(2);\n        pr_battle probe = *bt;\n        static pr_engine simulation_engine;\n        memset(&simulation_engine,0,sizeof simulation_engine); probe.engine=&simulation_engine;\n        for(int side=0;side<2;side++){\n            pr_ab_params params={.pokemon=probe.field[side*2],.move=-1,.simulated=true};\n            pr_apply_ab_attrs(&probe,PR_AC_BypassSpeedChanceAbAttr,&params);\n        }\n        if(memcmp(&probe.rng,&bt->rng,sizeof probe.rng)||memcmp(&probe.battle_rng,&bt->battle_rng,sizeof probe.battle_rng)) {fputs("simulated Quick Draw advanced RNG\\n",stderr);exit(1);}\n        for (int side = 0; side < 2; side++)\n            add_line(L, "BRACKET b=%d move=%d value=%.17g", side * 2, cs->bracket_moves[side],\n                     (double)pr_move_get_priority_modifier(&probe, cs->bracket_moves[side], probe.field[side * 2]));\n    }\n}\n\nstatic void dump_trace_queue')
Path(sys.argv[2]).write_text(s)
