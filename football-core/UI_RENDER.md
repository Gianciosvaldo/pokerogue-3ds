# Playcalling and dual-screen rendering

`football-core/src/playcall_ui.c` owns a portable, testable menu state machine:
Main (Offense) -> Formation (Shotgun / Under Center) -> Play Type (Run / Pass).
Touch buttons use physical bottom-screen coordinates, with matching draw/hit
rectangles. D-pad and A select; B goes back. Only KEY_TOUCH down edges activate,
so holding or dragging the stylus cannot accidentally select successive menus.
`football_playcall_take` delivers one tagged `struct PlayInfo`, then resets the
menu. The four initial play IDs are Inside Zone, Slants, Dive and Play Action.
They describe engine requests, not simulated results. Target slots 1/2 are hints;
the engine must resolve an eligible player for a short drafted roster.

On 3DS call `hidScanInput` once per app frame, then `football_playcall_poll`.
The latter explicitly uses `hidKeysDown`, `touchPosition`, and `hidTouchRead`.
Do not poll playcalling while the engine is executing a play or another menu
owns the bottom screen.

`game/source/football_render.c` draws the 400x240 top and 320x240 bottom screens.
The top-down field covers 120 x 53 1/3 yards, including end zones, with yard lines,
hash marks and a line-of-scrimmage marker drawn with `C2D_DrawRectSolid`. Score,
down/distance, quarter and time come from `FootballRenderScene`; the engine owns
the clock. Player coordinates are in yards. Non-finite positions are skipped,
coordinates and actor counts are bounded, and missing assets get team markers.
The bottom screen draws the same rectangles used for playcalling hit tests.

Allocate one zero-initialized `FootballRenderer` statically or on the heap, and
call `football_render_init` after the existing `rr_init`. The renderer owns a
512-glyph text buffer and at most 12 front-sprite cache entries (six per side).
Load entries with `football_render_load_front` only between matches or after an
evolution, then set each actor's borrowed `front_frame` from
`football_render_front_frame(renderer, slot, animation_ms)`. The loader always
uses `menu_sprite_load(..., false)`, reusing the existing front/adversary art,
form/gender variants and pack lookup. No directional or back assets are added.

Cached sprite GPU storage is limited to 4 MiB total; a load exceeding the retained
budget is immediately released. The existing atlas loader can temporarily allocate
a new atlas before its size is known, and can fail under memory pressure; a failed
slot remains empty and renders a marker. This budget excludes fonts, application
textures, transient loader storage and sprite metadata. It is not a claim that the
whole application fits within a 4 MiB graphics budget. Retain no borrowed frame
across a cache reload or renderer free. Free after the last submitted frame has
completed, before `rr_exit`.

The draw wrapper constructs `C2D_DrawParams` with negative X scale for left-facing
actors. PICA200 culls negative-width geometry, so before submission it converts
the signed rectangle into an equivalent positive-width rectangle with swapped U
coordinates. Trim offsets and the untrimmed center remain correct. Movement X
sets facing; stationary home actors face right and away actors face left.
`rr_frame_c2d_image` borrows the current atlas texture and frame UVs, avoiding
texture copies or extra uploads. No loading, allocation or filesystem access is
performed in the draw loop.

Use either `football_render_frame` as the frame owner, or call
`football_render_top/bottom` inside the application's existing rr frame/targets.
The complete wrapper uses `rr_frame_begin`, the existing `rr_target` for each
screen and `rr_frame_end`; it never creates duplicate GPU targets or nests frames.

Typical engine integration (illustrative control flow):

```c
hidScanInput();
if (engine_waiting_for_play) {
    football_playcall_poll(&playcall);
    PlayInfo play;
    if (football_playcall_take(&playcall, &play)) engine_execute_play(&play);
}
engine_update_scene(&scene); /* score, down, clock, positions and front frames */
football_render_frame(&renderer, &scene, &playcall);
```

Classic shop artwork remains identified by `football_shop_asset_key(offer)` and
`offer->classic_arg`. Pass those through the existing `gm_item_ui_find` lookup and
its icon into the `items` atlas, just as `game/menus/shop_ui.c` does; retain loaded
icons outside the frame loop. The season's purchase API owns currency/effects.

`make -C football-core test` includes all play paths, touch boundaries, back
navigation, single delivery, the libctru polling adapter, GPU submission recording
for trim-aware flipping, the cache budget and dual-screen frame ownership. The
recording test uses minimal SDK stubs; the normal CI 3DS build validates the real
libctru/citro2d headers. No screenshot, emulator run or hardware performance claim
is made by these host tests.

## Evolution choice modal

`evolution_ui.c` adds a separate modal to the same bottom screen. On an evolution
WAIT, fetch `gc_football_evolution_choices`, then open a `FootballEvolutionMenu`
with those targets. Opening or leaving the menu idle commits no outcome. The
player taps a species (or highlights it and presses A), or selects Cancel/B.
`football_evolution_take` returns the chosen index and cancellation flag once;
pass GC_CH_EVOLVE with that index, or GC_CH_CANCEL, to
`football_season_choose`. Draw the modal with
`football_render_evolution_bottom` in place of the playcalling bottom screen.
For Nincada the choices are explicitly Ninjask and Shedinja; both replace Nincada
in its existing slot. There is no forced Shedinja result or automatic double spawn.
Lists longer than three rows page with the D-pad. Move-learning prompts continue
to use the existing move-learning menu.

The season integration test now runs three full scenarios: choose Shedinja,
choose Ninjask, or cancel and retain Nincada. The modal tests separately verify
that neither evolution happens without an explicit player action.
