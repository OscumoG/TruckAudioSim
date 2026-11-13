#!/usr/bin/env python3
"""Generate three factory profiles per truck model."""
from collections import defaultdict
from copy import deepcopy
import json
from pathlib import Path

PRESETS = Path(__file__).resolve().parents[1] / "desktop/presets/default"
DESTINATION = PRESETS / "Variantes de cabina"
REFERENCES = Path(__file__).resolve().parent / "cab-reference"

# Models whose generic reference is sparse or duplicates another entry.
PREFERRED_REFERENCE = {
    "vehicle.intnational.9900i": "ATS, International, International 9900i, hook 2.7 m",
    "vehicle.kenworth.w900": "Kenworth W900, hook 3.6 m",
    "vehicle.volvo.vnl2018": "ATS, Volvo, Volvo VNL 2018, sleeper 740",
    "vehicle.volvo.vnl2025": "ATS, Volvo, Volvo VNL",
    "vehicle.westernstar.57x": "ATS, Western Star, Western Star 57X",
}

# Estimated driver-area positions in meters from the vehicle center line.
# Rear positions in coaches cover the area just behind the driver.
EXTRA_CABS = (
    ("ets2", "scania", "Scania", "Scania Touring", "vehicle.scania.touring", (0.98, -0.42, -0.70), (0.99, 0.52, 1.08)),
    ("ets2", "volvo", "Volvo", "Volvo 9700 DD", "vehicle.volvo.9700_dd", (0.96, -0.46, -0.64), (0.94, 0.58, 1.22)),
    ("ets2", "man", "MAN", "MAN Lion's Coach", "vehicle.man.lions_coach", (1.02, -0.40, -0.72), (0.95, 0.48, 1.02)),
    ("ets2", "skoda", "Škoda", "Škoda Superb / Scout", "vehicle.skoda.superb", (0.67, -0.41, -0.32), (0.65, -0.32, 0.70)),
    ("ats", "skoda", "Škoda", "Škoda Superb / Scout", "vehicle.skoda.scout_ats", (0.67, -0.41, -0.32), (0.65, -0.32, 0.70)),
)

def speaker(channel, name, position, kind="full", pair=None):
    return dict(id=channel, name=name, position=position, channel=channel,
                type=kind, gainDb=0, pair=pair)

def extra_cabs():
    for game, brand, brand_name, name, model, front, rear in EXTRA_CABS:
        drivers = []
        for suffix, channel, sign, other in (("L", "L", -1, "R"), ("R", "R", 1, "L")):
            for label, point in (("Puerta", front), ("Trasero", rear)):
                x, y, z = point
                driver = speaker(channel, f"{label} {suffix}", [sign * x, y, z], pair=f"{label}{other}")
                driver["id"] = f"{label}{suffix}"
                drivers.append(driver)
        bounds = {"min": [-front[0] - .14, -.8, front[2] - .35],
                  "max": [front[0] + .14, .85, rear[2] + .3]}
        yield Path(name.replace(" / ", " - ")), dict(
            truckerAuxPreset=1, name=name, vehicleName=name, vehicle=model,
            game=game, brand=brand, brandName=brand_name, layout=dict(bounds=bounds, speakers=drivers))

def surround(layout, mode):
    original = layout["speakers"]
    lo, hi = layout["bounds"]["min"], layout["bounds"]["max"]
    left = [s for s in original if s["channel"] == "L" and s["type"] != "sub"]
    right = [s for s in original if s["channel"] == "R" and s["type"] != "sub"]
    # Follow the existing door placement and pick a front pair capable of bass/mids.
    def front(group):
        return min(group, key=lambda s: (s["position"][2] > 0.12,
                                         {"midbass": 0, "full": 1, "small": 2, "mid": 3}.get(s["type"], 4),
                                         s["position"][2]))
    fl, fr = front(left)["position"], front(right)["position"]
    center = next((s["position"] for s in original if s["channel"] == "M"
                   and s["type"] != "sub" and s["position"][2] < 0), None)
    if center is None:
        dash = min(left, key=lambda s: s["position"][2])["position"]
        center = [0, round(max(lo[1], min(hi[1], dash[1], -0.12)), 2), dash[2]]
    sub = next((s["position"] for s in original if s["type"] == "sub"), None)
    if sub is None:
        sub = [0, round(lo[1] * 0.88, 2), round(hi[2] * 0.72, 2)]

    def surround_position(group, front_position, rear):
        front_x, front_y, front_z = front_position
        # Use a rear/upper reference when the cab layout has one.
        existing = [s["position"] for s in group if s["position"][2] > max(0.14, front_z + 0.3)]
        if rear and existing:
            return max(existing, key=lambda point: point[2])
        if not rear and mode == "5.1" and existing:
            return max(existing, key=lambda point: point[2])
        if not rear and existing:
            side = min(existing, key=lambda point: point[2])
            if max(point[2] for point in existing) - side[2] > 0.22:
                return side
        # Long sleepers leave more separation between side and rear objects.
        rear_z = hi[2] * 0.88
        z = rear_z if rear else max(front_z + 0.18, min(0.12, rear_z - 0.23))
        y = min(hi[1] - 0.04, max(front_y + 0.24, -0.22))
        return [round(front_x * (0.81 if rear else 0.95), 2), round(y, 2), round(z, 2)]

    return [
        speaker("FL", "Frontal izquierdo", fl, "full", "FR"),
        speaker("FR", "Frontal derecho", fr, "full", "FL"),
        speaker("FC", "Central del tablero", center, "mid"),
        speaker("LFE", "Subwoofer", sub, "sub"),
        speaker("SL", "Envolvente izquierdo", surround_position(left, fl, False), pair="SR"),
        speaker("SR", "Envolvente derecho", surround_position(right, fr, False), pair="SL"),
        *([
            speaker("BL", "Trasero izquierdo", surround_position(left, fl, True), pair="BR"),
            speaker("BR", "Trasero derecho", surround_position(right, fr, True), pair="BL"),
        ] if mode == "7.1" else []),
    ]

