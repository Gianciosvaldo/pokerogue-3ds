/* The title screen (source/title_screen.c) with the timed events: the banner on the top screen and the EVENTS panel on the bottom one, over
 * the real events table and the real clock adapter pinned to known instants, with a fake drawing layer that records every window and text.
 * Checks where things are drawn (inside the panel, left of the option list, inside the banner), that nothing overflows at two different
 * font widths (the text is cut, not drawn past its box), what the pages show and that they turn with the clock.
 * make title_events_test && build/title_events_test */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "events_host.h"
#include "gm_enum_names.h"
#include "title_screen.h"
#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

static int checks;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "title_events FAIL line %d: %s\n", __LINE__, #x); exit(1); } checks++; } while (0)

/* ------------------------------------------------------------------ fakes */
typedef struct { float x, y, origin_x, w; int style; char text[160]; } drawn_t;
typedef struct { float x, y, w, h; } win_t;
#define MAX 256
static drawn_t g_text[MAX];
static win_t g_win[16];
static int n_text, n_win, g_which;
static float g_char_w = 6.0f;   /* the fake font's width of one character, set per run */
static ui_screen* g_top;
static ui_space_id g_space;

void ui_set(ui_screen* s) { g_top = s; }
bool ui_space(ui_space_id sp) {
  g_space = sp;
  const bool top = sp == UI_FIELD || sp == UI_FIELD_UI || sp == UI_TOP_CANVAS || sp == UI_TOP_CANVAS_UI || sp == UI_PHYS_TOP;
  return top == (g_which == 0);
}
void rr_rect(float x, float y, float w, float h, rr_color c) { (void)x, (void)y, (void)w, (void)h, (void)c; }
void ui_window(float x, float y, float w, float h, win_variant v) {
  (void)v;
  if (n_win < 16) g_win[n_win++] = (win_t){x, y, w, h};
}
text_opts text_defaults(void) { text_opts o; memset(&o, 0, sizeof o); o.chars = TEXT_ALL; return o; }
/* the small style is narrower than the others, as upstream's 8 px against its 16 px fonts */
static float width_of(text_style st, const char* s) { return (st == TS_INSTRUCTIONS_TEXT ? 0.8f : 1.0f) * g_char_w * (float)strlen(s); }
float text_width(text_style st, const char* s, const text_opts* o) { (void)o; return width_of(st, s); }
float text_draw(text_style st, float x, float y, const char* s, const text_opts* o) {
  if (n_text < MAX) {
    drawn_t* d = &g_text[n_text++];
    d->x = x; d->y = y; d->origin_x = o ? o->origin_x : 0; d->w = width_of(st, s); d->style = (int)st;
    snprintf(d->text, sizeof d->text, "%s", s);
  }
  return width_of(st, s);
}
bool img_has(const char* k) { (void)k; return false; }
void img_draw(const char* k, const char* f, float x, float y, float ox, float oy) { (void)k, (void)f, (void)x, (void)y, (void)ox, (void)oy; }
void field_border_draw(const char* k) { (void)k; }
void bgm_on_title(void) {}
const char* i18n_t(char* out, int n, const char* key, ...) { snprintf(out, (size_t)n, "%s", key); return out; }
const char* gm_enum_lower(gm_enum e, int value, char* out, int n) { (void)e, (void)value; snprintf(out, (size_t)n, "plains"); return out; }
void optlist_init(optlist* l, const char* const* labels, int n) { memset(l, 0, sizeof *l); l->n = n; (void)labels; l->chosen = -1; }
bool optlist_input(optlist* l, ui_button b) { (void)l, (void)b; return false; }
bool optlist_touch(optlist* l, float x, float y) { (void)l, (void)x, (void)y; return false; }
void optlist_draw(const optlist* l) { (void)l; }

void gc_run_set_events(struct gc_run *run, const gc_event_source *src) { (void)run, (void)src; }

/* ------------------------------------------------------------------ helpers */
static int64_t T(int64_t day, int h, int m, int s) { return day * 86400 + h * 3600 + m * 60 + s; }
static void draw(int which) {
    n_text = n_win = 0;
    g_which = which;
    g_top->draw(g_top, which ? RR_BOTTOM : RR_TOP);
}
static bool has(const char* needle) {
    for (int i = 0; i < n_text; i++) if (strstr(g_text[i].text, needle)) return true;
    return false;
}
static const drawn_t* find(const char* exact) {
    for (int i = 0; i < n_text; i++) if (!strcmp(g_text[i].text, exact)) return &g_text[i];
    return NULL;
}

/* every text of the bottom screen lies inside the EVENTS window, which ends before the option list's window begins */
static void check_panel_geometry(void) {
    CHECK(n_win == 1);
    const win_t w = g_win[0];
    CHECK(w.x >= 0 && w.x + w.w <= 214);        /* the option list's window starts at about x = 214 (its widest label) */
    CHECK(w.y >= -240 && w.y + w.h <= 0);
    for (int i = 0; i < n_text; i++) {
        const drawn_t* d = &g_text[i];
        const float left = d->origin_x > 0.5f ? d->x - d->w : d->x;
        const float right = d->origin_x > 0.5f ? d->x : d->x + d->w;
        if (left < w.x + 8 - 0.01f || right > w.x + w.w - 4 + 0.01f) {
            fprintf(stderr, "text \"%s\" at %.1f..%.1f is outside the window %.1f..%.1f\n", d->text, left, right, w.x + 8, w.x + w.w - 4);
            exit(1);
        }
        CHECK(d->y >= w.y && d->y + 8 <= w.y + w.h);   /* at least its first 8 px inside, above the window's bottom edge */
        /* a row's name is drawn first and its time right after it, on the same line (the time 2 px lower): they must not run into each other */
        if (i > 0 && d->origin_x > 0.5f && d->style == TS_INSTRUCTIONS_TEXT) {
            const drawn_t* p = &g_text[i - 1];
            if (p->origin_x < 0.5f && p->y >= d->y - 3 && p->y <= d->y) CHECK(p->x + p->w + 2 <= left);
        }
    }
}

