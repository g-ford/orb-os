// Prints the theme_style state as JSON, in the shape of theme.yaml / the *_style.json files.
//
//   dump_theme_defaults           the firmware's own defaults: seed_defaults() with no theme
//   dump_theme_defaults <dir>     what load() makes of <dir>/theme.json and <dir>/*_style.json
//
// It links the real theme_style.cpp, so the values are whatever the firmware computes, not a
// copy of them. tools/gen_elegant_theme.py builds and runs it on a theme folder to write the
// Elegant's theme.yaml, and tests/test_elegant_theme.py runs it on the built Elegant to prove
// that file is exactly what the firmware reads. The no-argument mode is the fallback an Orb shows
// with no theme on its card, which is no longer the default theme.
//
// Colours come out as "0xRRGGBB" strings so the generator can write them as hex. A key the
// firmware only ever sets when a theme asks (a -1 "unset" sentinel) is left out of its section
// and named in "_unset" instead, because writing the -1 down would change what the firmware does.
#include "theme_style.h"
#include <ArduinoJson.h>
#include <cstdio>
#include <cstdlib>
#include <string>

static std::string g_dir;

namespace theme_select {
const char *activeSlug() { return g_dir.empty() ? "" : "dump"; }
}

namespace theme_sd {
// theme_style asks for "/themes/<slug>/<file>"; in directory mode the file comes from <dir>.
uint8_t *read_whole(const char *path, size_t &outLen, size_t maxBytes) {
    outLen = 0;
    if (g_dir.empty()) return nullptr;
    const char *leaf = strrchr(path, '/');
    FILE *f = fopen((g_dir + "/" + (leaf ? leaf + 1 : path)).c_str(), "rb");
    if (!f) return nullptr;
    fseek(f, 0, SEEK_END);
    const long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || (size_t)sz > maxBytes) { fclose(f); return nullptr; }
    uint8_t *buf = (uint8_t *)malloc((size_t)sz);
    const size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (got != (size_t)sz) { free(buf); return nullptr; }
    outLen = (size_t)sz;
    return buf;
}
void free(uint8_t *buf) { ::free(buf); }
}

using namespace theme_style;

static std::string hex(uint32_t c) {
    char b[16];
    snprintf(b, sizeof(b), "0x%06X", (unsigned)(c & 0xFFFFFF));
    return b;
}

// JSON key == struct field, which is how theme_style.cpp reads them. B plain, C colour, S text.
#define B(k) o[#k] = s.k
#define C(k) o[#k] = hex(s.k)
#define S(k) o[#k] = std::string(s.k)

static void pill(JsonObject o, uint32_t bg, int bgOpa, int radius) {
    o["bg"] = hex(bg);
    o["bgOpa"] = bgOpa;
    o["radius"] = radius;
}

static void clock_text(JsonObject o, const ClockText &s) {
    B(show); B(x); B(y); C(color); B(opa); B(glow); C(glowColor); S(fmt);
    pill(o, s.bg, s.bgOpa, s.radius);
    B(curved); B(curveR); B(arcDeg); B(align);
}

static void slot(JsonObject o, const TextSlot &s) {
    B(show); B(x); B(y); C(color); B(opa); B(glow); C(glowColor); S(fmt);
    pill(o, s.bg, s.bgOpa, s.radius);
    B(curved); B(curveR); B(arcDeg); B(align); B(onCard);
}

static void menu_text(JsonObject o, const MenuText &s) {
    B(show); B(x); B(y); C(color); B(opa); B(glow); C(glowColor); S(fmt);
    B(align); B(wrapWidth); B(lineGap); B(lineStep);
}

static void splash_text(JsonObject o, const SplashText &s, bool hideable) {
    if (hideable) B(show);
    B(x); B(y); B(size); C(color); B(opa); B(glow); C(glowColor); B(align);
    pill(o, s.bg, s.bgOpa, s.radius);
    B(curved); B(curveR); B(arcDeg);
}

static void hand(JsonObject o, const Hand &s) {
    B(show); B(pivotX); B(pivotY); B(centerX); B(centerY); B(blend);
}

