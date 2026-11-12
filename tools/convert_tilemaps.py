#!/usr/bin/env python3
"""Outils tilesets AngeVault (Python 3 + Pillow : pip install pillow).

1) Convertir les anciennes cartes (.map version 1, « une image par tile ») au format tilesets (version 2) :

       python tools/convert_tilemaps.py maps projects/rpg

   Pour chaque carte de <projet>/maps/*.map en version 1 :
     - chaque ancienne tile devient un tileset (feuille PNG + .tileset) dans assets/tilesets/<carte>/ :
         simple, 1 image        -> tileset simple (1 tile)
         simple, plusieurs      -> autotile 4 bords avec seulement des cases pleines (variantes au hasard)
         autotile 4 bords       -> autotile 4 bords, disposition standard (pleine / 1 bord / 2 bords / 4 bords / dessous)
         autotile 16 coins      -> autotile 16 coins (les feuilles variantes sont empilées en lignes)
     - l'ancien fichier est gardé à côté : <carte>.map.v1

2) Ranger un pack isométrique « une PNG par tile » (ex. Fantasy tileset - 2D Isometric) en feuilles :

       python tools/convert_tilemaps.py pack-iso "<dossier Environment>" projects/rpg assets/tilesets/fantasy_iso

   Une feuille par famille (« Ground A », « Wall F »...), 4 colonnes = rotations N E S W, une ligne par numéro.
   Le vide commun est rogné ; l'ancre est recalculée (option --anchor, 128 208 par défaut).
"""
import argparse
import json
import os
import re
import shutil
import sys
import unicodedata

try:
    from PIL import Image
except ImportError:
    sys.exit("Pillow est nécessaire : pip install pillow")

MAX_ROWS = 30          # 30 x 256 px = 7680 px : sous la limite de 8192 des cartes graphiques
ROT = "NESW"


# ---------------------------------------------------------------------------------------------- outils
def slug(name):
    s = unicodedata.normalize("NFKD", name).encode("ascii", "ignore").decode()
    s = re.sub(r"[^A-Za-z0-9]+", "_", s).strip("_").lower()
    return s or "tileset"


def natural_key(s):
    return [int(t) if t.isdigit() else t.lower() for t in re.split(r"(\d+)", s)]


def union_bbox(images):
    box = None
    for im in images:
        if im is None:
            continue
        b = im.getchannel("A").point(lambda a: 255 if a > 8 else 0).getbbox()
        if b is None:
            continue
        box = b if box is None else (min(box[0], b[0]), min(box[1], b[1]), max(box[2], b[2]), max(box[3], b[3]))
    return box


def build_sheet(rows, trim=True):
    """rows : liste de lignes d'images (None = case vide). Renvoie (feuille, (w, h), décalage du rognage)."""
    flat = [im for row in rows for im in row if im is not None]
    if not flat:
        raise ValueError("aucune image")
    W = max(im.width for im in flat)
    H = max(im.height for im in flat)
    box = union_bbox(flat) if trim else None
    if box is None:
        box = (0, 0, W, H)
    tw, th = box[2] - box[0], box[3] - box[1]
    cols = max(len(r) for r in rows)
    sheet = Image.new("RGBA", (cols * tw, len(rows) * th), (0, 0, 0, 0))
    for y, row in enumerate(rows):
        for x, im in enumerate(row):
            if im is not None:
                sheet.paste(im.crop(box), (x * tw, y * th))
    return sheet, (tw, th), (box[0], box[1]), cols


def write_tileset(project, rel_tileset, data):
    path = os.path.join(project, rel_tileset)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    doc = {"format": "angeVault.tileset", "version": 1}
    doc.update(data)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(doc, f, ensure_ascii=False, indent=2)
        f.write("\n")


def rel(path, project):
    return os.path.relpath(path, project).replace("\\", "/")


