#include "theme_palette.h"

#include <initializer_list>

using namespace theme_roles;

namespace theme_palette {
namespace {

// The readout lines a screen draws for the selected item, in order: the name, two facts, a note.
constexpr Role LINE_COLOUR[4] = { R_text, R_primary, R_secondary, R_muted };

template <typename Slot>
void lines(Slot (&s)[4], const Palette &p) {
    for (int i = 0; i < 4; ++i) {
        s[i].color     = p.v[LINE_COLOUR[i]];
        s[i].glowColor = p.v[R_primary];
        s[i].bg        = p.v[R_panel];      // only visible if the theme gives the line a plate (bgOpa > 0)
    }
}

} // namespace

void apply_role_defaults(const Palette &p,
                         theme_style::Clock &clock, theme_style::Radar &radar, theme_style::Weather &weather,
                         theme_style::Ticker &ticker, theme_style::Menu &menu, theme_style::Settings &settings,
                         theme_style::Splash &splash, theme_style::Intel &intel) {
    const uint32_t bg = p.v[R_bg], primary = p.v[R_primary], secondary = p.v[R_secondary], text = p.v[R_text],
                   muted = p.v[R_muted], dim = p.v[R_dim], hairline = p.v[R_hairline], panel = p.v[R_panel],
                   highlight = p.v[R_highlight], alert = p.v[R_alert];

    // ---- Clock. The winding screen's colours, the two text banners, and the layout a DRAWN face needs: all three
    // hands, in order, ticking once a second with no shadow. A theme with images states its own hands and wins.
    clock.bg = bg;
    clock.windBg = bg;
    clock.windRingTrack = hairline;
    clock.windRingFill = primary;
    clock.windTitleCol = text;
    clock.windAskCol = muted;
    clock.windTurnsCol = dim;
    for (theme_style::ClockText *t : { &clock.text1, &clock.text2 }) {
        t->color = text;
        t->glowColor = primary;
        t->bg = panel;
    }
    for (int i = 0; i < 3; ++i) clock.hand[i].show = true;
    clock.hand[3].show = false;
    clock.hand[4].show = false;
    clock.orderN = 3;
    clock.order[0] = 0;
    clock.order[1] = 1;
    clock.order[2] = 2;
    clock.secondSweep = false;
    clock.shadowOn = false;

    // ---- Flight Tracker
    radar.sweepColor = primary;
    radar.sweepLeadColor = text;
    radar.blipFixedColor = primary;
    // Altitude, low to high: a ramp through the palette rather than six unrelated hues.
    radar.blipAltGround = muted;
    radar.blipAltLow = secondary;
    radar.blipAltMid = mix(secondary, primary, 50);
    radar.blipAltHigh = primary;
    radar.blipAltCruise = mix(primary, text, 40);
    radar.blipAltJet = text;
    radar.blipGlowColor = text;
    radar.selColor = secondary;
    radar.selGlowColor = secondary;
    radar.offRangeColor = muted;
    radar.centerColor = primary;
    radar.centerInnerColor = bg;
    radar.mapRoadColor = dim;
    radar.mapAirportColor = muted;
    radar.sweepHubColor = primary;
    radar.sweepHubGlowColor = primary;
    radar.overlayColor = bg;                // off unless a theme enables the overlay; a tint of the background
    radar.card.color = panel;
    radar.card.borderColor = primary;
    lines(radar.rtext, p);

    // ---- Weather map
    weather.bg = bg;
    weather.sweepColor = secondary;
    weather.sweepLeadColor = text;
    weather.ringColor = hairline;
    weather.roadColor = dim;
    weather.coastColor = dim;
    lines(weather.text, p);
    weather.credit.color = muted;
    weather.credit.bg = bg;

    // ---- Stock ticker
    ticker.bg = bg;
    ticker.upColor = primary;
    ticker.downColor = alert;
    ticker.flatColor = muted;
    ticker.nameColor = muted;
    ticker.priceColor = text;
    ticker.stripColor = text;

    // ---- App switcher
    menu.current.color = text;
    menu.prev.color = muted;
    menu.next.color = muted;
    for (theme_style::MenuText *t : { &menu.current, &menu.prev, &menu.next }) t->glowColor = primary;

    // ---- Settings
    settings.selColor = primary;
    settings.itemColor = muted;
    settings.glowColor = primary;
    settings.selGlowColor = primary;
    settings.itemGlowColor = muted;
    settings.hlColor = highlight;

    // ---- Splash: the version, the network line, the credits and the theme's name
    splash.version.color = text;
    splash.network.color = muted;
    splash.credits.color = dim;
    splash.theme.color = primary;
    for (theme_style::SplashText *t : { &splash.version, &splash.network, &splash.credits, &splash.theme }) {
        t->glowColor = primary;
        t->bg = panel;
    }
    splash.theme.show = true;

    // ---- Headlines
    intel.bg = bg;
    intel.titleColor = muted;
    intel.textColor = text;
    intel.sourceColor = muted;
    intel.staleColor = alert;
    intel.selColor = text;
    intel.selBarColor = highlight;
    intel.briefColor = text;
    intel.ageColor = dim;
    intel.ageGlowColor = dim;
    intel.ageBg = bg;
}

} // namespace theme_palette
