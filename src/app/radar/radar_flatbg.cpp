// The flattened static background: the plate, rings, map and static layers composited once into a single image.
// Split out of radar_view.cpp; the shared palette, tunables and state are in radar_internal.h.
#include "radar_internal.h"

namespace radar {

// ---------------- flattened static background ----------------
// The radar's lower layers never change, but every sweep frame made LVGL
// re-blend all of them across the area the rotating hand dirties (roughly
// two thirds of the screen). Measured: ~880 ms of every second inside LVGL,
// ~6 fps, which is what made the sweep step ~2.2 deg at a time instead of turn.
//
// So they are composited ONCE here, into a single opaque image, and the plate
// object is pointed at that instead. Per frame the base then costs one plain
// copy rather than a stack of alpha blends.
//
// Which layers may be absorbed is decided by the theme's own layer order, not
// assumed: walk it from the back and take static layers until the first thing
// that moves. Anything above a moving layer has to stay live or it would be
// drawn underneath something it is supposed to cover. For Steam Punk's
// { 3, 5, 1, 2, 0, 4 } that absorbs static1 and the colour wash, and leaves
// static2 alone because it sits above the sweep on purpose.
//
// Drawing is done with LVGL's own canvas rather than hand-rolled blending, so
// the merged result is produced by the exact code path that drew the layers
// separately. Scale, opacity and centring therefore match by construction.
static lv_color_t  *s_flatBuf    = nullptr;    // PSRAM, SCREEN_W*SCREEN_H
static lv_obj_t    *s_flatCanvas = nullptr;    // offscreen only; never parented into the view
static bool         s_flatOn     = false;
static bool         s_flatTook[3] = { false, false, false };   // static1, static2, wash

// The map (roads + coastline + airports), etched. s_gridLayer re-vectors all of it on
// every frame through its draw callback — 513 polylines at the owner's location, measured at
// roughly a quarter of the radar's whole frame budget. The vectors only actually change
// when the projection does (home moved, range zoomed, airports toggled), so the layer is
// rendered ONCE into this snapshot on those events, the snapshot is baked into the
// flattened background, and the live layer is hidden. The owner's three-section architecture
// assumes the map is "etched in"; this makes that assumption true.
static lv_img_dsc_t *s_mapSnap  = nullptr;
static bool          s_mapBaked = false;
static bool          s_ringsBaked = false;   // the etching went into the flat plate, so hide the live object

void take_map_snapshot() {
    if (!s_gridLayer || !customStyled()) return;
    // The layer must be visible while it renders: snapshot drives the object's own draw
    // events, and a hidden object draws nothing, which would etch an empty map.
    show(s_gridLayer, true);
    lv_obj_update_layout(s_gridLayer);
    if (s_mapSnap) { lv_snapshot_free(s_mapSnap); s_mapSnap = nullptr; }
    s_mapSnap = lv_snapshot_take(s_gridLayer, LV_IMG_CF_TRUE_COLOR_ALPHA);
    if (!s_mapSnap) { Serial.println("[radar] map snapshot failed; live layer stays"); return; }

    // Punch the exclusion zones out of the etched map.
    //
    // Roads, coastline and airports come from three separate modules that know nothing
    // about zones, and teaching each of them to clip would mean threading zone state
    // through all three. But they have already been flattened into one RGBA image by the
    // line above — so the whole job is a single pass over that image, zeroing alpha
    // inside the zones. Every map layer gets masked at once, and the background art shows
    // through cleanly where a theme asked it to.
    //
    // Runs only when the projection changes (home moved, range zoomed), not per frame.
    if (customStyled() && theme_style::radar().zoneCount > 0) {
        // LV_IMG_CF_TRUE_COLOR_ALPHA at LV_COLOR_DEPTH 16 is 3 bytes per pixel: two of
        // colour, then the alpha byte this clears.
        const int bpp = LV_IMG_PX_SIZE_ALPHA_BYTE;
        uint8_t *px = (uint8_t *)s_mapSnap->data;
        const int w = s_mapSnap->header.w, h = s_mapSnap->header.h;
        int cleared = 0;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (!in_excluded_zone((lv_coord_t)x, (lv_coord_t)y)) continue;
                px[(y * w + x) * bpp + (bpp - 1)] = 0x00;
                ++cleared;
            }
        }
        Serial.printf("[radar] map: %d px cleared by %d exclusion zone(s)\n",
                      cleared, theme_style::radar().zoneCount);
    }
}

