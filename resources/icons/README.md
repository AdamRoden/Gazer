# Icons

Brand files sit in this folder, not in `svg/`. `gazerIcon.svg` is the app mark. `gazerWord.svg` is the wordmark. `scripts/generate-icons.ps1` rasterizes them to `gazer.ico`, `gazer-*.png` (white mark on black), and `website/docs/assets/wordmark.png` (white on transparent).

Cell and zone `icon` names are filename stems in `svg/` (`menu`, `pushPin`, `keyTab`). Matching is case-insensitive; a trailing `Icon` is ignored. Unknown names fall back to the label.

Most SVGs are **Material Symbols**, style **Rounded**, from:

https://fonts.google.com/icons?icon.set=Material+Symbols&icon.style=Rounded

Download the 24px SVG and save it as `svg/<stem>.svg`. Prefer camelCase stems (`waterDrop`, `recordVoiceOver`) so they match the rest of the catalog. `KeySymbols` recolors the glyph to the cell foreground, so the file’s `fill` does not need to match the theme.
