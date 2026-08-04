# managed_components/ hotfixes

Small local edits to managed components pulled by IDF's Component Manager
for this example. Each is applied automatically at CMake configure time by
`apply_patches.cmake` (invoked from the top-level `CMakeLists.txt` after
`project()`), because `managed_components/` is regenerated on
`idf.py fullclean` / `set-target` / dependency version bump, so any manual
edit there is volatile.

Application is idempotent: the CMake helper greps each target for a unique
marker string introduced by the patch and skips reapplying if the marker is
already present.

Two categories:

- **Compile-time upstream bugs** (`lvgl_frogfs_typedef.patch`,
  `esp_video_v4l2_include.patch`) — pure header hygiene fixes for actual
  upstream bugs. No runtime behavior changes. Should be deletable once
  upstream ships the fix.
- **Behavior tweaks** (`pp_ocr_v6_param_copy_false.patch`) — force
  `dl::Model`'s `param_copy=false` so the OCR model params stay in flash
  instead of being copied into the PSRAM heap. Upstream default is
  `true`; this example needs `false` to fit the second rec model in DUAL
  mode. See the patch entry below for the trade-off.

## Patches

### `lvgl_frogfs_typedef.patch`

- **Target**: `managed_components/lvgl__lvgl/src/libs/frogfs/src/frogfs_format.h`
- **Upstream**: LVGL 9.5.0 (also present on `master` as of writing)
- **Symptom**: `error: unknown type name 'frogfs_entry_t'` in `frogfs_dir_t`
  / `frogfs_file_t` / `frogfs_comp_t` struct bodies.
- **Root cause**: `frogfs_format.h` uses `frogfs_entry_t` in its struct
  members, but the typedef `typedef struct frogfs_entry_t frogfs_entry_t;`
  lives in `include/frogfs/frogfs.h`, which `frogfs_priv.h` only pulls in
  *after* `frogfs_format.h` in the include chain.
- **Fix**: Add a duplicate typedef right after the `struct frogfs_entry_t`
  definition. Identical typedef, safe if upstream also fixes it.
- **When to drop**: When the LVGL managed_component ships an
  `frogfs_format.h` that already contains this typedef (or reorders the
  includes in `frogfs_priv.h`). Bump the `lvgl/lvgl` dep in
  `main/idf_component.yml`, delete this patch, and re-run
  `idf.py reconfigure`.

### `esp_video_v4l2_include.patch`

- **Target**: `managed_components/espressif__esp_video/private_include/esp_video_internal.h`
- **Upstream**: `espressif/esp_video` 1.x (current as of writing)
- **Symptom**: `error: passing argument 2 of 'video->ops->set_format' from
  incompatible pointer type [-Werror=incompatible-pointer-types]` when
  building with GCC 14 (IDF v5.5+ toolchain).
- **Root cause**: `esp_video_internal.h` declares function pointers taking
  `struct v4l2_format *`, `struct v4l2_ext_controls *`, etc., but never
  includes `linux/videodev2.h`. This worked with GCC ≤13 because the
  parameter-list forward-declaration was silently unified with the file-
  scope struct once the header was pulled in later. GCC 14 tightened this
  and now treats the parameter-list forward-decl as a distinct incompatible
  type, breaking all downstream `esp_video_ops` struct-initializer
  assignments.
- **Fix**: Include `linux/videodev2.h` at the top of
  `esp_video_internal.h` so all V4L2 types have file-scope visibility
  before the function-pointer decls.
- **When to drop**: When `espressif/esp_video` includes `linux/videodev2.h`
  (or moves the types it uses to a header it already includes). Bump the
  `espressif/esp_video` dep in `main/idf_component.yml`, delete this
  patch, and re-run `idf.py reconfigure`.

### `pp_ocr_v6_param_copy_false.patch`

- **Target**: `managed_components/espressif__pp_ocr_v6/pp_ocr_v6.cpp`
- **Upstream**: `espressif/pp_ocr_v6` (current registry version)
- **Nature**: Behavior tweak, not a bug fix — see caveat below.
- **What it does**: Expands the four `new dl::Model(...)` call sites (two
  in `Det::Det`, two in `Rec::Rec`, each with an SDCARD / FLASH branch)
  to explicitly pass `param_copy=false`. Upstream default is
  `param_copy=true`, which memcpys ~3 MB of tensor params from
  `.flash.rodata` (or the flash partition) into the PSRAM heap at model
  load. `param_copy=false` skips that copy and reads params on demand
  through the flash cache.
- **Why**: PP-OCRv6's DUAL mode loads a second rec model (`REC_S16_W640`)
  and needs the PSRAM headroom. See the trade-off table in
  `examples/pp_ocr_v6/README.md` (Configuration section).
- **Caveat — SHORT + flash_partition slows down**: with `param_copy=false`
  and the model in a flash partition (not the main firmware's rodata),
  reads go through the flash cache during inference — `det:model`
  roughly doubles vs `param_copy=true`. `SHORT + rodata + XIP=y` (the
  default) is unaffected because the PSRAM XIP mirror sits in front of
  the flash cache. Users on `SHORT + flash_partition` who care about
  speed should either turn XIP off + use flash_rodata, or edit this
  patch to flip `param_copy` back to `true`.
- **Fragility**: Unlike the two compile fixes above, this patch changes
  the shape of function call arguments — if upstream refactors these
  call sites (adds a helper, renames the param, changes constructor
  signature) the hunks will reject. The `main/idf_component.yml`
  version pin prevents upstream from silently drifting under us, but
  version bumps require re-verifying (or regenerating) this patch.
- **When to drop**: When upstream `espressif/pp_ocr_v6` either flips its
  default to `param_copy=false`, or exposes a Kconfig / constructor
  arg for it. Bump the version and delete.

## Manual re-apply / revert

The CMake helper is idempotent, so `idf.py reconfigure` re-applies as
needed. To manually apply/revert (e.g. for debugging):

```bash
cd examples/pp_ocr_v6

patch -p1 -i patches/lvgl_frogfs_typedef.patch
patch -p1 -R -i patches/lvgl_frogfs_typedef.patch
```

## Regenerating a patch

If upstream shifts around and the patch stops applying cleanly, regenerate
it against the current managed_components/ tree. Concretely: back up the
patched file, revert to the upstream version, hand-apply the fix again,
then `diff -u upstream patched > new.patch` and update the paths in the
`---`/`+++` headers to match `a/managed_components/...` /
`b/managed_components/...`.
