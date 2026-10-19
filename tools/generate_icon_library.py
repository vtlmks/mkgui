#!/usr/bin/env python3
# Copyright (c) 2026 Peter Fors
# SPDX-License-Identifier: MIT

import json
import re
from collections import defaultdict
from pathlib import Path

root = Path(__file__).resolve().parent.parent
assets = root / "fluentui-system-icons" / "assets"
output = root / "out" / "icons"
output.mkdir(parents=True, exist_ok=True)
pattern = re.compile(r"ic_fluent_(.+)_(\d+)_(regular|filled|light|color)(?:_(ltr|rtl))?")
groups = defaultdict(list)
metadata = {}
payload = bytearray()
strings = {}
sources = {}


def add_string(value):
	if value not in strings:
		strings[value] = len(payload)
		payload.extend(value.encode("utf-8") + b"\0")
	return strings[value]


for path in sorted(assets.rglob("*.svg")):
	match = pattern.fullmatch(path.stem)
	if not match:
		raise SystemExit("Unrecognized icon filename: " + str(path))
	base, size, style, direction = match.groups()
	name = base.replace("_", "-") + ("@" + direction if direction else "")
	relative = path.relative_to(assets)
	if len(relative.parts) > 3:
		name += "@" + "-".join(relative.parts[1:-2])
	data = path.read_bytes()
	if style != "color":
		data = re.sub(rb'fill="(?!none"|url\()[^"]*"', b'fill="currentColor"', data)
		start = re.search(rb"<svg\b[^>]*>", data)
		if start is None:
			raise SystemExit("SVG root missing: " + str(path))
		if not re.search(rb"\bfill=", start.group()):
			data = data[:start.end() - 1] + b' fill="currentColor"' + data[start.end() - 1:]
	key = (name, style, int(size))
	if key in sources:
		if sources[key] != data:
			raise SystemExit("Conflicting SVG identity: " + str(path))
		continue
	sources[key] = data
	offset = len(payload)
	payload.extend(data + b"\0")
	groups[(name, style)].append((int(size), offset, len(data)))
	meta_path = assets / relative.parts[0] / "metadata.json"
	if name not in metadata:
		meta = json.loads(meta_path.read_text()) if meta_path.exists() else {}
		metadata[name] = " ".join([str(meta.get("description") or ""), *(meta.get("metaphor") or [])]).lower()

families = sorted({name for name, style in groups})
topics = sorted({name.split("-")[0] for name in families})
topic_indices = {name: index for index, name in enumerate(topics)}
entries = {}
for name in families:
	styles = [style for style in ("regular", "filled", "light", "color") if (name, style) in groups]
	entries[name] = (name, styles[0])
	for style in styles:
		if style != "regular":
			entries[name + ":" + style] = (name, style)

variants = []
ranges = {}
for key, values in sorted(groups.items()):
	values.sort()
	if len({value[0] for value in values}) != len(values):
		raise SystemExit("Duplicate size: " + str(key))
	ranges[key] = (len(variants), len(values))
	variants.extend(values)

ordered = sorted(entries)
rows = []
macros = {}
for name in ordered:
	base, style = entries[name]
	first, count = ranges[(base, style)]
	rows.append((add_string(name), first, count, topic_indices[base.split("-")[0]], add_string(metadata[base])))
	macro = "MKICON_" + name.upper().replace("-", "_").replace(":", "_").replace("@", "_LANG_")
	if macro in macros:
		raise SystemExit("Duplicate C name: " + macro)
	macros[macro] = name

topic_rows = [add_string(name) for name in topics]
for filename in ("LICENSE", "NOTICE"):
	payload.extend((root / "fluentui-system-icons" / filename).read_bytes() + b"\0")

copyright = "// Copyright (c) 2026 Peter Fors\n// SPDX-License-Identifier: MIT\n\n"
release = (root / "mkguiicons.version").read_text().strip()
if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", release):
	raise SystemExit("Invalid icon release: " + release)
lines = [copyright, f'static char icon_release[] = "{release}";\n\n']
for declaration, records in [("struct mkguiicons_entry icon_entries", rows), ("struct mkguiicons_variant icon_variants", variants)]:
	lines.append("static " + declaration + "[] = {\n")
	lines.extend("\t{" + ", ".join(str(value) for value in record) + "},\n" for record in records)
	lines.append("};\n\n")
lines.append("static uint32_t icon_topics[] = {\n")
lines.extend("\t" + str(value) + ",\n" for value in topic_rows)
lines.append("};\n")
(output / "catalogue.h").write_text("".join(lines))
(output / "data.bin").write_bytes(payload)
header = copyright + "#ifndef MKGUIICONS_NAMES_H\n#define MKGUIICONS_NAMES_H\n\n"
header += "".join('#define ' + macro + ' "' + name + '"\n' for macro, name in sorted(macros.items()))
header += "\n#endif\n"
(root / "mkguiicons_names.h").write_text(header)
print(f"[icons] {len(families)} families, {len(entries)} names, {len(variants)} SVGs, {len(topics)} name groups, {len(payload)} bytes")
