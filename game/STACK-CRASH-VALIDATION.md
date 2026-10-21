# ARM11 battle-command stack crash

The reported console crash has PC `0x001694dc`, LR `0x00159498`, and a write
fault at `0x07fff8ac`. The photographed stack pointer is `0x07fff8b8`:
the faulting instruction pushes 12 bytes below it.

## Exact build diagnosis

Retrieve SD-builder artifact **12232174254**, workflow **39350450476**, from
commit `a4d6b3deb98fe22c9c46877cf100ec38e68e8622` (PR #337). Its stripped-debug
ELF retains symbols. Rebuilding that commit with the cached devkitARM image gives
byte-identical `.text`, `.rodata` and `.data` sections. In particular:

- `0x001694dc` is the first instruction in `pr_msg_p`, `push {r4,r5,lr}`.
- Its caller at `0x00159494` is `battler_tags.c:on_add`, through `say`.
- The key at the photographed `R2=0x00305b08` is `battlerTags:tauntOnAdd`.
- The builder inherited libctru's weak `__stacksize__ = 0x8000` (32 KiB).
- ARM `-fstack-usage` reports 25,896 bytes in `pr_apply_move_effect`, 4,168
  in the nested `on_add`, and 1,504 in `move_effect_phase`, before the rest of
  the caller chain and UI/library frames. Other individual core frames reach
  45,056 bytes, already exceeding that default by themselves.

This locates the failure during enemy Taunt execution after a command, explaining
why it can follow either a selected move or a Poké Ball. It is not evidence that
every possible console abort has the same cause.

## Fix and regression

`main.c` now supplies libctru's strong startup override using the shared
`POKEROGUE_MAIN_STACK_BYTES` limit: **512 KiB**. This changes startup allocation,
not battle mechanics, save layout or menu behavior. The extra reserved stack
replaces reliance on the 32 KiB default.

The normal game-core host suite includes a real command regression with a
manually allocated stack between inaccessible guard pages. It submits eight
Fight and eight Ball commands through `gc_run_choose`, reaching enemy Taunt
in all sixteen fixtures. At 512 KiB, all **228 checks pass**; host high-water
usage is approximately 45.8 KiB. Running the same executable with the old
32,768-byte allocation terminates with **SIGSEGV**. Host high-water measurements
are not ARM runtime measurements.

`check_stack_size.py` inspects the actual built ARM ELF, requires the app's strong
startup override with at least 512 KiB, and checks compiler frame reports. The
fixed build passes with **524,288 bytes**, **2,245 ARM function reports**, and
largest individual frame **45,056 bytes**. The original builder is rejected.
An independent counterfactual ELF retaining a strong symbol but restoring its
value to 32 KiB is also rejected.

CI runs the built-ELF check after the normal 3DS build. The Windows-builder job
checks both the full and stripped bundled ELF, so packaging cannot silently
reintroduce the weak default. ARM stack reports are generated with the build.
These checks cover the reported path and measured frames, not every possible
nested runtime call chain. Physical-console confirmation still requires testing
the refreshed builder output.

## Reproduce

```sh
make -C game-core command-stack-host-test
# Diagnostic counterfactual: expected SIGSEGV; disable core dumps in the runner.
game-core/build/command_stack_host_test 32768
python3 game/tests/check_stack_size.py game/pokerogue.elf --usage-dir game/build
```

Also retain normal game-core, full app/custom/save sanitizer tests and the 3DS
build. Only the separate custom-only Task63 job remains excluded.