# ----------------------------------------------------------------------------------- conversion des cartes
def convert_map(project, map_path):
    with open(map_path, encoding="utf-8") as f:
        old = json.load(f)
    if old.get("format") != "angeVault.tilemap" or old.get("version", 1) != 1:
        print(f"  {rel(map_path, project)} : déjà au nouveau format, ignorée")
        return False
    iso = old.get("orientation") == "isometric"
    stem = os.path.splitext(os.path.basename(map_path))[0]
    out_dir = os.path.join(project, "assets", "tilesets", stem)
    tiles = old.get("tiles", [])
    by_name = {t.get("name"): t for t in tiles}

    def load(p):
        full = os.path.join(project, p)
        return Image.open(full).convert("RGBA") if os.path.exists(full) else None

    tileset_files = []
    for t in tiles:
        name = t.get("name", "Tile")
        mode = t.get("autotile", "none")
        images = [load(p) for p in t.get("images", [])]
        images = [im for im in images if im is not None]
        png = os.path.join(out_dir, slug(name) + ".png")
        ts = {"name": name}
        anchor = t.get("anchor")
        if mode == "corners16":
            cell = int(t.get("cellPixels", 16))
            # chaque feuille variante : ses lignes sont empilées
            parts = images or [Image.new("RGBA", (16 * cell, cell))]
            sheet = Image.new("RGBA", (16 * cell, sum(p.height for p in parts)))
            y = 0
            for p in parts:
                sheet.paste(p.crop((0, 0, 16 * cell, p.height)), (0, y))
                y += p.height
            os.makedirs(out_dir, exist_ok=True)
            sheet.save(png)
            ts.update(image=rel(png, project), tileSize=[cell, cell], margin=0, spacing=0, columns=16,
                      count=16 * (sheet.height // cell), autotile="corners16")
            if t.get("frameTime", 0) > 0:
                ts["frameTime"] = t["frameTime"]
        else:
            if mode == "edges16":
                base = by_name.get(t.get("base", ""))
                under = [load(p) for p in base.get("images", [])] if base else []
                four = lambda key: [load(p) if p else None for p in (t.get(key, []) + [""] * 4)[:4]]
                rows = [images[:4] + [None] * (4 - len(images[:4])), four("edge"), four("corner"),
                        ([load(p) for p in t.get("ring", [])] + [None] * 4)[:4],
                        ([im for im in under if im is not None] + [None] * 4)[:4]]
                rows = [r for r in rows]  # 5 lignes, disposition standard
            elif len(images) > 1:
                rows = [images[i:i + 4] for i in range(0, len(images), 4)]
            else:
                rows = [images[:1]]
            sheet, (tw, th), (ox, oy), cols = build_sheet(rows, trim=iso)
            os.makedirs(out_dir, exist_ok=True)
            sheet.save(png)
            count = cols * len(rows)
            ts.update(image=rel(png, project), tileSize=[tw, th], margin=0, spacing=0, columns=cols, count=count)
            if anchor:
                ts["anchor"] = [anchor[0] - ox, anchor[1] - oy]
            elif iso:
                W = max(im.width for r in rows for im in r if im is not None)
                H = max(im.height for r in rows for im in r if im is not None)
                ts["anchor"] = [W / 2 - ox, H / 2 - oy]
            if mode == "edges16":
                idx = lambda r, c: r * cols + c if rows[r][c] is not None else -1
                ts.update(autotile="edges16",
                          full=[idx(0, c) for c in range(4) if idx(0, c) >= 0],
                          edge=[idx(1, c) for c in range(4)], corner=[idx(2, c) for c in range(4)])
                ring = [idx(3, c) for c in range(4) if idx(3, c) >= 0]
                under = [idx(4, c) for c in range(4) if idx(4, c) >= 0]
                if ring:
                    ts["ring"] = ring
                if under:
                    ts["under"] = under
            elif len(images) > 1:   # variantes : un « terrain » sans transitions
                ts.update(autotile="edges16", full=list(range(len(images))), edge=[-1] * 4, corner=[-1] * 4)
            else:
                ts["autotile"] = "none"
        if t.get("scale", 1) != 1:
            ts["scale"] = t["scale"]
        if t.get("solid"):
            ts["solid"] = True
        rel_ts = rel(os.path.join(out_dir, slug(name) + ".tileset"), project)
        write_tileset(project, rel_ts, ts)
        tileset_files.append((rel_ts, ts.get("autotile", "none") != "none"))
        print(f"  tileset {rel_ts}  ({ts.get('autotile')})")

    # Cases : id v1 (1..n) -> « slot:a » (autotile) ou « slot:0 » (tile simple)
    def token(i):
        if i <= 0 or i > len(tileset_files):
            return "0"
        return f"{i - 1}:a" if tileset_files[i - 1][1] else f"{i - 1}:0"

    rows = []
    for row in old.get("cells", []):
        out = []
        for tok in row.split():
            if "*" in tok:
                n, v = tok.split("*")
                out.append(f"{n}*{token(int(v))}")
            else:
                out.append(token(int(tok)))
        rows.append(" ".join(out))
    new = {"format": "angeVault.tilemap", "version": 2, "orientation": old.get("orientation", "orthogonal"),
           "size": old.get("size", [0, 0]), "tileSize": old.get("tileSize", [32, 32]),
           "tilesets": [f for f, _ in tileset_files], "cells": rows}
    shutil.copyfile(map_path, map_path + ".v1")
    with open(map_path, "w", encoding="utf-8") as f:
        json.dump(new, f, ensure_ascii=False, indent=2)
        f.write("\n")
    print(f"  {rel(map_path, project)} : convertie ({len(tileset_files)} tileset(s)), ancienne version : .map.v1")
    return True


# ------------------------------------------------------------------------------------ pack isométrique
def pack_iso(env_dir, project, out_rel, anchor):
    pat = re.compile(r"^(.*?)(\d+)_([NESW])\.png$")
    families, loose = {}, []
    for f in os.listdir(env_dir):
        if not f.lower().endswith(".png"):
            continue
        m = pat.match(f)
        if m:
            fam = m.group(1).strip()
            families.setdefault(fam, {}).setdefault(int(m.group(2)), {})[m.group(3)] = f
        else:
            loose.append(f)
    out_dir = os.path.join(project, out_rel)
    os.makedirs(out_dir, exist_ok=True)
    made = []

    def emit(name, rows, labels):
        sheet, (tw, th), (ox, oy), cols = build_sheet(rows, trim=True)
        png = os.path.join(out_dir, slug(name) + ".png")
        sheet.save(png, optimize=True)
        data = {"name": name, "image": rel(png, project), "tileSize": [tw, th], "margin": 0, "spacing": 0,
                "columns": cols, "count": cols * len(rows), "anchor": [anchor[0] - ox, anchor[1] - oy],
                "autotile": "none"}
        rel_ts = rel(os.path.join(out_dir, slug(name) + ".tileset"), project)
        write_tileset(project, rel_ts, data)
        made.append(rel_ts)
        print(f"  {rel_ts}  {cols} x {len(rows)}  ({labels})")

    for fam in sorted(families, key=natural_key):
        nums = sorted(families[fam])
        load = lambda f: Image.open(os.path.join(env_dir, f)).convert("RGBA") if f else None
        rows = [[load(families[fam][n].get(r)) for r in ROT] for n in nums]
        parts = [(rows[i:i + MAX_ROWS], nums[i:i + MAX_ROWS]) for i in range(0, len(rows), MAX_ROWS)]
        for k, (part, pn) in enumerate(parts):
            name = fam if len(parts) == 1 else f"{fam} ({pn[0]}-{pn[-1]})"
            emit(name, part, f"{fam} {pn[0]}..{pn[-1]}")

    # Images sans rotation (torches, feu...) : une feuille « Divers », images de même taille seulement
    same = [f for f in sorted(loose, key=natural_key) if Image.open(os.path.join(env_dir, f)).size == (256, 256)]
    if same:
        ims = [Image.open(os.path.join(env_dir, f)).convert("RGBA") for f in same]
        rows = [ims[i:i + 4] for i in range(0, len(ims), 4)]
        rows = [r + [None] * (4 - len(r)) for r in rows]
        emit("Divers", rows, ", ".join(os.path.splitext(f)[0] for f in same))
    return made


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    m = sub.add_parser("maps", help="convertir les .map v1 d'un projet")
    m.add_argument("project")
    p = sub.add_parser("pack-iso", help="ranger un pack iso (une PNG par tile) en feuilles")
    p.add_argument("env_dir")
    p.add_argument("project")
    p.add_argument("out", help="dossier de sortie, relatif au projet (ex. assets/tilesets/fantasy_iso)")
    p.add_argument("--anchor", nargs=2, type=float, default=[128, 208],
                   help="point des images d'origine posé au centre de la case (défaut : 128 208)")
    a = ap.parse_args()
    if a.cmd == "maps":
        maps = os.path.join(a.project, "maps")
        for f in sorted(os.listdir(maps)):
            if f.endswith(".map"):
                convert_map(a.project, os.path.join(maps, f))
    else:
        pack_iso(a.env_dir, a.project, a.out, a.anchor)


if __name__ == "__main__":
    main()
