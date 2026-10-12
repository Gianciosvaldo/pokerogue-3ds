#!/usr/bin/env python3
"""Add amount/virtual transport to an owning copy; comparison remains unchanged."""
from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text()
def replace(a,b):
 global s
 assert s.count(a)==1,a
 s=s.replace(a,b,1)
replace('            m->is_transferable = true;', '''            m->is_transferable = true;
            if (m->cls == PR_MOD_MOVE_ACCURACY_BOOSTER) {
                m->num = (int16_t)inum(l, "amount");
                m->virtual_stack_count = (int32_t)inum(l, "virtual");
            }''')
replace('            if (m->cls == PR_MOD_BERRY)\n                add_line', '''            if (m->cls == PR_MOD_MOVE_ACCURACY_BOOSTER)
                add_line(L, "MOD side=%d cls=%s pid=%lld stack=%d amount=%d virtual=%d", side,
                         pr_mod_class_name[m->cls], (long long)m->pokemon_id, m->stack_count,
                         m->num, m->virtual_stack_count);
            else if (m->cls == PR_MOD_BERRY)
                add_line''')
replace('    int picks[8], n_picks;', '    uint16_t accuracy_moves[2];\n    uint8_t accuracy_mask;\n    int picks[8], n_picks;')
replace('        } else if (!strncmp(l, "PRE ", 4)) {', '        } else if (!strncmp(l, "ACCURACY ", 9)) {\n            int b = (int)inum(l, "b"), move = (int)inum(l, "move");\n            if ((b != 0 && b != 2) || move <= 0 || move >= PR_MOVE_COUNT || (cs->accuracy_mask & (1u << (b / 2)))) exit(2);\n            cs->accuracy_moves[b / 2] = (uint16_t)move; cs->accuracy_mask |= 1u << (b / 2);\n            (void)need(l, "value");\n        } else if (!strncmp(l, "PRE ", 4)) {')
replace('\n}\n\nstatic void dump_trace_queue', '\n    if (dump_before) {\n        if (cs->accuracy_mask != 3) exit(2);\n        pr_battle probe = *bt;\n        for (int side = 0; side < 2; side++)\n            add_line(L, "ACCURACY b=%d move=%d value=%.17g", side * 2, cs->accuracy_moves[side],\n                     pr_calculate_battle_accuracy(&probe, probe.field[side * 2], probe.field[(1 - side) * 2], cs->accuracy_moves[side]));\n    }\n}\n\nstatic void dump_trace_queue')
Path(sys.argv[2]).write_text(s)
