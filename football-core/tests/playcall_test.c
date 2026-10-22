#include "playcall_ui.h"
#include <assert.h>
#include <stdio.h>
static void press(FootballPlaycall *ui, unsigned keys)
{
    football_playcall_update(ui,(FootballCallInput){.pressed=keys});
}
static void tap(FootballPlaycall *ui, unsigned x, unsigned y)
{
    football_playcall_update(ui,(FootballCallInput){.touch_down=true,.touch_x=x,.touch_y=y});
}
int main(void)
{
    FootballPlaycall ui;
    PlayInfo out;
    for (unsigned formation=0;formation<2;++formation)
        for (unsigned type=0;type<2;++type) {
            football_playcall_reset(&ui);
            assert(!football_playcall_take(&ui,&out));
            tap(&ui,300,60); /* Right boundary is outside. */
            assert(ui.state==FB_CALL_MAIN);
            tap(&ui,20,60);
            assert(ui.state==FB_CALL_FORMATION);
            tap(&ui,30,70+64*formation);
            assert(ui.state==FB_CALL_TYPE);
            football_playcall_update(&ui,(FootballCallInput){0});
            assert(ui.state==FB_CALL_TYPE); /* Held touch is not a new tap. */
            tap(&ui,30,70+64*type);
            assert(football_playcall_take(&ui,&out));
            assert((unsigned)out.formation==formation && (unsigned)out.type==type);
            assert((unsigned)out.id==formation*2+type);
            assert(!football_playcall_take(&ui,&out));
        }
    football_playcall_reset(&ui);
    press(&ui,FB_INPUT_ACCEPT);
    press(&ui,FB_INPUT_UP); /* Wrap to Under Center. */
    assert(ui.highlighted==1);
    press(&ui,FB_INPUT_ACCEPT);
    press(&ui,FB_INPUT_DOWN);
    press(&ui,FB_INPUT_ACCEPT);
    assert(ui.state==FB_CALL_READY);
    press(&ui,FB_INPUT_BACK);
    assert(ui.state==FB_CALL_FORMATION);
    assert(!football_playcall_take(&ui,&out));
    press(&ui,FB_INPUT_BACK|FB_INPUT_ACCEPT);
    assert(ui.state==FB_CALL_MAIN); /* Back wins simultaneous inputs. */
    tap(&ui,400,60);
    assert(ui.state==FB_CALL_MAIN);
    puts("football playcalling: all four plays, touch edges, back and single delivery passed");
}
