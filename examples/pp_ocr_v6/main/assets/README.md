# PP-OCRv6 example — generated assets

The overlay renders text with a 24 pt / 2 bpp CJK font. Continuous
scaling in the overlay (`transform_scale_x/y`) stretches it to each
quad's on-screen height, so no additional sizes are needed.

The current `pp_ocr_v6_cjk_24.c` here is generated from Noto Sans CJK
SC (SIL OFL) and is committed with the example. `sdkconfig.bsp.*` has
`CONFIG_PP_OCR_V6_EXAMPLE_HAVE_CJK_FONT=y`, so the out-of-the-box
build already renders CJK correctly — nothing to do.

## Regenerate (only if you want a different face)

```bash
python3 ../../tools/gen_cjk_font.py \
    /usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc \
    --size 24 --bpp 2
```

`--bpp 2` is required. At 4 bpp the rodata grows ~1.3 MB, and with
`CONFIG_SPIRAM_XIP_FROM_PSRAM=y` that copy comes straight off the
PSRAM heap — enough to push the largest free block below the overlay's
headroom guard and disable the on-screen display.

## Disable (Latin-only builds)

Run `idf.py menuconfig` → **PP-OCRv6 example → [ ] Include CJK LVGL
font for the on-screen overlay** and rebuild. The overlay falls back
to `montserrat_bold_20`; non-Latin glyphs then render as placeholders
but everything else still works.