static void zones(JsonObject parent, const Zone *z, int n) {
    JsonArray a = parent["zones"].to<JsonArray>();
    for (int i = 0; i < n; ++i) {
        JsonObject o = a.add<JsonObject>();
        o["x"] = z[i].x; o["y"] = z[i].y; o["r"] = z[i].r; o["w"] = z[i].w; o["h"] = z[i].h;
        o["rect"] = z[i].rect; o["invert"] = z[i].invert;
    }
}

// Left out of the section, named in "_unset": the firmware treats -1 as "the theme said nothing".
static void unset_if_negative(JsonObject o, JsonArray unset, const char *key, double v) {
    if (v < 0) unset.add(std::string(key)); else o[key] = v;
}

static void dump_clock(JsonObject o) {
    const Clock &s = theme_style::clock();   // qualified: libc has a clock() too
    C(bg); B(plateFollow); B(textOverHands); B(secondSweep);
    clock_text(o["text1"].to<JsonObject>(), s.text1);
    clock_text(o["text2"].to<JsonObject>(), s.text2);
    JsonObject h = o["hands"].to<JsonObject>();
    static const char *names[5] = { "hour", "minute", "second", "static1", "static2" };
    for (int i = 0; i < 5; ++i) hand(h[names[i]].to<JsonObject>(), s.hand[i]);
    JsonArray ord = h["order"].to<JsonArray>();
    for (int i = 0; i < s.orderN; ++i) ord.add(s.order[i]);
    JsonObject sh = h["shadow"].to<JsonObject>();
    sh["on"] = s.shadowOn; sh["dx"] = s.shadowDX; sh["dy"] = s.shadowDY;
    B(windOn); B(windSecs); B(windTurns); B(windSound); B(windNotice); B(windImageOn);
    B(windRingShow); B(windTitleShow); B(windAskShow);
    B(windCrankOn); B(windCrankX); B(windCrankY); B(windCrankPX); B(windCrankPY); B(windCrankRest);
    B(windCrankShadowOn); B(windCrankShadowDX); B(windCrankShadowDY);
    C(windBg); C(windRingTrack); C(windRingFill); B(windRingWidth); B(windRingR);
    B(windTitleML); B(windTitleMR); B(windAskML); B(windAskMR); B(windTurnsML); B(windTurnsMR);
    S(windTitle); B(windTitleSize); C(windTitleCol); B(windTitleY); B(windTitleOpa);
    S(windAsk); B(windAskSize); C(windAskCol); B(windAskY); B(windAskOpa);
    B(windTurnsShow); B(windTurnsSize); C(windTurnsCol); B(windTurnsY); B(windTurnsOpa);
}

