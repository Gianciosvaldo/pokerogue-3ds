#!/bin/sh
# apt_install.sh may not be able to hang a job: a stalled apt-get is stopped and tried again, a failing one is tried again, the
# packages reach apt-get untouched, and after three tries it gives up with an error. apt-get, sudo and dpkg are stand-ins that
# follow a plan (one word per apt-get call: ok, fail or hang).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
mkdir "$T/bin"
printf '#!/bin/sh\nexec "$@"\n' > "$T/bin/sudo"
printf '#!/bin/sh\nexit 0\n' > "$T/bin/dpkg"
cat > "$T/bin/apt-get" <<'EOF'
#!/bin/sh
n=$(cat "$APT_STUB_DIR/calls" 2>/dev/null || echo 0)
n=$((n + 1))
echo "$n" > "$APT_STUB_DIR/calls"
echo "$*" >> "$APT_STUB_DIR/log"
case $(sed -n "${n}p" "$APT_STUB_DIR/plan") in
  fail) exit 100 ;;
  hang) exec sleep 60 ;;
esac
exit 0
EOF
chmod +x "$T/bin/sudo" "$T/bin/dpkg" "$T/bin/apt-get"

# run PLAN PACKAGE...: sets RC (exit status), CALLS (apt-get calls made) and SECONDS_TAKEN
run() {
  printf '%s\n' $1 > "$T/plan"
  shift
  rm -f "$T/calls" "$T/log"
  start=$(date +%s)
  RC=0
  PATH="$T/bin:$PATH" APT_STUB_DIR="$T" APT_UPDATE_TIMEOUT=2 APT_INSTALL_TIMEOUT=2 APT_RETRY_PAUSE=0 \
    sh "$HERE/apt_install.sh" "$@" > "$T/out" 2>&1 || RC=$?
  SECONDS_TAKEN=$(( $(date +%s) - start ))
  CALLS=$(cat "$T/calls" 2>/dev/null || echo 0)
}
check() { # DESCRIPTION CONDITION...
  what=$1
  shift
  if "$@"; then echo "ok: $what"; else echo "FAILED: $what" >&2; cat "$T/out" >&2; exit 1; fi
}

run "ok ok" mingw-w64 zip
check "a good apt-get installs once: exit 0, two calls (update, install)" test "$RC" -eq 0 -a "$CALLS" -eq 2
check "the packages and -y reach apt-get untouched" grep -q -- "-y -qq mingw-w64 zip$" "$T/log"

run "fail ok ok" ffmpeg
check "a failing update is tried again and then installs" test "$RC" -eq 0 -a "$CALLS" -eq 3

run "ok fail ok ok" ffmpeg
check "a failing install is tried again from the update" test "$RC" -eq 0 -a "$CALLS" -eq 4

run "hang ok ok" ffmpeg
check "a stalled update is stopped (not waited for) and the second try installs" test "$RC" -eq 0 -a "$CALLS" -eq 3 -a "$SECONDS_TAKEN" -lt 30

run "ok hang ok ok" ffmpeg
check "a stalled install is stopped and the second try installs" test "$RC" -eq 0 -a "$CALLS" -eq 4 -a "$SECONDS_TAKEN" -lt 30

run "hang hang hang" ffmpeg
check "three stalls end in an error after three tries, not in a hang" test "$RC" -eq 1 -a "$CALLS" -eq 3 -a "$SECONDS_TAKEN" -lt 40
check "the error says so" grep -q "did not finish in 3 tries" "$T/out"

run "ok"
check "no packages is a usage error, not an apt-get run" test "$RC" -eq 2 -a "$CALLS" -eq 0
echo "apt_install: all cases passed"
