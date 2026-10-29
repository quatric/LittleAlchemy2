#!/usr/bin/env python3
"""Turn a binary file into a C header: `const unsigned char <name>[]` and `<name>_size`.

usage: bin2c.py <input> <name> <output.h>

The Wii build gets font_ttf.h and logo_rgba.h from devkitPro's bin2o; the other ports use this."""
import sys

src, name, out = sys.argv[1:4]
data = open(src, "rb").read()
with open(out, "w") as f:
    f.write("#ifndef %s_H\n#define %s_H\n" % (name.upper(), name.upper()))
    f.write("static const unsigned char %s[%d] __attribute__((aligned(4))) = {\n" % (name, len(data)))
    for i in range(0, len(data), 24):
        f.write(",".join(str(b) for b in data[i:i + 24]) + ",\n")
    f.write("};\nstatic const unsigned %s_size = %d;\n#endif\n" % (name, len(data)))
