#include "evolution_ui.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    const uint16_t nincada_options[]={291,292}; /* Ninjask, Shedinja */
    FootballEvolutionMenu menu;
    unsigned selected;
    bool cancelled;
    for (unsigned pick=0;pick<2;++pick) {
        assert(football_evolution_open(&menu,nincada_options,2));
        assert(!football_evolution_take(&menu,&selected,&cancelled));
        football_evolution_update(&menu,(FootballCallInput){0});
        assert(!menu.ready); /* Neither outcome is automatically chosen. */
        football_evolution_update(&menu,(FootballCallInput){.touch_down=true,
            .touch_x=30,.touch_y=(uint16_t)(70+48*pick)});
        assert(football_evolution_take(&menu,&selected,&cancelled));
        assert(!cancelled && selected==pick);
        assert(!football_evolution_take(&menu,&selected,&cancelled));
    }
    assert(football_evolution_open(&menu,nincada_options,2));
    football_evolution_update(&menu,(FootballCallInput){.pressed=FB_INPUT_BACK});
    assert(football_evolution_take(&menu,&selected,&cancelled) && cancelled);
    assert(football_evolution_open(&menu,nincada_options,2));
    football_evolution_update(&menu,(FootballCallInput){.touch_down=true,.touch_x=30,.touch_y=166});
    assert(football_evolution_take(&menu,&selected,&cancelled) && cancelled);
    uint16_t many[FB_EVOLUTION_CAP];
    for (unsigned i=0;i<FB_EVOLUTION_CAP;++i) many[i]=(uint16_t)(i+1);
    assert(football_evolution_open(&menu,many,FB_EVOLUTION_CAP));
    football_evolution_update(&menu,(FootballCallInput){.pressed=FB_INPUT_UP});
    assert(menu.selected==FB_EVOLUTION_CAP && football_evolution_page_start(&menu)==15);
    football_evolution_update(&menu,(FootballCallInput){.pressed=FB_INPUT_ACCEPT});
    assert(football_evolution_take(&menu,&selected,&cancelled) && cancelled);
    assert(!football_evolution_open(&menu,many,FB_EVOLUTION_CAP+1));
    puts("football evolution menu: explicit Ninjask/Shedinja choices, cancellation and paging passed");
}
