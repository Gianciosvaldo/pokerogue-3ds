# Keep the controlled kick returner visible

football_live_focus selected the flying ball before checking for a receiving
kick. The camera therefore left the controlled returner as soon as the ball
launched. Punt reception also focused on the opposing kicker during the setup.

Give the controlled human returner precedence for kickoffs and punts throughout
setup, flight, bounce and return. Dead-ball results retain their ball focus.
The existing screen uses this helper for both its pre-play framing pause and
camera updates; no new controls or camera speed changes are needed.

The added return_camera_test fails on the old focus selection. After the fix it
plays 80 kickoffs and 80 punts through the actual football engine and camera:
34,540 flight frames and 55,531 return frames keep the controlled player inside
the screen bounds while steering to the landing spot. It also retains kicking,
CPU-only, pass-flight, field-goal and dead-ball focus checks.

The same regression passes with ASan/UBSan and leak detection. Retained special
teams, kickoffs/tries and live/full-match/camera tests pass. The new regression
runs in normal football tests and the existing sanitized football CI job.
Normal repository and actual 3DS/ARM stack CI remain required. No Windows builder.
The actual screen-controller host test also passes its held movement, playcall,
pause, save/resume and failed-write checks with the updated focus selection.