def realistic(layout):
    kept = []
    seen = set()
    for s in deepcopy(layout["speakers"]):
        point = (s["channel"], tuple(s["position"]))
        if point in seen:
            continue
        seen.add(point)
        s["gainDb"] = 0
        if s["channel"] == "M" and s["type"] != "sub":
            s["type"] = "mid"
        kept.append(s)
    ids = {s["id"] for s in kept}
    for s in kept:
        if s.get("pair") not in ids:
            s["pair"] = None
    # A midrange without a bass driver or full-range speaker leaves a hole in
    # the low mids. Use its original mounting point for a midbass instead.
    for channel in ("L", "R"):
        group = [s for s in kept if s["channel"] == channel and s["type"] != "sub"]
        group = [s for s in group if s["position"][2] <= 0.12] or group
        if any(s["type"] in ("midbass", "full", "small") for s in group):
            continue
        mid = next((s for s in group if s["type"] == "mid"), None)
        if mid:
            mid["type"] = "midbass" if any(s["type"] == "tweeter" for s in group) else "full"
    return kept

def fit_bounds(layout):
    for axis in range(3):
        coordinates = [s["position"][axis] for s in layout["speakers"]]
        layout["bounds"]["min"][axis] = min(layout["bounds"]["min"][axis], *coordinates)
        layout["bounds"]["max"][axis] = max(layout["bounds"]["max"][axis], *coordinates)

def main():
    models = defaultdict(list)
    counts = {"ets2": 0, "ats": 0}
    references = ((p, json.loads(p.read_text(encoding="utf-8"))) for p in sorted(REFERENCES.glob("*.json")))
    for source, profile in (*references, *extra_cabs()):
        vehicle, game = profile.get("vehicle", ""), profile.get("game")
        if game not in counts or not vehicle.startswith("vehicle."):
            continue
        models[game, vehicle.split("@")[0]].append((source, profile))
    written = set()
    for (game, model), choices in sorted(models.items()):
        source, profile = min(choices, key=lambda item: (
            item[0].stem != PREFERRED_REFERENCE.get(model),
            "@" in item[1]["vehicle"], len(item[0].stem), item[0].stem,
        ))
        folder = DESTINATION / game.upper()
        folder.mkdir(parents=True, exist_ok=True)
        # Filenames carry cab names or chassis details that are sometimes absent from the preset title.
        parts = source.stem.split(", ", 2)
        cab_name = parts[2] if parts[0] in ("ETS2", "ATS") and len(parts) == 3 else source.stem
        cab_name = profile.get("vehicleName") or cab_name.split(", hook ")[0]
        for mode, label in (("realistic", "Realista"), ("5.1", "5.1"), ("7.1", "7.1")):
            variant = deepcopy(profile)
            variant["vehicle"] = model
            variant.pop("hooks", None)
            variant["name"] = f"{game.upper()} › {cab_name.rstrip('-')} · {label}"
            variant["layout"]["label"] = label
            variant["layout"]["cabAudio"] = mode
            variant["layout"]["width"] = 1
            variant["layout"]["speakers"] = realistic(profile["layout"]) if mode == "realistic" else surround(profile["layout"], mode)
            fit_bounds(variant["layout"])
            destination = folder / f"{source.stem} - {label}.json"
            destination.write_text(json.dumps(variant, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
            written.add(destination)
        counts[game] += 1
    for stale in set(DESTINATION.rglob("*.json")) - written:
        stale.unlink()
    print(f"{len(models)} modelos, {counts}, {sum(counts.values()) * 3} perfiles")

if __name__ == "__main__":
    main()
