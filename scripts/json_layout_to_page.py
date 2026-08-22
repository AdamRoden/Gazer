"""One-off JSON layout -> Page XML converter."""
import json
import sys
from pathlib import Path


def esc(s):
    return (
        str(s)
        .replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
        .replace('"', "&quot;")
        .replace("'", "&apos;")
    )


def anchor_xml(name: str) -> str:
    n = (name or "").replace("Center", "")
    table = {
        "topLeft": "TopLeft",
        "top": "Top",
        "topRight": "TopRight",
        "left": "Left",
        "center": "Center",
        "right": "Right",
        "bottomLeft": "BottomLeft",
        "bottom": "Bottom",
        "bottomRight": "BottomRight",
        "topCenter": "Top",
        "bottomCenter": "Bottom",
        "leftCenter": "Left",
        "rightCenter": "Right",
    }
    return table.get(name, table.get(n, "TopLeft"))


def px(item, key):
    pk = f"{key}Px"
    if pk in item:
        return str(int(item[pk]))
    if key in item and not isinstance(item[key], dict):
        v = item[key]
        try:
            n = float(v)
        except (TypeError, ValueError):
            return None
        # JSON bare number is percent of the reference (100 = full span).
        return f"{n / 100.0}"
    return None


def action_xml(act: dict) -> str:
    t = act.get("type", "")
    if t == "typeText":
        return f'    <Action id="Send" value="{esc(act.get("text", ""))}"/>\n'
    if t == "command":
        return f'    <Action id="Command" value="{esc(act.get("name", ""))}"/>\n'
    if t == "openLayout":
        lid = esc(act.get("layoutId", ""))
        return f'    <Action id="Page" value="Open, Page, {lid}"/>\n'
    if t == "loadLayout":
        lid = esc(act.get("layoutId", ""))
        return (
            '    <Action id="Page" value="Close, Page, self"/>\n'
            f'    <Action id="Page" value="Open, Page, {lid}"/>\n'
        )
    if t == "closeLayout":
        return '    <Action id="Page" value="Close, Page, self"/>\n'
    if t == "speak":
        return f'    <Action id="Speak" value="{esc(act.get("text", ""))}"/>\n'
    return ""


def style_attrs(st: dict) -> str:
    if not st:
        return ""
    parts = []
    if "background" in st:
        parts.append(f'background="{esc(st["background"])}"')
    fg = st.get("foreground")
    if fg:
        parts.append(f'foreground="{esc(fg)}"')
    border = st.get("borderColor") or st.get("border")
    if border:
        parts.append(f'border="{esc(border)}"')
    th = st.get("borderWidth") or st.get("thickness")
    if th is not None:
        parts.append(f'thickness="{th}"')
    rad = st.get("borderRadius") or st.get("radius")
    if rad is not None:
        parts.append(f'radius="{rad}"')
    blur = st.get("blur") or st.get("glass")
    if blur is True:
        parts.append('blur="15"')
    elif blur:
        parts.append(f'blur="{blur}"')
    return (" " + " ".join(parts)) if parts else ""


