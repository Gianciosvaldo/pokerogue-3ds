#!/usr/bin/env python3
"""The builder skips a pack only when its marker equals recipe(). The recipe must change when an extra picture (the JoJo
sprites), a pipeline script, the encoder or a pin changes, or an update over an old folder would keep stale packs.

    python3 sd-builder/tests/recipe_test.py

Copies the builder, its pins, the pipeline scripts and the extra pictures into a temporary folder shaped like the zip's
files/ folder and imports builder.py from there (it opens build-log.txt next to the folder it is in). Standard library only.
"""
import importlib
import json
import os
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SD = os.path.dirname(HERE)
REPO = os.path.dirname(SD)


def main():
    with tempfile.TemporaryDirectory() as t:
        files = os.path.join(t, "PokeRogue-SD-Builder", "files")
        os.makedirs(os.path.join(files, "pipeline", "tools"))
        for f in ("builder.py", "bundle.py", "pins.json"):
            shutil.copy(os.path.join(SD, f), files)
        for f in ("build_sd.py", "gfx.py", "audio.py", "font.py", "rpak.py"):
            shutil.copy(os.path.join(REPO, "assets-pipeline", f), os.path.join(files, "pipeline"))
        shutil.copy(os.path.join(REPO, "assets-pipeline", "tools", "dspadpcm.c"), os.path.join(files, "pipeline", "tools"))
        shutil.copytree(os.path.join(SD, "extra-images"), os.path.join(files, "extra-images"))
        sys.path.insert(0, files)
        builder = importlib.import_module("builder")
        pins = builder.PINS
        base = builder.recipe()
        assert builder.recipe() == base, "recipe() is not stable"

        def changes(what, action):
            before = builder.recipe()
            undo = action()
            after = builder.recipe()
            undo()
            assert after != before, f"recipe() ignores {what}"
            assert builder.recipe() == before, f"recipe() did not come back after undoing {what}"

        pics = os.path.join(files, "extra-images", "trainer", "custom")
        one = os.path.join(pics, sorted(os.listdir(pics))[0])

        def edit_picture():
            data = open(one, "rb").read()
            open(one, "wb").write(data + b"\0")
            return lambda: open(one, "wb").write(data)

        def add_picture():
            new = os.path.join(pics, "zzz-new.png")
            shutil.copy(one, new)
            return lambda: os.remove(new)

        def remove_picture():
            data = open(one, "rb").read()
            os.remove(one)
            return lambda: open(one, "wb").write(data)

        def edit_file(rel):
            path = os.path.join(files, "pipeline", *rel.split("/"))

            def go():
                data = open(path, "rb").read()
                open(path, "wb").write(data + b"\n# changed\n")
                return lambda: open(path, "wb").write(data)
            return go

        def edit_pin(name):
            def go():
                old = json.dumps(pins[name])
                pins[name] = dict(pins[name], commit="0" * 40)
                return lambda: pins.__setitem__(name, json.loads(old))
            return go

        changes("an edited extra picture", edit_picture)
        changes("a new extra picture", add_picture)
        changes("a removed extra picture", remove_picture)
        for rel in ("gfx.py", "audio.py", "font.py", "rpak.py", "build_sd.py", "tools/dspadpcm.c"):
            changes(rel, edit_file(rel))
        for name in ("assets", "locales", "game"):
            changes(f"the {name} pin", edit_pin(name))

        # packs made by the old recipe are all pending, packs made by this one are not
        os.makedirs(builder.PACK_DIR)
        for s in pins["packs"]:
            open(os.path.join(builder.PACK_DIR, f"{s}.pak"), "wb").write(b"x")
            open(os.path.join(builder.PACK_DIR, f".{s}.done"), "w").write("an older recipe")
        assert builder.pending_stages() == pins["packs"], builder.pending_stages()
        for s in pins["packs"]:
            open(os.path.join(builder.PACK_DIR, f".{s}.done"), "w").write(builder.recipe())
        assert builder.pending_stages() == [], builder.pending_stages()
        open(os.path.join(builder.PACK_DIR, ".gfx.done"), "w").write("an older recipe")
        assert builder.pending_stages() == ["gfx"], builder.pending_stages()
        builder._log.close()
    print("recipe_test: OK")


if __name__ == "__main__":
    main()
