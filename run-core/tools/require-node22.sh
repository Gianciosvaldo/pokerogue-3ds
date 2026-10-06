# Sourced by every run-core tool that launches Node (difftest.sh, challengedifftest.sh, redump.sh).
# The references here were generated under Node 22 (V8 12.4): a newer V8 rounds Math.pow differently
# (see battle-core/src/js_math.c), so the shop's money and price suites report mismatches under Node 24.
# Stop on any other major version instead of falling back to whatever `node` is first on PATH.
NODE_BIN=$(command -v node) || { echo "run-core: node not found on PATH; Node 22 is required" >&2; exit 1; }
NODE_VERSION=$("$NODE_BIN" --version) || { echo "run-core: '$NODE_BIN --version' failed; Node 22 is required" >&2; exit 1; }
case $NODE_VERSION in
  v22.*) echo "run-core: using Node $NODE_VERSION ($NODE_BIN)" >&2 ;;
  *) echo "run-core: Node 22 is required but $NODE_BIN is $NODE_VERSION; refusing to run" >&2; exit 1 ;;
esac