// The live map layer earns its keep only while there is no baked copy. Called after any
// rebuild attempt, so a failed bake degrades to the old per-frame path, never to no map.
void apply_grid_visibility() {
    if (s_gridLayer) show(s_gridLayer, !(customStyled() && s_mapBaked));
}

static void release_flat_background() {
    if (s_flatCanvas) { lv_obj_del(s_flatCanvas); s_flatCanvas = nullptr; }
    if (s_flatBuf)    { heap_caps_free(s_flatBuf); s_flatBuf = nullptr; }
    s_flatOn = false;
    s_flatTook[0] = s_flatTook[1] = s_flatTook[2] = false;
    s_ringsBaked = false;
    if (s_ringsImg && radar_custom_rings()) show(s_ringsImg, true);
}

void rebuild_flat_background() {
    s_flatOn = false;
    s_mapBaked = false;
    s_ringsBaked = false;
    s_flatTook[0] = s_flatTook[1] = s_flatTook[2] = false;
    if (s_ringsImg && radar_custom_rings()) show(s_ringsImg, true);
    if (!customStyled() || !s_plateImg) return;

    const lv_img_dsc_t *plate = radar_custom_plate();
    if (!plate) return;   // nothing opaque to build on; leave the live stack alone

    // Which layers sit below the first moving one.
    const int *order = nullptr;
    const int orderN = radarLayerOrder(&order);
    bool take[3] = { false, false, false };
    int  taken = 0;
    for (int i = 0; i < orderN; ++i) {
        const int k = order[i];
        if (k == 0 || k == 1 || k == 2) break;      // sweep / aircraft / text: stop here
        if (k == 3) { take[0] = true; ++taken; }
        else if (k == 4) { take[1] = true; ++taken; }
        else if (k == 5) { take[2] = true; ++taken; }
    }
    if (!taken && !s_mapSnap) return;   // nothing to merge; not worth 434 KB to copy the plate alone

    const theme_style::Radar &rs = theme_style::radar();
    const theme_style::RadarStatic *st[2] = { &rs.static1, &rs.static2 };
    // Recheck that the layers we picked are actually contributing; a theme can
    // list a layer in its order and then switch it off.
    bool any = (s_mapSnap != nullptr);
    for (int i = 0; i < 2; ++i) if (take[i] && st[i]->show && radar_custom_static(i)) any = true;
    if (take[2] && rs.overlayEnabled && rs.overlayOpacity > 0) any = true;
    if (!any) return;

    if (!s_flatBuf) {
        s_flatBuf = (lv_color_t *)heap_caps_malloc((size_t)SCREEN_W * SCREEN_H * sizeof(lv_color_t),
                                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_flatBuf) { Serial.println("[radar] flatten: no PSRAM, keeping live layers"); return; }
    }
    if (!s_flatCanvas) {
        // Parented to the screen but permanently hidden: it exists to own the
        // buffer and give lv_canvas_draw_* somewhere to render, never to display.
        s_flatCanvas = lv_canvas_create(lv_scr_act());
        if (!s_flatCanvas) { Serial.println("[radar] flatten: no canvas"); return; }
        lv_obj_add_flag(s_flatCanvas, LV_OBJ_FLAG_HIDDEN);
    }
    lv_canvas_set_buffer(s_flatCanvas, s_flatBuf, SCREEN_W, SCREEN_H, LV_IMG_CF_TRUE_COLOR);

    // 1. the plate, filling the canvas
    {
        lv_draw_img_dsc_t d; lv_draw_img_dsc_init(&d);
        lv_canvas_draw_img(s_flatCanvas, 0, 0, plate, &d);
    }
    // 1b. the etched map, directly on the plate — the same slot the live s_gridLayer
    //     occupies in the stack today (below the decorations and the wash).
    if (s_mapSnap) {
        lv_draw_img_dsc_t d; lv_draw_img_dsc_init(&d);
        lv_canvas_draw_img(s_flatCanvas, 0, 0, s_mapSnap, &d);
        s_mapBaked = true;
    }
    // 1c. the etched rings, over the map for the same reason they sit over it live.
    if (const lv_img_dsc_t *rings = radar_custom_rings()) {
        lv_draw_img_dsc_t d; lv_draw_img_dsc_init(&d);
        lv_canvas_draw_img(s_flatCanvas, 0, 0, rings, &d);
        s_ringsBaked = true;
    }
    // 1d. PUT THE PLATE BACK INSIDE THE KEEP-OUT AREAS.
    //
    // Here rather than on each layer, because it is the one place where everything a zone is
    // supposed to hide has been drawn and nothing it is supposed to keep has been. The map
    // arrives already punched (take_map_snapshot clears its alpha), but the rings are a
    // separate baked image and nothing was punching those, so a design with a keep-out area
    // got a clean gap in the roads with the range rings still ruled straight across it.
    //
    // Copying the plate back is simpler than teaching each layer to clip, and it cannot
    // disagree with itself: whatever was on the plate is exactly what returns. It also means
    // a layer added above this line is covered for free, and one added below is deliberately
    // not, which is the right default for decoration.
    //
    // Runs when the theme changes, not per frame.
    if (rs.zoneCount > 0 && plate && plate->header.cf == LV_IMG_CF_TRUE_COLOR &&
        plate->header.w == SCREEN_W && plate->header.h == SCREEN_H && s_flatBuf) {
        const lv_color_t *src = (const lv_color_t *)plate->data;
        int restored = 0;
        for (int y = 0; y < SCREEN_H; ++y) {
            for (int x = 0; x < SCREEN_W; ++x) {
                if (!in_excluded_zone((lv_coord_t)x, (lv_coord_t)y)) continue;
                s_flatBuf[y * SCREEN_W + x] = src[y * SCREEN_W + x];
                ++restored;
            }
        }
        Serial.printf("[radar] %d px of map and rings put back to the plate by %d keep-out area(s)\n",
                      restored, rs.zoneCount);
    }

    // 2. the decorative statics we are allowed to absorb, same transform as the
    //    live path above (zoom about the image's own centre, positioned by
    //    unscaled w/h so the visual centre lands on x,y at any scale)
    for (int i = 0; i < 2; ++i) {
        if (!take[i] || !st[i]->show) continue;
        const lv_img_dsc_t *img = radar_custom_static(i);
        if (!img) continue;
        lv_draw_img_dsc_t d; lv_draw_img_dsc_init(&d);
        d.zoom    = (uint16_t)lroundf(st[i]->scale * 256.0f);
        d.opa     = (lv_opa_t)st[i]->opacity;
        d.pivot.x = (lv_coord_t)(img->header.w / 2);
        d.pivot.y = (lv_coord_t)(img->header.h / 2);
        lv_canvas_draw_img(s_flatCanvas,
                           (lv_coord_t)(st[i]->x - (int)img->header.w / 2),
                           (lv_coord_t)(st[i]->y - (int)img->header.h / 2), img, &d);
        s_flatTook[i] = true;
    }
    // 3. the colour wash, over everything absorbed so far
    if (take[2] && rs.overlayEnabled && rs.overlayOpacity > 0) {
        lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
        d.bg_color = lv_color_hex(rs.overlayColor);
        d.bg_opa   = (lv_opa_t)rs.overlayOpacity;
        d.radius   = 0;
        lv_canvas_draw_rect(s_flatCanvas, 0, 0, SCREEN_W, SCREEN_H, &d);
        s_flatTook[2] = true;
    }

    // Swap the plate over to the merged image and retire what it now contains.
    lv_img_set_src(s_plateImg, lv_canvas_get_img(s_flatCanvas));
    show(s_plateImg, true);
    if (s_ringsBaked && s_ringsImg) show(s_ringsImg, false);
    for (int i = 0; i < 2; ++i) if (s_flatTook[i] && s_staticImg[i]) show(s_staticImg[i], false);
    if (s_flatTook[2] && s_dimLayer) show(s_dimLayer, false);
    s_flatOn = true;
    Serial.printf("[radar] flattened into the plate: map=%d rings=%d static1=%d static2=%d wash=%d\n",
                  (int)s_mapBaked, (int)s_ringsBaked, (int)s_flatTook[0], (int)s_flatTook[1], (int)s_flatTook[2]);
}

} // namespace radar