int main(void) {
    title_args a = {true, -1, NULL, NULL};
    title_screen_open(&a);
    CHECK(g_top != NULL);

    /* day 20773, 12:00: six open (all closing at midnight), six coming; the pages hold seven rows each at most */
    eh_use_fixed_clock(T(20773, 12, 0, 0));
    for (int pass = 0; pass < 2; pass++) {
        g_char_w = pass ? 9.0f : 6.0f;   /* a narrow and a wide font: the same boxes hold at both, by cutting the text */
        draw(1);
        CHECK(find("EVENTS") != NULL);
        CHECK(find("1/2") != NULL);
        CHECK(find("ON NOW") != NULL);
        CHECK(find("COMING UP") != NULL);
        if (!pass) CHECK(has("Lucky Catch") && has("Sweet Tooth") && has("Voucher Plus Day") && has("Delibirdy Day") && has("Waiting Room") && has("Snowy Town"));
        CHECK(has("12 h"));                       /* what is left of an open one */
        CHECK(has("today 13:00"));                /* when the first coming one opens */
        CHECK(!has("Candy Jar Day"));             /* page 2 */
        check_panel_geometry();
        /* the page turns by the clock: five seconds on, the second page, with the coming ones that did not fit */
        eh_use_fixed_clock(T(20773, 12, 0, 5));
        draw(1);
        CHECK(find("2/2") != NULL);
        if (!pass) {   /* the wide font cuts the long names: only the narrow one shows them whole */
            CHECK(has("Candy Jar Day") && has("Fusion Day") && has("tomorrow 00:00"));
            CHECK(has("Charm Hour") && has("Golden Hour") && has("Lucky Species Hour"));
        }
        CHECK(has("tomorrow 00:00"));
        CHECK(!has("Lucky Catch"));
        CHECK(find("COMING UP") != NULL && find("ON NOW") == NULL);
        check_panel_geometry();
        eh_use_fixed_clock(T(20773, 12, 0, 10));   /* and round again */
        draw(1);
        CHECK(find("1/2") != NULL);
        eh_use_fixed_clock(T(20773, 12, 0, 0));
    }
    g_char_w = 6.0f;

    /* the banner: the event that closes first, inside its window in the field (UI_FIELD: 320x180) */
    eh_use_fixed_clock(T(20773, 13, 30, 0));
    draw(0);
    CHECK(has("Shiny Trainers is on"));
    CHECK(has("Trainer shiny chance 1 in 10. 30 min left."));
    CHECK(n_win == 1);
    CHECK(g_win[0].x >= 0 && g_win[0].x + g_win[0].w <= 320 && g_win[0].y >= 0 && g_win[0].y + g_win[0].h <= 180);
    for (int i = 0; i < n_text; i++) {
        if (!strcmp(g_text[i].text, "PokeRogue")) continue;   /* the logo stand-in */
        CHECK(g_text[i].x >= g_win[0].x && g_text[i].x + g_text[i].w <= g_win[0].x + g_win[0].w);
        CHECK(g_text[i].y >= g_win[0].y && g_text[i].y < g_win[0].y + g_win[0].h);
    }
    /* a wide font cuts the headline to fit instead of running out of the window */
    g_char_w = 12.0f;
    draw(0);
    for (int i = 0; i < n_text; i++) {
        if (!strcmp(g_text[i].text, "PokeRogue")) continue;
        CHECK(g_text[i].x + g_text[i].w <= g_win[0].x + g_win[0].w);
    }
    CHECK(has("..") );
    /* the longest name of the table, at the hour it closes first: its title is cut in a wide font, whole in a narrow one */
    eh_use_fixed_clock(T(20773, 16, 30, 0));
    g_char_w = 6.0f;
    draw(0);
    CHECK(has("Lucky Species Hour is on"));
    g_char_w = 12.0f;
    draw(0);
    CHECK(!has("Lucky Species Hour is on") && has("Lucky Species H"));
    for (int i = 0; i < n_text; i++) {
        if (!strcmp(g_text[i].text, "PokeRogue")) continue;
        CHECK(g_text[i].x + g_text[i].w <= g_win[0].x + g_win[0].w);
    }
    g_char_w = 6.0f;

    /* a clock that cannot be read: said on both screens, nothing else */
    eh_use_fixed_clock(-1);
    draw(0);
    CHECK(has("No event today") && has("The console clock cannot be read."));
    draw(1);
    CHECK(find("EVENTS") != NULL && has("The console clock cannot be read."));
    CHECK(!has("ON NOW"));
    check_panel_geometry();

    /* the option list is still drawn on the bottom screen, after the panel (the panel is not on top of it) and the title still has its entries */
    eh_use_fixed_clock(T(20773, 12, 0, 0));
    draw(1);
    CHECK(g_top != NULL);

    eh_use_live_clock();
    printf("title_events: %d checks, 0 failures\n", checks);
    return 0;
}
