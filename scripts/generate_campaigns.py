import os
import sys
import json

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "external", "BadLevel")))

from badlevel.recipes import get_recipe, compile_recipe, roomier, varied
from badlevel.generator import generate, GenerationError
from badlevel.export.ir_json import to_dict
from badlevel.export.mesh import build_mesh
from badlevel.export.gltf import to_glb
from badlevel.export.quake_map import to_map


def main():
    out_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "assets", "levels"))
    os.makedirs(out_dir, exist_ok=True)

    campaigns = [
        # Campaign 1: The Elder Ward (Gothic / Castle / Runic / Fortress)
        ("c1m1", "The Slipgate Sanctum",   1, 1, 101,  "classic",    "gothic",     5, 2, 1, 1, 1, 0.35, 0.35),
        ("c1m2", "Vault of the Iron Keys", 1, 2, 204,  "classic",    "castle",     6, 3, 2, 1, 2, 0.40, 0.45),
        ("c1m3", "Catacombs of Sorrow",    1, 3, 307,  "fortress",   "gothic",     7, 3, 2, 2, 2, 0.45, 0.50),
        ("c1m4", "The Sunken Basilica",    1, 4, 419,  "fortress",   "egyptian",   8, 4, 3, 2, 3, 0.50, 0.55),
        ("c1m5", "Altar of the Elder God", 1, 5, 523,  "fortress",   "gothic",     9, 4, 3, 2, 3, 0.55, 0.60),
        # Campaign 2: The Iron Citadel (Bunker / Industrial / Compound / Sci-Fi)
        ("c2m1", "Perimeter Breach",       2, 1, 601,  "bunker",     "bunker",     5, 2, 1, 1, 2, 0.35, 0.35),
        ("c2m2", "Slag Processing Works",  2, 2, 709,  "industrial", "industrial", 6, 3, 2, 1, 2, 0.45, 0.45),
        ("c2m3", "Logistics Compound",     2, 3, 811,  "compound",   "compound",   7, 3, 2, 2, 1, 0.50, 0.50),
        ("c2m4", "Reactor Sub-Level 9",    2, 4, 919,  "bunker",     "scifi",      8, 4, 3, 2, 3, 0.50, 0.55),
        ("c2m5", "The Iron Citadel Core",  2, 5, 1031, "industrial", "scifi",      9, 4, 3, 2, 3, 0.55, 0.60),
    ]

    manifest = []
    for code, title, camp, idx, seed, rec_name, style, mrooms, srooms, loops, keys, storeys, sp, va in campaigns:
        rec = get_recipe(rec_name)
        s = seed
        lvl = None
        while lvl is None:
            try:
                cfg = compile_recipe(rec, s)
                cfg.main_rooms = mrooms
                cfg.side_rooms = srooms
                cfg.loops = loops
                cfg.keys = keys
                cfg.storeys = storeys
                cfg.arch_style = style
                roomier(cfg, sp)
                varied(cfg, va)
                lvl = generate(cfg)
            except GenerationError:
                s += 1

        d = to_dict(lvl, include_spans=True)
        d["level_code"] = code
        d["title"] = title
        d["campaign"] = camp
        d["level_index"] = idx
        d["style"] = style
        d["recipe"] = rec_name

        mb = build_mesh(lvl)
        mesh_groups = {}
        for mat, g in mb.groups.items():
            if g["idx"]:
                mesh_groups[mat] = {
                    "pos": g["pos"],
                    "nrm": g["nrm"],
                    "uv": g["uv"],
                    "idx": g["idx"]
                }
        d["render_mesh"] = mesh_groups

        json_path = os.path.join(out_dir, f"{code}.json")
        with open(json_path, "w") as f:
            json.dump(d, f)

        glb_path = os.path.join(out_dir, f"{code}.glb")
        with open(glb_path, "wb") as f:
            f.write(to_glb(lvl))

        map_path = os.path.join(out_dir, f"{code}.map")
        with open(map_path, "w") as f:
            f.write(to_map(lvl))

        st = lvl.stats()
        print(f"{code} ({title}): seed={s}, {st['rooms']} rooms, {st['corridors']} corridors, "
              f"{st['entities']} entities, storeys={st['storeys']}, style={style}")
        manifest.append({
            "code": code,
            "title": title,
            "campaign": camp,
            "level_index": idx,
            "seed": s,
            "style": style,
            "recipe": rec_name,
            "stats": st
        })

    with open(os.path.join(out_dir, "campaigns.json"), "w") as f:
        json.dump({"campaigns": manifest}, f, indent=2)


if __name__ == "__main__":
    main()
