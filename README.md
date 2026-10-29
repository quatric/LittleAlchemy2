# Little Alchemy 2 for Wii, Wii U, Switch and 3DS

**Version:** 2.5.0 · **Coder:** quatric

An element-combining puzzle game for the Wii Homebrew Channel. It uses the same
elements and recipes as [Little Alchemy 2](https://littlealchemy2.com/): start with
water, fire, earth and air and combine your way to all 720 elements.

`LittleAlchemy2-hbc.zip` includes the icon pack (`apps/LittleAlchemy2/icons.bin`), so the game
shows Little Alchemy 2's own icons. You can rebuild it with `python3 tools/fetch_icons.py`
(needs Google Chrome and ImageMagick). Without it the game draws its own icons.

The zip also includes the audio pack (`apps/LittleAlchemy2/audio/`), so the game plays Little Alchemy 2's own music and sound effects, with the music
picking calm or busy tracks from how fast you are discovering, like the original. Rebuild it
with `python3 tools/fetch_audio.py`. Without
it the game plays simple built-in sound effects and no music. Music and sound effects can
each be turned off in the menu (+); the choice is saved with your progress.

Like Little Alchemy 2, a few elements are unlocked by progress instead of being made:
metal (50 discoveries), time (100), big, science and philosophy (150), container (300),
and light, motion and small once you have found 5 related elements.

## Download

Grab the latest build from the [latest release](../../releases/latest): `LittleAlchemy2-hbc.zip`. Every push to the main branch refreshes it, and tagged versions get their own entry on the [releases page](../../releases).

1. Extract `LittleAlchemy2-hbc.zip` to the root of your SD card (or USB drive).
2. Open Little Alchemy 2 from the Homebrew Channel.

The zip includes the icon pack and the music and sound effects.

## Other consoles

The same game also runs on the Switch, the Wii U and the 3DS. Each release has an SD card zip per console
(`LittleAlchemy2-switch.zip`, `LittleAlchemy2-wiiu.zip`, `LittleAlchemy2-3ds.zip`); extract it to the root
of the SD card. They include the icon pack and the music and sound effects.

| Console | Folder on the SD card | Launch from | Pointer |
|---|---|---|---|
| Switch | `switch/LittleAlchemy2/` | hbmenu (`.nro`) | Touch screen, Joy-Con / Pro Controller stick, USB mouse |
| Wii U | `wiiu/apps/LittleAlchemy2/` | Aroma (`.wuhb`) or the Homebrew Launcher (`.rpx`) | GamePad touch screen, sticks, Pro Controller |
| 3DS | `3ds/LittleAlchemy2/` | Homebrew Launcher (`.3dsx`) | Touch screen, circle pad |

Buttons: A = drag (touching the screen counts as A), B = remove, D-pad = scroll, + / R = menu,
− / L = clear the table, Y = hint, X = hide finished elements. On the 3DS the game is shown on the touch
screen at half size and mirrored on the top screen.

Build them with `make -C ports/switch`, `make -C ports/wiiu` or `make -C ports/3ds` (devkitPro with
libnx + `switch-sdl2`, `wut` + `wiiu-sdl2`, or libctru), then `scripts/package_port.sh switch|wiiu|3ds`.
`make -C ports/host run` builds a desktop SDL2 version for quick testing.

## Install
Copy `apps/LittleAlchemy2/` to `sd:/apps/LittleAlchemy2/` and launch it from the Homebrew Channel
(or unzip `LittleAlchemy2-hbc.zip` onto the root of the SD card). Progress is saved to
`sd:/apps/LittleAlchemy2/LittleAlchemy2.sav` (saves from `sd:/apps/alchemii/` are picked up).

## Controls
| Wii Remote | Classic / Wii U Pro / GamePad | GameCube | DualShock 3 | Action |
|---|---|---|---|---|
| Pointer | Left stick | Control stick | Left stick | Move the cursor |
| A (hold) | A | A | Cross | Drag an element; drop it on another to combine |
| A A | A A | A A | Cross Cross | Double-press to copy an element |
| B | B | B | Circle | Remove the element under the pointer |
| D-pad | D-pad | D-pad | D-pad | Scroll (↑↓) / page (←→) the element list |
| − | − / L / ZL | X / L | Select / L1 / L2 | Clear the table |
| 1 | Y | Z | Square | Hint |
| 2 | X | Y | Triangle | Hide elements with nothing left to make |
| + | + / R / ZR | Start / R | Start / R1 / R2 | Menu (music, sound effects, reset progress, exit) |
| HOME | HOME | — | PS | Exit |

A Nunchuk's stick also moves the cursor (Z = A, C = B).

**USB mouse:** move to point, left button = A, right button = B, middle button = menu,
wheel scrolls the element list.
**USB keyboard:** arrows = D-pad, Enter / Space = A, Backspace / Delete = B, H or X = hint,
Y = hide finished, + or M = menu, − or C = clear, Esc = exit.

Drag an element back onto the list to remove it. Clean up, hide done, hints and settings
are also on the tool strip down the left edge — point at one and press A.

The Wii U GamePad works under Wii U vWii with a patched fw.img; the Wii U Pro Controller
pairs over Bluetooth like a Wii Remote; the DualShock 3 works over USB.

## Build
Requires devkitPPC + libogc (devkitPro), Tremor (`dkp-pacman -S ppc-libvorbisidec ppc-libogg`)
and Python 3.
```
./scripts/build_controller_portlibs.sh   # optional: Wii U GamePad, Wii U Pro, DualShock 3
make package        # builds LittleAlchemy2.dol and LittleAlchemy2-xz.dol, copies the latter to apps/LittleAlchemy2/boot.dol
make run            # sends it to a Wii with wiiload
```
`make` also writes `LittleAlchemy2-xz.dol`, the same program packed into a self-extracting `.dol` a fraction of the size; `make package` uses it as `boot.dol`. The first build downloads and builds [dolxz](https://github.com/FIX94/dolxz), which needs a host C compiler and liblzma (Debian/Ubuntu: `apt-get install liblzma-dev pkg-config`; macOS: `brew install xz`). `make NO_DOLXZ=1` skips this and uses the plain `.dol`.

Elements, recipes and unlock rules live in `tools/elements.txt`; `tools/gen_elements.py`
validates that every element is reachable and regenerates `source/elements.h`.

`tools/host/host.c` is a desktop harness that runs the game with scripted input and
writes screenshots (`cc -O2 -o host tools/host/host.c source/{game,gfx,icons}.c -lm`;
`host out` plays a short session, `host out unlock` tests a progress unlock and
`host out icons` draws every element icon).
