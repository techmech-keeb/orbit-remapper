#!/usr/bin/env python3
"""Collects the license notices for a firmware image, for the release.

The image carries code from ESP-IDF, its toolchain's C and C++ runtime,
the managed components (TinyUSB, LVGL, ...) and upstream HID Remapper.
Their licenses (Apache-2.0, MIT, BSD, ...) ask that a binary comes with the
license texts and copyright notices, so the release attaches this file.

What is in the image is read from the linker map (the archives a member
was taken from), not from the component list, which also names
components that are built but not linked. For each such component it
copies every LICENSE / LICENCE / COPYING / NOTICE file up to three
directories deep (skipping tests, examples and docs) and the copyright
lines esp-idf-sbom found in it; an ESP-IDF component without a file of its
own falls under ESP-IDF's LICENSE. Over-inclusion is fine; a missing notice
is not.

Usage: third-party-licenses.py <build-dir> <esp-idf-sbom license -p --format json> <output>
"""

import json
import os
import re
import sys

NAME = re.compile(r"^(LICEN[CS]E|COPYING|NOTICE)", re.IGNORECASE)
SKIP_DIR = re.compile(r"^(\.|test|example|doc)", re.IGNORECASE)
MAX_DEPTH = 3


def linked_components(map_file):
    text = open(map_file, encoding="utf-8", errors="replace").read()
    head = text.split("Memory Configuration")[0]  # "Archive member included ..."
    return sorted(set(re.findall(r"esp-idf/([A-Za-z0-9_\-]+)/lib[^/\s()]+\.a\(", head)))


def license_files(base):
    found = []
    for root, dirs, files in os.walk(base):
        depth = os.path.relpath(root, base).count(os.sep) + (root != base)
        dirs[:] = sorted(d for d in dirs if not SKIP_DIR.match(d)) if depth < MAX_DEPTH else []
        found += [os.path.join(root, f) for f in sorted(files) if NAME.match(f)]
    return found


def main():
    build, sbom_json, output = sys.argv[1:4]
    desc = json.load(open(os.path.join(build, "project_description.json"), encoding="utf-8"))
    sbom = json.load(open(sbom_json, encoding="utf-8"))
    idf = desc["idf_path"]
    repo = os.path.normpath(os.path.join(desc["project_path"], "..", ".."))
    info = desc["build_component_info"]
    packages = {p["name"]: p for p in sbom["packages"]}

    toolchain = os.path.dirname(os.path.dirname(os.path.realpath(desc["c_compiler"])))

    def shown(path):
        for prefix, label in ((idf, "esp-idf"), (repo, "orbit-remapper"), (toolchain, "toolchain")):
            if path.startswith(prefix + os.sep):
                return label + "/" + os.path.relpath(path, prefix)
        return os.path.basename(path)

    sections = []  # (title, copyrights, files)
    sections.append(("ESP-IDF " + desc.get("git_revision", ""), [], [os.path.join(idf, "LICENSE")]))
    for name in linked_components(os.path.join(build, desc["project_name"] + ".map")):
        if name == "main":
            continue  # Orbit itself: LICENSE at the end
        base = info[name]["dir"]
        files = license_files(base)
        if name == "hid_remapper_core":  # built from the subtree, not from its own directory
            files = [os.path.join(repo, "firmware", "hid-remapper", "LICENSE")]
        copyrights = sorted({c for p, v in packages.items() if p == name or p.startswith(name + "-")
                             for c in v.get("copyrights", [])})
        sections.append((name, copyrights, files))
    runtime = [f for lib in ("gcc", "newlib") for f in license_files(os.path.join(toolchain, "share", "licenses", lib))]
    sections.append(("toolchain runtime (libgcc, libstdc++, newlib)", [], runtime))
    sections.append(("Orbit Remapper", [], [os.path.join(repo, "LICENSE")]))

    # An ESP-IDF component without its own file is under ESP-IDF's LICENSE (the first section).
    missing = [title for title, _, files in sections if not files and not info.get(title, {}).get("dir", "").startswith(idf + os.sep)]
    if missing:
        sys.exit("no license file found for: " + ", ".join(missing))

    seen = {}  # text -> where it was first written
    with open(output, "w", encoding="utf-8") as out:
        out.write("Third-party notices for the Orbit Remapper firmware image\n")
        out.write("Generated from the build by dev/tools/build/third-party-licenses.py.\n")
        out.write("Sections: " + ", ".join(t for t, _, _ in sections) + "\n")
        for title, copyrights, files in sections:
            out.write("\n" + "=" * 78 + "\n" + title + "\n" + "=" * 78 + "\n")
            if copyrights:
                out.write("\nCopyright notices found in the sources:\n")
                out.writelines("  Copyright " + c + "\n" for c in copyrights)
            if not files:
                out.write("\nUnder the ESP-IDF license above (no license file of its own).\n")
            for f in files:
                text = open(f, encoding="utf-8", errors="replace").read().rstrip()
                out.write("\n--- " + shown(f) + " ---\n\n")
                if text in seen:
                    out.write("(the same text as " + seen[text] + ")\n")
                else:
                    seen[text] = shown(f)
                    out.write(text + "\n")
    print(f"{output}: {len(sections)} sections, {sum(len(f) for _, _, f in sections)} license files")


if __name__ == "__main__":
    main()
