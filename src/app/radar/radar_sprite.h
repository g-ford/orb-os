// The theme's Flight Tracker layers (plate, overlay, blip, rings, card, statics, sweep), loaded on first use
// through plate_sprite (pre-baked flash, then the card, then nothing) into buffers that radar_view.cpp shows as
// plain LVGL image objects. LVGL's own compositor blits and alpha-blends them into whatever region needs a
// redraw, so unlike the clock's raw-raster compositor there is no manual per-pixel blend loop here.
#pragma once
#include <lvgl.h>

const lv_img_dsc_t *radar_custom_plate();     // 466x466 opaque RGB565 background+rings+crosshair, or nullptr
const lv_img_dsc_t *radar_custom_overlay();   // 466x466 RGB565+alpha CRT+glass layer, or nullptr
const lv_img_dsc_t *radar_custom_blip_icon(); // aircraft-icon blip sprite (real alpha), or nullptr
const lv_img_dsc_t *radar_custom_static(int idx); // idx 0/1 — the two plain decorative overlays, or nullptr
const lv_img_dsc_t *radar_custom_sweep();     // the sweep's "image" type sprite, rotated live by radar_view.cpp, or nullptr
const lv_img_dsc_t *radar_custom_card();      // the selection card's plate art, or nullptr
const lv_img_dsc_t *radar_custom_rings(); // etched rings+crosshair, drawn OVER the map, or nullptr
void radar_sprite_release();                  // free decoded PSRAM buffers; next call re-decodes
