#!/bin/sh
# Real source attribute and independently replayed ordered held-item fixtures.
set -eu
UP=${1:?upstream checkout}; UP=$(CDPATH= cd -- "$UP" && pwd)
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
case "$(node --version)" in v22.*) ;; *) echo 'Node22 required' >&2; exit 2 ;; esac
make -s -C "$HERE" build/poltergeist-message-test
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/poltergeist-message.producer.test.ts" "$UP/test/tests/zz-dump/"
EVIDENCE=$(mktemp -d "$HERE/build/poltergeist-message-evidence.XXXXXX")
echo "Poltergeist evidence: $EVIDENCE"
(cd "$UP" && POLTERGEIST_OUT="$EVIDENCE/messages.txt" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/poltergeist-message.producer.test.ts --maxWorkers=1) > "$EVIDENCE/source.log" 2>&1
TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/poltergeist-message-test" "$EVIDENCE/messages.txt" | tee "$EVIDENCE/replay.log"