def convert(src: Path, dest: Path):
    data = json.loads(src.read_text(encoding="utf-8"))
    pid = data["id"]
    name = data.get("name", pid)
    bounds = data.get("boundsMode") or data.get("window", {}).get("boundsMode", "desktop")
    desktop = "true" if bounds in ("desktop", "available", "work") else "false"
    win = data.get("window") or {}
    grid = data.get("grid") or {}
    w = px(win, "width") or "600"
    h = px(win, "height") or "400"
    x = px(win, "x") or "0"
    y = px(win, "y") or "0"
    anc = anchor_xml(win.get("anchor") or "topLeft")
    cols = int(grid.get("columns") or 1)
    rows = int(grid.get("rows") or 1)
    gap = int(grid.get("gapPx") or 0)
    margin = int(grid.get("marginPx") or 0)
    above = "true" if win.get("aboveTaskbar") else "false"
    st_page = style_attrs({**(win.get("style") or {}), **(data.get("style") or {})})
    ac = ""
    if data.get("autoClose"):
        ac += ' autoClose="true"'
        if data.get("autoCloseIdleMs"):
            ac += f' autoCloseIdleMs="{int(data["autoCloseIdleMs"])}"'
    dwell = data.get("dwell") or {}
    dwell_attr = ""
    if "scanGraceMs" in dwell:
        dwell_attr += f' scanGrace="{dwell["scanGraceMs"]}"'
    ms = dwell.get("ms")
    if isinstance(ms, list):
        dwell_attr += f' activation="{",".join(str(x) for x in ms)}"'
    elif ms is not None:
        dwell_attr += f' activation="{ms}"'

    page_ac = ac
    lines = [f'<Page id="{esc(pid)}" name="{esc(name)}"{page_ac}>\n']
    lines.append(
        f'  <Grid id="board" desktopMode="{desktop}" rows="{rows}" columns="{cols}" '
        f'anchor="{anc}" offset="{x},{y}" size="{w},{h}" gap="{gap}" margin="{margin}" '
        f'aboveTaskbar="{above}"{ac}{st_page}{dwell_attr}>\n'
    )
    zones = []
    for it in data.get("items") or []:
        acts = []
        if it.get("actions"):
            for a in it["actions"]:
                acts.append(action_xml(a))
        elif it.get("action"):
            acts.append(action_xml(it["action"]))
        extra = ""
        if it.get("icon"):
            extra += f' icon="{esc(it["icon"])}"'
        if it.get("label"):
            extra += f' label="{esc(it["label"])}"'
        if it.get("caption"):
            extra += f' caption="{esc(it["caption"])}"'
        if it.get("role"):
            extra += f' role="{esc(it["role"])}"'
        if it.get("textStyle"):
            extra += f' textStyle="{esc(it["textStyle"])}"'
        if it.get("settingKey"):
            extra += f' settingKey="{esc(it["settingKey"])}"'
        if it.get("cluster"):
            extra += f' cluster="{esc(it["cluster"])}"'
        if it.get("clusterSlot"):
            extra += f' clusterSlot="{esc(it["clusterSlot"])}"'
        extra += style_attrs(it.get("style") or {})
        if it.get("activeState"):
            extra += f' activeState="{esc(it["activeState"])}"'
        if it.get("visibleWhen"):
            extra += f' visibleWhen="{esc(it["visibleWhen"])}"'
        if it.get("dwellExempt"):
            extra += ' dwellExempt="true"'
        if it.get("interactive") is False:
            extra += ' interactive="false"'
        if it.get("actionLoop"):
            extra += ' actionLoop="true"'
        idw = it.get("dwell") or {}
        if "scanGraceMs" in idw:
            extra += f' scanGrace="{idw["scanGraceMs"]}"'
        body = "".join(acts)
        if it.get("screenAnchor"):
            za = anchor_xml(it["screenAnchor"])
            zw = px(it, "width") or "100"
            zh = px(it, "height") or "48"
            zx = px(it, "x") or "0"
            zy = px(it, "y") or "0"
            z = (
                f'  <Zone id="{esc(it["id"])}" anchor="{za}" offset="{zx},{zy}" '
                f'size="{zw},{zh}"{extra}'
            )
            if body:
                z += ">\n" + body + "  </Zone>\n"
            else:
                z += "/>\n"
            zones.append(z)
        else:
            row = int(it.get("row") or 0)
            col = int(it.get("col") or 0)
            rs = int(it.get("rowSpan") or 1)
            cs = int(it.get("colSpan") or 1)
            span = ""
            if rs != 1:
                span += f' rowSpan="{rs}"'
            if cs != 1:
                span += f' colSpan="{cs}"'
            cell = (
                f'    <Cell id="{esc(it["id"])}" row="{row}" col="{col}"{span}{extra}'
            )
            if body:
                cell += ">\n" + body + "    </Cell>\n"
            else:
                cell += "/>\n"
            lines.append(cell)
    lines.append("  </Grid>\n")
    lines.extend(zones)
    lines.append("</Page>\n")
    dest.write_text("".join(lines), encoding="utf-8")
    print(f"wrote {dest}")


SKIP = {"main_master", "main_drawer", "main_quit_confirm"}


def should_skip(stem: str) -> bool:
    return stem in SKIP or stem.startswith("main_settings")


def main():
    root = Path(__file__).resolve().parents[1] / "resources" / "layouts"
    if sys.argv[1:]:
        names = sys.argv[1:]
    else:
        names = [p.stem for p in sorted(root.glob("*.json")) if not should_skip(p.stem)]
    for n in names:
        src = root / f"{n}.json"
        dest = root / f"{n}.xml"
        convert(src, dest)


if __name__ == "__main__":
    main()
