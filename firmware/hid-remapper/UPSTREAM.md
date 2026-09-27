# Upstream

This directory is a git subtree of upstream HID Remapper (decision I1 in
`docs/drafts/2026-09-27/implementation-design.md`).

| Item | Value |
| --- | --- |
| Repository | https://github.com/jfedor2/hid-remapper |
| Commit | `51ab8b367b810d1deb0d3e487e9b7fe8c7e9c24e` (2026-06-10, "Update gamepad-to-mouse example") |
| Imported | 2026-09-27, with `git subtree add --prefix=firmware/hid-remapper` (full history) |
| License | MIT (`LICENSE` in this directory). Some files carry their own notices. |

Orbit builds only the core in `firmware/src` (the file list of upstream's
Bluetooth build) from `firmware/orbit/components/hid_remapper_core`.

Changes to upstream files are kept minimal and marked with `// ORBIT:`.
List of changes: none.

To update: `git subtree pull --prefix=firmware/hid-remapper https://github.com/jfedor2/hid-remapper <commit>`
in a dedicated PR, then update the commit above and `ORBIT_UPSTREAM_COMMIT` in
`firmware/orbit/components/hid_remapper_core/CMakeLists.txt`.
