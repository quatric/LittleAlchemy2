#!/usr/bin/env python3
"""Download the Little Alchemy 2 music and sound effects into apps/LittleAlchemy2/audio/.

The audio is Little Alchemy 2's own, so it is not part of this repository: run this once
and copy apps/LittleAlchemy2/audio/ to the SD card. The game plays the Ogg Vorbis files
as they are; without them it falls back to its own synthesized sound effects.
"""
import os, sys, urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "apps", "LittleAlchemy2", "audio")
SITE = "https://littlealchemy2.com/static/audio"
SFX = ["GenericClick", "PickUpItem", "DropItem", "AlreadyMixed", "CloneItem", "FailedToMix",
       "RemoveItem", "ChargeUp", "PopUp"]
MUSIC = ["LA_001", "LA_002", "LA_003", "LA_FS_3_4", "LA_004_Loop", "LA_005_Loop", "LA_006_Loop",
         "LA_FS_001_Loop", "LA_FS_0032_Loop", "LA_001_Loop", "LA_002_Loop", "LA_003_Loop",
         "LA_FS_3_4_Loop"]


def fetch(kind, name):
    path = os.path.join(OUT, kind, name + ".ogg")
    if os.path.exists(path):
        return True
    req = urllib.request.Request(f"{SITE}/{kind}/{name}.ogg", headers={"User-Agent": "Mozilla/5.0 (LittleAlchemy2 fetch_audio)"})
    try:
        with urllib.request.urlopen(req, timeout=60) as r:
            data = r.read()
    except OSError as e:
        print(f"fetch_audio: {kind}/{name}: {e}")
        return False
    os.makedirs(os.path.dirname(path), exist_ok=True)
    open(path + ".part", "wb").write(data)
    os.replace(path + ".part", path)
    return True


def main():
    ok = sum(fetch("sfx", n) for n in SFX) + sum(fetch("music", n) for n in MUSIC)
    print(f"fetch_audio: {ok}/{len(SFX) + len(MUSIC)} files in {os.path.relpath(OUT, os.path.join(HERE, '..'))}")
    if ok < len(SFX) + len(MUSIC):
        sys.exit(1)


if __name__ == "__main__":
    main()
