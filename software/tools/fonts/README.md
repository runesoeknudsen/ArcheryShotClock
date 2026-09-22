# Panel typefaces

Large cabinets stamp filled outlines taken from a real TTF or OTF. The
32×16 bitmap face is separate and is not generated here.

## Generate a face

From the repo root, with [fontTools](https://pypi.org/project/fonttools/)
installed:

```text
python3 software/tools/font_to_glyphs.py \
  --font /path/to/SomeSans-Bold.ttf \
  --name some_sans \
  --out software/firmware/src/core/generated/font_some_sans.h
```

Then include that header from `software/firmware/src/core/font_faces.cpp`
and return its `kFont…` from `activeFontFace()`.

The default face is DejaVu Sans Bold (`sans_serif`). Its license is
`LICENSE-DejaVu.txt`. Regenerate with:

```text
python3 software/tools/font_to_glyphs.py \
  --font /usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf \
  --name sans_serif \
  --out software/firmware/src/core/generated/font_sans_serif.h
```

Use a freely licensed font. Generated outlines are a derived work of that
font; keep its notice in `NOTICE.md`.

The clock needs `0-9`, `A-H`, `K`, `N`, `O`, `R`, `S`, `:`, and `-`. Extra
characters can be passed with `--chars`.
