#!/bin/sh
# Copies the browser demo into docs/ so GitHub Pages can also be served
# from the branch /docs folder (Settings → Pages → Deploy from a branch).
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cp "$root/web/demo/index.html" "$root/web/demo/app.js" "$root/web/demo/engine.js" \
   "$root/web/demo/panel.js" "$root/web/demo/operator.js" "$root/web/demo/core.js" \
   "$root/web/demo/core.wasm" \
   "$root/../docs/"
touch "$root/../docs/.nojekyll"
echo "synced software/web/demo into docs/"
