"""Real diagnostic replay pairing and bounded CLI/step-limit contracts."""
import subprocess
import sys

binary = sys.argv[1]
cases = int(sys.argv[2]) if len(sys.argv) > 2 else 2
assert 1 <= cases <= 1000
def run(*args):
    return subprocess.run([binary, *args], text=True, capture_output=True, timeout=90)

def gameplay_lines(output):
    # Worker completion order varies; per-seed comparisons prevent XOR cancellation.
    return sorted(line for line in output.splitlines()
                  if line.startswith(("run ", "diagnostic ", "stub ", "run-core ", "unported "))
                  and not line.startswith("diagnostic save/load:"))

normal = run("--runs", str(cases), "--jobs", "8")
saved = run("--runs", str(cases), "--jobs", "8", "--save-test")
assert normal.returncode == saved.returncode and normal.returncode in (0, 1), (normal.stderr, saved.stderr)
assert "crashed=0 timeout=0" in normal.stdout and "crashed=0 timeout=0" in saved.stdout
assert gameplay_lines(normal.stdout) == gameplay_lines(saved.stdout), "diagnostic gameplay changed across reload"
assert f"reached100={cases}" in normal.stdout
assert "roundtrips=" in saved.stdout
limited = run("--runs", "1", "--steps", "1")
assert limited.returncode == 1 and "reason=step-limit" in limited.stdout
for args in (("--runs",), ("--runs", "0"), ("--jobs", "0"), ("--first", "-1"),
             ("--timeout", "0"), ("--steps", "0"), ("--runs", "nan"),
             ("--runs", "2147483648"), ("--first", "1000000"), ("--unknown",)):
    assert run(*args).returncode == 2, args
print("Endgame diagnostic: paired actual replay, step-limit and 10 CLI guards passed")
