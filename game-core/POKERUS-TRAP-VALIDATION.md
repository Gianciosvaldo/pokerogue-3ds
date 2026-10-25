# Pokérus party lifecycle and trapping refusal text

Target: upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node22.

`gc_battle_player_command` now asks the actual battle-core `pr_can_switch`
owner for its structured refusal. The application formats all message arguments,
including the opposing Pokémon and its trapping ability, rather than passing a
bare `noEscapePokemon` key with missing move/verb arguments. Run-command reasons
also retain their existing structured arguments. Refusals clear at the next
choice and are transient, rather than serialized run state.

Pokérus starters and the 1.5x EXP multiplier already existed. The party/summary
and starter-description adapters omitted the infection flag. They now expose it,
and the starter panel explicitly shows PKRS beside the dex number. Between-wave
spreading now iterates the current party order, matching `trySpreadPokerus`,
rather than the original storage order. The requested custom rotation remains
three species per three hours; the native pin selects five per day.

The additive native producer captures `BattleScene.prototype.randBattleSeedInt`
before GameManager construction and restores it after natural battle startup.
It calls real `trySpreadPokerus` for all six three-member party permutations,
eight infection masks and eight wave indices: 384 cases. The C comparator invokes
`gc_new_battle`, checks carriers by persistent Pokémon identity, and saves/loads
each result. Forty-five reordered cases infect another member. Four actual
`CommandPhase.tryLeaveField` cases cover Arena Trap and Shadow Tag, plus Ghost
escape for each. Exact rendered refusal text is compared with native English
resources captured in the fixture. Rejected commands preserve RNG and the next
unrelated refusal cannot reuse the old message.

Frontend checks cover grid/description/selected-starter consistency across 64
three-hour epochs and infection reaching real new runs, and both party-summary
adapter paths. These are bounded ownership and display checks, not a completion
claim for every trapping interaction or all battle-core behavior.

Reproduce:

```sh
. /workspace/shared/activate-parity.sh # or activate your Node22 installation
game-core/tools/native-pokerus-trap.sh /path/to/pinned-upstream
make -C game-core pokerus-trap-test
```

Hatch presentation also uses separate stat columns, larger IV numbers with
16-pixel line spacing, and opaque foreground/background colors. Geometry/color
checks complement, and do not replace, a physical 3DS readability test.
No SD-card builder is produced by this change.
