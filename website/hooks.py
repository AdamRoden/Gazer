"""Copy the app favicon into docs/assets so the site does not store a second copy."""

from __future__ import annotations

import shutil
from pathlib import Path


def on_pre_build(config, **kwargs) -> None:
    website = Path(__file__).resolve().parent
    icons = website.parent / "resources" / "icons"
    dest = website / "docs" / "assets"
    dest.mkdir(parents=True, exist_ok=True)
    shutil.copy2(icons / "gazer-32.png", dest / "favicon.png")
