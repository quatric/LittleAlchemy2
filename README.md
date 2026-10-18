# Little Alchemy 2 for Wii

An element-combining puzzle game for the Wii Homebrew Channel. It uses the same
elements and recipes as [Little Alchemy 2](https://littlealchemy2.com/): start with
water, fire, earth and air and combine your way to all 720 elements.

With the icon pack (`apps/LittleAlchemy2/icons.bin`) on the SD card the game shows Little
Alchemy 2's own icons. The pack is Little Alchemy 2 artwork, so it is not included in
`LittleAlchemy2-hbc.zip`; make it yourself with `python3 tools/fetch_icons.py` (needs Google
Chrome and ImageMagick) and copy it next to `boot.dol`. Without it the game draws its
own icons.

Like Little Alchemy 2, a few elements are unlocked by progress instead of being made:
metal (50 discoveries), time (100), big, science and philosophy (150), container (300),
and light, motion and small once you have found 5 related elements.

## Install
Copy `apps/LittleAlchemy2/` to `sd:/apps/LittleAlchemy2/` and launch it from the Homebrew Channel
(or unzip `LittleAlchemy2-hbc.zip` onto the root of the SD card and add `icons.bin` to
`sd:/apps/LittleAlchemy2/` for the real icons). Progress is saved to
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
| + | + / R / ZR | Start / R | Start / R1 / R2 | Menu (reset progress, exit) |
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
Requires devkitPPC + libogc (devkitPro) and Python 3.
```
./scripts/build_controller_portlibs.sh   # optional: Wii U GamePad, Wii U Pro, DualShock 3
make package        # builds LittleAlchemy2.dol and copies it to apps/LittleAlchemy2/boot.dol
make run            # sends it to a Wii with wiiload
```
Elements, recipes and unlock rules live in `tools/elements.txt`; `tools/gen_elements.py`
validates that every element is reachable and regenerates `source/elements.h`.

`tools/host/host.c` is a desktop harness that runs the game with scripted input and
writes screenshots (`cc -O2 -o host tools/host/host.c source/{game,gfx,icons}.c -lm`;
`host out` plays a short session, `host out unlock` tests a progress unlock and
`host out icons` draws every element icon).
