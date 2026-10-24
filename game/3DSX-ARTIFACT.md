# PokeRogue 3DSX download

Each successful PR CI run publishes a **PokeRogue-3DSX** artifact containing
`pokerogue.3dsx`, its Homebrew Launcher icon, and this note. Open the PR's Checks,
choose the CI run, and download it under Artifacts. The run's commit identifies
which version you are testing.

This is the plain app. For an existing installation with the asset packs already
on the SD card, replace the installed app with this `.3dsx` (the SD builder names
it `3ds/PokeRogue.3dsx`). The existing `3ds/pokerogue/*.pak` files are still needed.
The icon is optional and can be placed alongside the app with the same basename.

For a fresh card, download **PokeRogue-SD-Builder** from the separate **SD card
builder on Windows** workflow and run its `BUILD-SD-CARD.bat`. That package builds
the packs and can produce either a folder installation or a self-contained
`.3dsx` / `.cia`. The standalone CI app artifact does not contain those packs.
