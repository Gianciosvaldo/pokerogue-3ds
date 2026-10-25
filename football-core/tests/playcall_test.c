#include "playcall_ui.h"
#include "football_playbook.h"
#include <math.h>
#include <string.h>
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
        for (unsigned type=0;type<2;++type)
          for (unsigned choice=0;choice<2;++choice) {
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
            assert(ui.state==FB_CALL_PLAY);
            assert(!football_playcall_take(&ui,&out));
            const FootballPlayDefinition *p=football_playbook_option(formation,type,choice);
            assert(p && !strcmp(football_playcall_label(&ui,choice),p->name));
            tap(&ui,30,70+64*choice);
            assert(football_playcall_take(&ui,&out));
            assert((unsigned)out.formation==formation && (unsigned)out.type==type);
            assert(out.id==p->id && football_playbook_valid(out));
            assert(!football_playcall_take(&ui,&out));
        }
    football_playcall_reset(&ui);
    press(&ui,FB_INPUT_ACCEPT);
    press(&ui,FB_INPUT_UP); /* Wrap to Under Center. */
    assert(ui.highlighted==1);
    press(&ui,FB_INPUT_ACCEPT);
    press(&ui,FB_INPUT_DOWN);
    press(&ui,FB_INPUT_ACCEPT);
    assert(ui.state==FB_CALL_PLAY);
    press(&ui,FB_INPUT_ACCEPT);
    assert(ui.state==FB_CALL_READY);
    press(&ui,FB_INPUT_BACK);
    assert(ui.state==FB_CALL_TYPE);
    assert(!football_playcall_take(&ui,&out));
    press(&ui,FB_INPUT_BACK);
    assert(ui.state==FB_CALL_FORMATION);
    press(&ui,FB_INPUT_BACK|FB_INPUT_ACCEPT);
    assert(ui.state==FB_CALL_MAIN); /* Back wins simultaneous inputs. */
    tap(&ui,400,60);
    assert(ui.state==FB_CALL_MAIN);
    assert(!football_playbook_get((FootballPlayId)-1));
    assert(!football_playbook_get(FB_PLAY_COUNT));
    assert(!football_playbook_option(FB_FORM_SHOTGUN,FB_PLAY_RUN,2));
    assert(!football_playbook_count((FootballFormation)99,FB_PLAY_RUN));
    for (unsigned i=0;i<FB_PLAY_COUNT;++i) {
        const FootballPlayDefinition *p=football_playbook_get((FootballPlayId)i);
        assert(p && p->route_count<=FB_ROUTE_POINTS);
        for (unsigned j=0;j<=100;++j) {
            FootballRoutePoint point=football_playbook_route(p,j/100.f);
            assert(isfinite(point.x) && isfinite(point.y));
        }
        FootballRoutePoint end=football_playbook_route(p,1),bad=football_playbook_route(p,NAN);
        assert(end.x==p->route[p->route_count-1].x && bad.x==0 && bad.y==0);
    }
    puts("football playcalling: all eight named plays, route samples, touch edges, back and single delivery passed");
}
