#!/bin/sh
# apt-get install for the builder workflow's runners, which cannot hang the job. Twice the step "sudo apt-get update -qq &&
# sudo apt-get install -y -qq ..." printed nothing for the whole 30-minute (once over an hour) job limit and the run was lost.
# Every apt-get here is stopped after a while and the whole thing tried again, and each stage says what it is doing, so a
# stall shows in the log as the last line instead of as silence.
#
#   sd-builder/tests/apt_install.sh PACKAGE...
#
# APT_UPDATE_TIMEOUT / APT_INSTALL_TIMEOUT (seconds) and APT_RETRY_PAUSE are for the test, apt_install_test.sh.
set -u
[ $# -gt 0 ] || { echo "usage: apt_install.sh PACKAGE..." >&2; exit 2; }
UPDATE_TIMEOUT=${APT_UPDATE_TIMEOUT:-90}
INSTALL_TIMEOUT=${APT_INSTALL_TIMEOUT:-300}
PAUSE=${APT_RETRY_PAUSE:-10}
APT="-o Acquire::Retries=3 -o Acquire::http::Timeout=30 -o Acquire::https::Timeout=30 -o DPkg::Lock::Timeout=60"
now() { date -u +%H:%M:%S; }
for try in 1 2 3; do
  echo "$(now) apt-get update (try $try of 3, stopped after ${UPDATE_TIMEOUT}s)"
  # shellcheck disable=SC2086
  if sudo timeout -k 10 "$UPDATE_TIMEOUT" apt-get $APT update -qq; then
    echo "$(now) apt-get install $* (try $try of 3, stopped after ${INSTALL_TIMEOUT}s)"
    # shellcheck disable=SC2086
    if sudo timeout -k 10 "$INSTALL_TIMEOUT" apt-get $APT install -y -qq "$@"; then
      echo "$(now) installed: $*"
      exit 0
    fi
  fi
  echo "$(now) apt try $try of 3 failed or was stopped" >&2
  sudo dpkg --configure -a || true
  sleep "$PAUSE"
done
echo "apt-get did not finish in 3 tries" >&2
exit 1
