# NightDriverStrip Web UI

The firmware now embeds a static `HTML/CSS/JS` administration UI from:

- [site/index.html](/Users/dave/source/repos/NightDriverStrip/site/index.html)
- [site/styles.css](/Users/dave/source/repos/NightDriverStrip/site/styles.css)
- [site/app.js](/Users/dave/source/repos/NightDriverStrip/site/app.js)

There is no Node, React, Vite, or external frontend build dependency anymore.

## Build

The PlatformIO pre-build hook runs:

- [tools/bake_site.py](/Users/dave/source/repos/NightDriverStrip/tools/bake_site.py)

That script gzips the static assets into `site/dist/` for embedding in flash.

## Runtime contract

The web UI is intentionally thin. It talks directly to the device APIs exposed in:

- [src/webserver.cpp](/Users/dave/source/repos/NightDriverStrip/src/webserver.cpp)

This means frontend changes should prefer preserving the existing API shape unless there is a clear firmware-side reason to change it.
