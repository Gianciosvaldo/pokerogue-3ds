#!/bin/sh
cd /home/claude/pokerogue && npx vite build -c test/bench-3ds/vite.bench.config.ts > /tmp/vite.log 2>&1 || { tail -30 /tmp/vite.log; exit 1; }
ls -la test/bench-3ds/dist/bench.mjs
