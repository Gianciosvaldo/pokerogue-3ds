# Fusion ability power queries

Pinned source: `8555c08c823b856cbec4eb99ca84ea52a955836d`.

An active Liquid Voice ability changes sound moves to Water before a Water
Bubble passive tests their power. The old move-generation table queried alternate
types only for moves with `VariableMoveTypeAttr`. Disarming Voice has no such
attribute: its estimated power was 40 instead of native 80. The focused native
capture exposed 13 power differences and two generated moveset differences.

`gen/dump-movegen.test.ts` now observes all 18 resolved types for every damaging
move. The runtime consults the resolved-type row for ordinary moves too. It also
consults rows for the original type: a non-variable move's baseline observation
already includes its own active ability's usual type resolution.

The baseline dumper reproduced the previous generated file byte-for-byte. The
expanded native output preserves all 9,131 old rows and every other generated
table, adding 8,839 type rows. The existing assertion rejecting multiple
independent dimensions for one ability/move remains enabled and passes. Generator
STAB predictions retain upstream's separate behavior; ability type changes do not
alter that prediction.

Baseline generated-file SHA-256:
`177cd399d2360af8c76867b6b6b2344e31e5b76a95c896e3c1d57c49f7f83aad`.
Expanded generated-file SHA-256:
`c35b168722ca0785c11f7f96c3d4111c474dc241f0bff41c1ab65da6c2d52227`.
Both native captures, the expanded table and transported C fixture reproduced
byte-for-byte in a fresh Node 22 run.

`fusion-power-focused.producer.test.ts` records eight prepared compatible fusion
snapshots: six Liquid Voice/Water Bubble pairs and two ability controls.
`fusion-power-constructors.producer.test.ts` records 804 actual Spliced boss
constructors, covering 17 primary species, both battle formats, levels 100,
65,536 and 70,000, and Ability Charm contexts. Each captures native
`Move.calculateEffectivePower` with `movesetGenInProgress` enabled and verifies
that these queries do not consume RNG. Native battle RNG is captured before
constructing `GameManager` and restored after battle startup.

The strict transport validates all 812 inputs, complete ordered pools, implemented
move selections and observed powers. The host test compares the public
`gc_mg_effective_powers` query with 27,118 native power observations, checking
stats, active/passive identities, input immutability and caller RNG. It retains
challenge-constructor rejection and its atomicity. Successful fused-boss
generation for every one of these inputs has a [separate owner proof](FUSED-BOSS-OWNER-VALIDATION.md).
Passing these bounded queries does not establish full battle-core completion.

Checks:

```sh
make -C game-core fusion-power-host-test
python3 game-core/tools/check-fusion-power-proof.py
FUSION_POWER_SANITIZE=1 python3 game-core/tools/check-fusion-power-proof.py
```

The proof checker requires seven compiled faults, twelve malformed captures and
three forged native observations to fail. Scoped ASan/UBSan/LSan covers
`gc_movegen.c`; supporting libraries are unsanitized. Retained ordinary-wild,
unfused-boss, fusion-type and historical adapter tests remain enabled alongside
normal app/custom/save tests and the ARM build.
