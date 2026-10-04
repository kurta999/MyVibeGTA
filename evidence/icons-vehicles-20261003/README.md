Validated the Direct3D 11 MiniCity3D build on 2026-10-03.

- All 21 CTest suites passed. The full report is `ctest.log`.
- Five 1920x1080 smoke captures were checked: day and night weapon pickups/HUD, cars and helicopter alongside the standing player, seated car driver, and helicopter flight.
- The final helicopter capture checks the adjusted pilot seat; all item crops exclude neighboring atlas fragments.
- All 20 PNGs were checked for RGBA format, transparent and opaque alpha, and manifest dimensions. Native source pixels are preserved.
- Root and build executables match SHA256 `23374A95C58AAE1F934D7553EDD9D4826FB33C6070DC0B635D6E7510591C3F72`.

Icons use built-in image generation. Their original source, exact prompt, native dimensions, and crop provenance are in `assets/icons/weapons/`. Vehicle motion remains arcade-oriented. These scripted checks and captures do not replace a full human mission playthrough.
