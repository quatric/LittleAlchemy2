#!/usr/bin/env python3
"""Download the Little Alchemy 2 element icons and pack them into apps/alchemii/icons.bin.

The icons are Little Alchemy 2's own artwork, so they are not part of this repository:
run this once to make an icon pack for your own SD card. Without the pack the game
falls back to the icons it draws itself.

Needs Google Chrome (to rasterise the SVGs) and ImageMagick (`magick`).

icons.bin layout (all integers big-endian):
  "AL2I", u16 count, u16 small size, u16 big size, u16 0
  count x 32-byte NUL-padded element names
  count x (small*small + big*big) RGBA pixels, straight alpha
An entry whose pixels are all transparent means "no icon, draw it yourself".
"""
import base64, json, os, re, shutil, struct, subprocess, sys, tempfile, urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
SITE = "https://littlealchemy2.com"
CACHE = os.path.join(ROOT, "build", "la2-icons")
OUT = os.path.join(ROOT, "apps", "alchemii", "icons.bin")
SMALL, BIG = 52, 104
CHROME = os.environ.get("CHROME", "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome")


def get(url):
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0 (alchemii fetch_icons)"})
    with urllib.request.urlopen(req, timeout=30) as r:
        return r.read()


def la2_names():
    """id -> element name from the game's bundled element table"""
    html = get(SITE + "/").decode()
    for js in re.findall(r'src="?/?(js/chunk-vendors\.[0-9a-f]+\.js)', html):
        src = get(f"{SITE}/{js}").decode("utf-8")
        m = re.search(r"JSON\.parse\('(\{\"1\":\{.*?)'\)\}", src, re.S)
        if m:
            raw = m.group(1).encode().decode("unicode_escape") if "\\" in m.group(1) else m.group(1)
            return {k: v["n"] for k, v in json.loads(raw).items()}
    sys.exit("fetch_icons: could not find the element table on " + SITE)


def our_names():
    names = []
    for line in open(os.path.join(HERE, "elements.txt"), encoding="utf-8"):
        line = line.strip()
        if line and not line.startswith("#"):
            names.append(line.split("|")[0].strip())
    return names


def main():
    ours = our_names()
    ids = {n.lower(): k for k, n in la2_names().items()}
    os.makedirs(CACHE, exist_ok=True)
    svgs = []
    for n in ours:
        k = ids.get(n.lower())
        path = os.path.join(CACHE, f"{k}.svg")
        if k and not os.path.exists(path):
            try:
                data = get(f"{SITE}/static/icons/{k}.svg")
                open(path, "wb").write(data)
            except OSError as e:
                print(f"fetch_icons: no icon for {n}: {e}")
        svgs.append(open(path, "rb").read() if k and os.path.exists(path) else None)
        if k is None:
            print(f"fetch_icons: {n} is not in Little Alchemy 2")
    print(f"fetch_icons: {sum(s is not None for s in svgs)}/{len(ours)} icons")

    # rasterise every icon into one atlas with headless Chrome
    cols = 32
    rows = (len(ours) + cols - 1) // cols
    urls = json.dumps(["data:image/svg+xml;base64," + base64.b64encode(s).decode() if s else "" for s in svgs])
    page = f"""<!doctype html><body><script>
const urls = {urls}, B = {BIG}, M = 4, cv = document.createElement('canvas');
cv.width = {cols} * B; cv.height = {rows} * B;
const g = cv.getContext('2d');
Promise.all(urls.map((u, i) => u ? new Promise(ok => {{
  const im = new Image();
  im.onload = () => {{ g.drawImage(im, (i % {cols}) * B + M, Math.floor(i / {cols}) * B + M, B - 2 * M, B - 2 * M); ok(); }};
  im.onerror = ok; im.src = u;
}}) : null)).then(() => {{
  const p = document.createElement('pre'); p.id = 'atlas'; p.textContent = cv.toDataURL('image/png');
  document.body.appendChild(p);
}});
</script></body>"""
    tmp = tempfile.mkdtemp()
    try:
        html = os.path.join(tmp, "atlas.html")
        open(html, "w").write(page)
        dom = subprocess.run([CHROME, "--headless=new", "--disable-gpu", "--virtual-time-budget=60000",
                              f"--user-data-dir={os.path.join(tmp, 'profile')}", "--dump-dom", "file://" + html],
                             capture_output=True, text=True, timeout=300).stdout
        m = re.search(r'id="atlas">data:image/png;base64,([^<]+)<', dom)
        if not m:
            sys.exit("fetch_icons: Chrome did not render the atlas")
        png = os.path.join(tmp, "atlas.png")
        open(png, "wb").write(base64.b64decode(m.group(1)))
        big = subprocess.run(["magick", png, "-depth", "8", "RGBA:-"], capture_output=True, check=True).stdout
        small = subprocess.run(["magick", png, "-filter", "Lanczos", "-resize", "50%", "-depth", "8", "RGBA:-"],
                               capture_output=True, check=True).stdout
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    def cell(raw, size, i):
        x0, y0, stride = (i % cols) * size, (i // cols) * size, cols * size * 4
        return b"".join(raw[(y0 + y) * stride + x0 * 4:(y0 + y) * stride + (x0 + size) * 4] for y in range(size))

    with open(OUT, "wb") as f:
        f.write(b"AL2I" + struct.pack(">HHHH", len(ours), SMALL, BIG, 0))
        for n in ours:
            f.write(n.encode()[:31].ljust(32, b"\0"))
        for i in range(len(ours)):
            f.write(cell(small, SMALL, i) + cell(big, BIG, i))
    print(f"fetch_icons: wrote {os.path.relpath(OUT, ROOT)} ({os.path.getsize(OUT) // 1024} KB)")


if __name__ == "__main__":
    main()
