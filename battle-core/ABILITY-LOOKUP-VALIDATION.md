# Indexed ability lookup

Target: upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.

`Pokemon.getAbility` distinguishes temporary summon overrides from permanent
ability indices, custom abilities, side overrides, fusion abilities and daily
boss inputs. In particular, temporary `NONE` is ignored while custom `NONE` is
an actual ability. Ignoring temporary overrides also ignores temporary species
and fusion forms; it still honors the scene's side override. Ordinary missing
form abilities fall back to the raw species' first ability. Fusion abilities
have no such fallback.

The C core now accepts these inputs explicitly and maintains its existing
concrete active/permanent caches at the temporary-ability and summon-reset
boundaries. New game-core Pokemon supply their existing ability index;
evolution and reward form changes keep the bridge index synchronized.
The owning caller supplies applicable scene/daily overrides. This does not
implement a missing daily-event controller or new custom-content behavior.

`tests/ability-lookup.producer.test.ts` configures actual source Pokemon and
calls the unchanged getter for both `ignoreOverride` values. It compares
8,000 randomized cases (4,000 each at seeds 777 and 2034), covering all 32 input
scenarios on both sides, species/forms, ordinary and fusion indices, temporary
forms, precedence, positive/zero overrides and actual daily boss predicates.
Neither source RNG stream changes during a query. The strict C reader also
checks that querying leaves its actor unchanged and refreshing produces the
same pair of values. This is getter/API proof, not complete battle turns.

Fourteen compiled production-code mutations are caught: temporary/global
precedence and ignore semantics, custom zero semantics, temporary fusion/form
handling, secondary/hidden selection, daily override/zero and raw-species
fallback. Fourteen malformed transports are rejected, including truncated,
extra or missing cases, wrong IDs/header/footer, invalid identity/index and
invalid ability IDs. The comparison does not tolerate unsupported queries.

Game-core's command/save integration has 44 checks for custom zero, copied
positive abilities, both fusion identities, scene/daily metadata, reset and
invalid records. All eight new serialization fields are required together.
Historical saves without them retain their concrete ability caches; ambiguous
historical indices/custom inputs are not inferred. Transformed `NONE` still
reports unsupported when these lookup inputs are unavailable. Save support
uses named additive fields without changing historical field widths.

Reproduce against a clean pinned checkout:

```
battle-core/tools/ability-lookup-diff.sh /path/to/upstream 4000 777
battle-core/tools/ability-lookup-diff.sh /path/to/upstream 4000 2034
make -C game-core ability-lookup-host-test
```

Broader form-change lifecycle, Sketch/copy effects, global suppression leave
callbacks and doubles remain separate work. A getter matrix does not establish
complete battle-core, rendering, emulator or hardware parity.