static void dump_radar(JsonObject o) {
    const Radar &s = radar();
    JsonArray unset = o["_unset"].to<JsonArray>();
    B(sweepEnabled); B(sweepTypeImage); C(sweepColor); C(sweepLeadColor); B(sweepTrailDeg);
    B(sweepOpacity); B(sweepTrailWidth); B(sweepLeadWidth); B(sweepTrailSteps); B(sweepLength); B(sweepSpeed);
    B(sweepHubOn); C(sweepHubColor); B(sweepHubRadius); B(sweepHubGlow); C(sweepHubGlowColor);
    B(blipEnabled); B(blipTypeImage); B(blipRotate); B(blipKiteShape); B(blipSize); B(blipKiteT);
    B(blipFixedColorMode); C(blipFixedColor);
    C(blipAltGround); C(blipAltLow); C(blipAltMid); C(blipAltHigh); C(blipAltCruise); C(blipAltJet);
    B(blipGlow); C(blipGlowColor); B(blipImageTint);
    B(selEnabled); B(selStyle); C(selColor); B(selWidth); B(selDiameter); B(selGlow); C(selGlowColor);
    B(offRangeEnabled); C(offRangeColor); B(offRangeSize);
    B(centerEnabled); B(centerRadius); C(centerColor); B(centerInnerRadius); C(centerInnerColor);
    JsonArray rt = o["rtext"].to<JsonArray>();
    for (int i = 0; i < 4; ++i) slot(rt.add<JsonObject>(), s.rtext[i]);
    {
        JsonObject c = o["card"].to<JsonObject>();
        const RadarCard &k = s.card;
        c["enabled"] = k.enabled; c["typeImage"] = k.typeImage; c["radius"] = k.radius;
        c["w"] = k.w; c["h"] = k.h; c["corner"] = k.corner; c["color"] = hex(k.color);
        c["opacity"] = k.opacity; c["borderColor"] = hex(k.borderColor); c["borderWidth"] = k.borderWidth;
    }
    B(mapRoadsOn); C(mapRoadColor); B(mapRoadOpacity); B(mapRoadWidth); B(mapAirportsOn); C(mapAirportColor);
    B(ringsPlate);
    for (int i = 0; i < 2; ++i) {
        const RadarStatic &k = i ? s.static2 : s.static1;
        JsonObject c = o[i ? "static2" : "static1"].to<JsonObject>();
        c["show"] = k.show; c["x"] = k.x; c["y"] = k.y; c["opacity"] = k.opacity; c["scale"] = k.scale;
    }
    B(overlayEnabled); C(overlayColor); B(overlayOpacity);
    unset_if_negative(o, unset, "maxAircraft", s.maxAircraft);
    unset_if_negative(o, unset, "minAltFt", s.minAltFt);
    unset_if_negative(o, unset, "rangeKm", s.rangeKm);
    if (s.hideGround < 0) unset.add("hideGround"); else o["hideGround"] = s.hideGround != 0;
    unset_if_negative(o, unset, "deadZonePx", s.deadZonePx);
    B(simulate);
    unset_if_negative(o, unset, "sweepPivotX", s.sweepPivotX);
    unset_if_negative(o, unset, "sweepPivotY", s.sweepPivotY);
    unset_if_negative(o, unset, "sweepCenterX", s.sweepCenterX);
    unset_if_negative(o, unset, "sweepCenterY", s.sweepCenterY);
    unset_if_negative(o, unset, "blipPivotX", s.blipPivotX);
    unset_if_negative(o, unset, "blipPivotY", s.blipPivotY);
    if (s.orderN > 0) {
        JsonArray ord = o["order"].to<JsonArray>();
        for (int i = 0; i < s.orderN; ++i) ord.add(s.order[i]);
    } else {
        unset.add("order");
    }
    zones(o, s.zones, s.zoneCount);
}

static void dump_weather(JsonObject o) {
    const Weather &s = weather();
    C(bg); B(sweepEnabled); B(sweepTypeImage); C(sweepColor); C(sweepLeadColor); B(sweepTrailDeg);
    B(sweepOpacity); B(sweepLength); B(sweepSpeed); B(sweepTrailWidth); B(sweepLeadWidth); B(sweepTrailSteps);
    C(ringColor); B(ringColorOn); B(ringsEnabled); B(ringCount); B(ringWidth); B(ringOpacity); B(crosshair);
    C(roadColor); B(roadsEnabled); C(coastColor); B(coastEnabled);
    JsonArray wt = o["wtext"].to<JsonArray>();
    for (int i = 0; i < 4; ++i) slot(wt.add<JsonObject>(), s.text[i]);
    {
        JsonObject c = o["credit"].to<JsonObject>();
        const Weather::Credit &k = s.credit;
        c["x"] = k.x; c["y"] = k.y; c["color"] = hex(k.color); c["opa"] = k.opa;
        pill(c, k.bg, k.bgOpa, k.radius);
        c["align"] = k.align; c["curved"] = k.curved; c["curveR"] = k.curveR; c["arcDeg"] = k.arcDeg;
    }
    zones(o, s.zones, s.zoneCount);
}

static void dump_ticker(JsonObject o) {
    const Ticker &s = ticker();
    C(bg); S(symbols); B(pollSeconds); C(upColor); C(downColor); C(flatColor);
    C(nameColor); B(nameSize); B(nameY); B(nameShow);
    B(priceSize); B(priceY); B(priceShow); B(priceColorOn); C(priceColor);
    B(changeSize); B(changeY); B(changeShow); B(changePct);
    B(stripShow); o["stripPlace"] = (int)s.stripPlace; B(stripSize); C(stripColor); B(stripOpa);
    B(stripSpeed); B(stripY); B(stripRadius); B(stripAngle); B(stripUpDown);
}

