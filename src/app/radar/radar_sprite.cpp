#include "radar_sprite.h"
#include "plate_sprite.h"

// Every Flight Tracker layer a theme can ship is one plate_sprite: pre-baked flash first (free), then the card,
// then nothing, which is a plainer screen. The compiled-in fallbacks these used to carry are gone, and so is the
// copy of the loader each one needed.
namespace {

using plate_sprite::Plate;
Plate s_plate   { "radar_plate.png",   "radar_plate" };                                   // opaque
Plate s_overlay { "radar_overlay.png", "radar_overlay", nullptr, {}, false, true };       // alpha from here down
Plate s_blip    { "radar_blip.png",    "radar_blip",    nullptr, {}, false, true };
Plate s_rings   { "radar_rings.png",   "radar_rings",   nullptr, {}, false, true };
Plate s_card    { "radar_card.png",    "radar_card",    nullptr, {}, false, true };
Plate s_static[2] = {
    { "radar_static1.png", "radar_static1", nullptr, {}, false, true },
    { "radar_static2.png", "radar_static2", nullptr, {}, false, true },
};
Plate s_sweep   { "radar_sweep.png",   "radar_sweep",   nullptr, {}, false, true };

}  // namespace

const lv_img_dsc_t *radar_custom_plate()   { return plate_sprite::get(s_plate); }
const lv_img_dsc_t *radar_custom_overlay() { return plate_sprite::get(s_overlay); }
const lv_img_dsc_t *radar_custom_blip_icon() { return plate_sprite::get(s_blip); }
const lv_img_dsc_t *radar_custom_rings()   { return plate_sprite::get(s_rings); }
const lv_img_dsc_t *radar_custom_card()    { return plate_sprite::get(s_card); }
const lv_img_dsc_t *radar_custom_sweep()   { return plate_sprite::get(s_sweep); }

const lv_img_dsc_t *radar_custom_static(int idx) {
    if (idx < 0 || idx > 1) return nullptr;
    return plate_sprite::get(s_static[idx]);
}

void radar_sprite_release() {
    plate_sprite::release(s_plate);
    plate_sprite::release(s_overlay);
    plate_sprite::release(s_blip);
    plate_sprite::release(s_rings);
    plate_sprite::release(s_card);
    plate_sprite::release(s_static[0]);
    plate_sprite::release(s_static[1]);
    plate_sprite::release(s_sweep);
}