static void dump_settings(JsonObject o) {
    const Settings &s = settings();
    B(wheelR); B(wheelRx); B(wheelStepDeg); B(wheelCy); B(wheelFade);
    C(selColor); B(selOpa); C(itemColor); B(itemOpa); B(glow); C(glowColor);
    B(selGlow); C(selGlowColor); B(itemGlow); C(itemGlowColor);
    B(hlShow); C(hlColor); B(hlOpacity); B(hlW); B(hlH); B(hlRadius); B(defaultSel);
}

static void dump_menu(JsonObject o) {
    const Menu &s = menu();
    menu_text(o["current"].to<JsonObject>(), s.current);
    menu_text(o["prev"].to<JsonObject>(), s.prev);
    menu_text(o["next"].to<JsonObject>(), s.next);
}

static void dump_splash(JsonObject o) {
    const Splash &s = splash();
    splash_text(o["version"].to<JsonObject>(), s.version, false);
    splash_text(o["network"].to<JsonObject>(), s.network, true);
    splash_text(o["credits"].to<JsonObject>(), s.credits, false);
    splash_text(o["theme"].to<JsonObject>(), s.theme, true);
}

static const char *align_name(int a) {
    return a == Intel::ALIGN_LEFT ? "left" : a == Intel::ALIGN_RIGHT ? "right" : "center";
}

static void dump_intel(JsonObject o) {
    const Intel &s = intel();
    C(bg); C(titleColor); B(titleOpa); C(textColor); B(textOpa); C(sourceColor); B(sourceOpa); C(staleColor);
    B(selDim); B(selColorOn); C(selColor); B(selBarOn); C(selBarColor); B(selBarOpa);
    B(selBarRadius); B(selBarPadX); B(selBarPadY);
    B(briefColorOn); C(briefColor); B(briefOpa); B(briefGap); B(briefSize); B(briefHideTitle);
    B(count); B(onScreen); S(topic); S(source); B(pollMinutes);
    B(curvedBounds); B(curveRadius);
    S(title); B(titleShow); B(titleSize); B(titleX); B(titleY);
    B(textSize); B(marginLeft); B(marginRight); B(marginTop); B(marginBottom);
    B(lineGap); B(sourceGap); B(blockOffsetY); B(blockAngle); B(sourceSize);
    o["textAlign"] = align_name(s.textAlign);
    o["sourceAlign"] = align_name(s.sourceAlign);
    B(morePlace); B(moreX); B(moreY); B(backPlace); B(backX); B(backY);
    B(briefMorePlace); B(briefMoreX); B(briefMoreY);
    B(ageShow); S(ageFmt); B(ageX); B(ageY); C(ageColor); B(ageOpa); B(ageSize); B(ageGlow); C(ageGlowColor);
    B(ageCurved); C(ageBg); B(ageBgOpa); B(ageRadius); B(ageCurveR); B(ageArcDeg);
}

int main(int argc, char **argv) {
    if (argc > 1) g_dir = argv[1];
    theme_style::load();

    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    {
        const Apps &a = apps();
        JsonObject o = root["apps"].to<JsonObject>();
        o["clock"] = a.clock; o["flight"] = a.flight; o["weather"] = a.weather;
        o["surveillance"] = a.surveillance; o["headlines"] = a.headlines; o["ticker"] = a.ticker;
    }
    {
        // names.theme and names.author are the theme's name and author, set from theme.json:
        // identity, not an option, so they are not part of the state being compared.
        const Names &s = names();
        JsonObject o = root["names"].to<JsonObject>();
        S(clock); S(flight); S(weather); S(surveillance); S(headlines); S(ticker); S(settings);
    }
    dump_clock(root["clock"].to<JsonObject>());
    dump_radar(root["radar"].to<JsonObject>());
    dump_weather(root["weather"].to<JsonObject>());
    dump_ticker(root["ticker"].to<JsonObject>());
    dump_settings(root["settings"].to<JsonObject>());
    dump_menu(root["menu"].to<JsonObject>());
    dump_splash(root["splash"].to<JsonObject>());
    dump_intel(root["intel"].to<JsonObject>());

    std::string out;
    serializeJson(doc, out);
    puts(out.c_str());
    return 0;
}
