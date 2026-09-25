# THEME_CAPS: the capability level, and the ledger of what each level added

What this firmware understands of a theme, as one number a designer's tool can ask for.

The version string is no use for this. FW_VERSION tracks releases and sat at 1.3.24
across several new theme settings, so a design tool comparing versions would have said
"up to date" about an Orb that silently dropped half of what it was sent. That is not a
hypothetical: it cost an afternoon chasing a sweep hand that would not move behind the
aircraft, on an Orb whose firmware simply had no idea the setting existed.

So: bump this by one whenever the firmware learns to read a NEW theme setting, and add a
line to the ledger. Never renumber, never reuse. The ledger says which setting needs which
level, and an Orb refuses a design it would not honour rather than letting it look
installed.

Firmware older than this constant reports no caps field at all, which a tool should read
as level 0: assume nothing, verify nothing.

## Ledger

Every new theme capability bumps `THEME_CAPS` in `src/theme/core/theme_style.h` and adds an entry here that says what
it is and what an Orb below that level does. `tests/test_theme_caps_ledger.py` fails when the newest entry and the
constant disagree. Levels 45 to 48 have no entry here: they were documented beside the fields that added them
(search `theme_style.h` for `THEME_CAPS 47`).

```
  1  radar layer order, keep-out areas, synthesised test traffic, and sweep/aircraft
     rotation pivots as theme data
  2  the menu's per-slot show flag: a design can drop the previous/next hints and keep
     only the centred app name
  3  the clock's two live text banners (text1/text2), drawn from the theme's own show
     flag instead of whichever CUSTOM_HAS_TEXT{1,2} a past firmware push happened to
     compile in
  4  hand shadows cast by a FIXED light: a separate pre-blurred silhouette sprite per
     hand, drawn at the same angle as the hand but offset in SCREEN space, so the shadow
     falls the same way whatever hour it is. A theme baking its shadow into the hand
     sprite instead needs nothing from the firmware and still works below this.
  5  the flight tracker's own furniture, all of it added in one sitting and all of it
     invisible to an Orb below this level:
       - the selection card (radar_card.png plus the `card` block) and the readout lines'
         runtime show/onCard flags. A line laid out for the card reads its across/down as
         offsets from the card's centre, so on older firmware it does not merely lose the
         card, it stacks near the middle of the dial with nothing behind it.
       - the sweep's own trail geometry: sweepTrailWidth / sweepLeadWidth /
         sweepTrailSteps, which were 5 / 2 / 20 welded into sweep_draw_cb.
       - the map's colours: mapRoadsOn / mapRoadColor / mapRoadOpacity / mapAirportsOn /
         mapAirportColor, previously a fixed grey drawn whether or not it was wanted.
       - rangeKm: how far the rim is. Launch Kit could set this only by recompiling
         (CUSTOM_RADAR_RANGE_KM), so a files-only theme had no way to state it at all.
  6  the rings and crosshair as their OWN etched plate (radar_rings.png), drawn above
     the map instead of baked into the background beneath it. Scope furniture could not
     stay readable over busy roads while it lived under them. An Orb below this level
     finds no such asset and draws a background with no rings on it at all, which is
     why `ringsPlate` below exists to be refused rather than silently dropped.
  7  mapRoadWidth: how heavy the roads are drawn. roads_sd::draw has always taken a
     width and every call site passed a literal 1, so a theme could pick the roads'
     colour and opacity but never their weight. An Orb below this draws hairlines.
  8  the sweep's own hub: a disc at the pivot the hand turns about, with its own colour,
     size and glow. The sweep drew lines out of a bare centre and the only thing ever at
     the middle of the dial was the aircraft layer's centre mark, which belongs to the
     aircraft and travels with them through the stack.
  9  the Headlines screen as theme data: background colour, the four text colours, how
     many headlines (1..5), and which topic/source the gateway is asked for. All of it
     was a fixed black screen with four hardcoded colours and always exactly three
     general-topic BBC headlines below this level, because intel_view.cpp had never
     once read anything from a theme.
 10  the Headlines screen's text boundary: square (the original fixed-width wrap) or
     curved, where each row's wrap width is the chord of a circle at that row's height
     instead of one constant. curveRadius picks which circle: small numbers pull the
     margins in hard near the top and bottom rows, large numbers approach the same
     straight-sided wrap square mode has always used. An Orb below this level has no
     concept of a curved boundary at all and wraps every theme at the original fixed
     width, which is exactly square mode's behaviour, so nothing regresses for it.
 11  the Headlines screen as a composed screen rather than a fixed layout: the title is
     its own text (any words, own size, own position, hideable), headlines can pick a
     type size from the compiled set instead of the automatic 14/16, the text block has
     real left/right margins, the poll interval is the theme's to choose, and the
     "just now" age line is a placeable, hideable field like every other line of text
     in the theme tool, with its own colour, type size and glow. A size that overflows the dial
     scrolls: press enters scroll mode, turn steps through the headlines, press again
     or six idle seconds releases — the same knob grammar the Flight Tracker's aircraft
     selection already taught. An Orb below this level draws the fixed INTEL layout it
     always has, whatever is set here.
 12  the Headlines screen finished: the full compiled size ladder (12..48 rather than
     the six sizes level 11 shipped with), a vertical offset that moves the whole
     headline block without touching the title or the age line, and a headline that
     does not fit its rows FADING OUT at the bottom instead of ending in an ellipsis.
     The scroll indicator also stopped being a column of dots beside the text, where it
     read as stray punctuation, and became a row along the bottom shown only while the
     knob is actually scrolling. An Orb below this level cannot draw the larger faces at
     all (they are compiled glyph bitmaps, not scalable outlines), which is why this is
     a level rather than a graceful fallback.
 13  the Headlines screen's typography: leading between a headline's own two lines
     (lineGap), an overrun marked by fading the RIGHT END of the last line rather than
     its underside, and the age line as a format string with a {t} token so a theme can
     write "Last updated: 5 min ago" or "5 min ago, last checked" instead of the bare
     phrase. The underside fade was the wrong shape for the job: it read as the second
     line failing to render rather than as the sentence continuing.
 14  the Headlines screen's own artwork: a background picture (intel_plate.png) and the
     shared glass/CRT overlay (intel_overlay.png), decoded by plate_sprite.cpp with the
     same flash-then-SD order every other screen uses. It was colour-only before this
     because no decoder for it existed, and offering the picker in the theme tool anyway would
     have installed a setting the device silently ignored. A theme that ships neither
     file still gets the flat background colour, so nothing older changes.
 15  the Headlines screen's typography and framing finished: its own typeface per text
     slot (title, headline, source, age) shipped as theme fonts like every other screen
     already had, an arc option for the age line, top and bottom margins for the
     headline band, up to twenty headlines held rather than five, and an explicit
     how-many-on-screen separate from how-many-fetched. An Orb below this draws the
     compiled face, keeps the line straight, and holds five.
 16  the splash became a real screen rather than a flat picture. Its three standing
     lines, the firmware version, the config address and the data credits, moved out of
     hardcoded offsets in settings_view.cpp and into splash_style.json, so a theme places
     and styles them like any other text: position, size from the ladder, colour, glow,
     alignment, and an arc. They have no show/hide, on purpose: the firmware version has to
     stay readable off the device, and the map credit is required by
     OpenStreetMap's ODbL rather than offered as a courtesy.
     This level also moves the glass OUT of splash.png. The picture used to have the
     overlay painted into it in the browser, which put the glass under anything the
     firmware drew afterwards; it is now composited on top like every other screen's,
     reusing clock_overlay.png rather than baking a second copy. An Orb below this level
     keeps the compiled offsets, and a THEME above it that lands on an older Orb simply
     does not find splash_style.json, so nothing draws twice.
 17  the gap between a headline and its source credit. It was 2 px, hardcoded in three
     places, and 2 px is a statement about a compact sans: a 24 px display serif puts the
     descenders of g and y straight through the credit line beneath it. An Orb below this
     level keeps the 2 px, which is what every theme built before this asked for anyway.
 18  opacity on every piece of text a theme controls. It costs nothing to draw: both
     glyph blitters already multiply each pixel by an opacity, and the value was pinned
     at full, while LVGL labels alpha-blend for anti-aliasing whatever happens. Faint
     type is a real design tool on a screen this bright, and there was no way to ask for
     it. An Orb below this level draws every one of them at full strength.
 19  the News screen's browsing marks and its briefing. Turning the knob now moves a
     selection through the headlines instead of sliding the window, and pressing opens
     the story's own summary from the feed. An Orb below this level scrolls the way it
     always did and does nothing on a press, so the theme's selection colours and bar
     would be settings with no screen to appear on.
 20  how the News screen's headline block is set: which edge the headlines and their
     source credits line up on, and what angle the whole block sits at. All three were
     welded — centred, centred, and square to the screen — which is the right default on
     a round dial and the wrong one the moment the background art has an edge in it:
     type set against a drawn sheet of paper has to sit where the paper's own margin is
     and lie at the paper's own angle. An Orb below this level centres both and draws
     the block square, so a design set flush left on a tilted sheet would appear centred
     and level on the device with nothing saying why. LVGL has left, centre and right
     and no justify, so justify is not offered here rather than being offered and
     silently centred.
 21  a separate glow for the Settings wheel's selected row and for the rest. There was
     one, applied to every row, so the one setting that could not mark which row is
     selected was the one people reached for to do it. An Orb below this level reads only
     the shared pair, which a theme still sets as the larger of the two, so an old device
     shows one halo rather than none.
 22  a second weight for the Settings wheel's selected row, shipped as its own converted
     face (font_settings_sel.bin). Weight is baked into a font rather than something the
     device can vary, so bold-when-selected is a second file or it is nothing. An Orb
     below this level has no slot to load it into and draws every row in the one weight,
     which is what the theme looked like before anybody asked for two.
 23  the weather map as its own app, with weather_style.json: its own background, its
     own sweep and its own colours. It had none of these and was borrowing the Flight
     Tracker's sweep OBJECT outright, so it wore the Flight Tracker's artwork. An Orb
     below this level ignores the file and draws the weather map as it always did, which
     is with no sweep at all.
 30  the weather map's data credit wearing the theme: where it sits, its colour, and the
     pill behind it. UX-030 always allowed a credit to be restyled and only forbade
     removing it; this was read as "leave it alone entirely", which left a white-on-black
     chip sitting on top of designs that had composed everything else. There is no show
     switch and text opacity has a floor, because "invisible" is how a removal would be
     spelled if the field allowed it. An Orb below this level draws the credit exactly
     where it always did, which is the same credit in a different place.
 29  {age} and {ageMin} on the weather map: how old the picture currently on the glass
     is, stepping with the animation rather than sitting on the weather data's slower
     clock. Its own level rather than folded into 28 because 28 shipped without them for
     a few minutes and an Orb flashed in that window would render the token as a gap,
     which is exactly the silent nothing THEME_CAPS exists to turn into a refusal.
 28  the weather map's own text and its own keep-out zones. Everything the map said was
     fixed in ui.cpp: a temperature, a wind line, a range label, a centre label and a
     title, each at a hard-coded position in a hard-coded colour, so a design could
     restyle the map underneath them and not move one word on top of it. It now carries
     four text slots of the same shape the Flight Tracker has, filled from a {token}
     table (see wx_text_refresh in ui.cpp for the list), and its own zones, which restore
     the theme's plate wherever the map is told not to draw. An Orb below this level
     ignores both keys and draws the fixed labels exactly as it always did, which is also
     what any Orb does when a theme defines no slots: absence means no opinion, not off.

     Two things are deliberately NOT slots: the RainViewer credit, because a data source
     credit is not a theme's to remove, and the status line, because the screen has to be
     able to say a feed is loading or dead.
 27  three things the weather map was offered in the theme tool and never given, plus a
     background picture for it and for the Stock Ticker. Its bg colour was never read at
     all, so the map inherited whatever sat behind it and stayed black however a design
     set it. Its sweepSpeed was never read either: both sweeps shared one angle, so the
     slider moved the Flight Tracker's hand or nothing. And neither screen could carry a
     plate. An Orb below this level keeps a black weather map at the Flight Tracker's
     sweep speed and ignores both plates.
 33  a text background behind any line of text on the device, not just the weather map's
     data credit. Every text card in the theme tool now asks the same question: sit on a
     background, or curve to the dial. The credit was the only line that had ever been
     offered a plate, which made it the odd one out on the one screen where somebody
     would notice. bgOpa defaults to 0, so this changes nothing about how an existing
     theme draws. An Orb below this level draws every line without its background.
 32  the weather map's range rings gain the controls the Flight Tracker's have had all
     along: how many, how thick, how strong, and a crosshair. An Orb below this level
     draws the three fixed circles it always did.
 31  the weather map's data credit can curve, on the same arc renderer every other text
     element on the device uses. Only when the pill is off: a rounded rectangle is not a
     shape that survives the line bending. An Orb below this level draws it straight.
 26  the Stock Ticker: a watchlist the theme carries, a focused readout, and a strip
     that can run along the bottom or bend around the bezel. An Orb below this level has
     no such app and ignores ticker_style.json entirely.
 25  the weather map's rings honoured at last: colour (behind its own switch, see
     ringColorOn) and the on/off toggle. Both were in the theme and in the theme tool, and
     the firmware read neither: the rings were the built-in palette's accent, which is
     the Flight Tracker's phosphor, on a screen that is supposed to be its own app. An
     Orb below this level keeps drawing them in the accent and cannot switch them off.
 24  a coastline on the weather map, and a road colour that is finally the theme's own.
     The weather map drew roads at a hard-coded grey and had no coastline at all, so a
     theme could set roadColor and roadsEnabled and watch neither do anything. An Orb
     below this level draws no coastline and keeps the fixed grey.
 34  the Flight Tracker's centre dead zone as theme data (deadZonePx), and the end of the
     compile-time overrides that sat on top of the scope's other operational values. The
     dead zone was the last of the six with no key at all: CUSTOM_RADAR_DEADZONE_PX only,
     set by recompiling, with no device-side control either, so a files-only theme could
     not ask for one and could not work around not having one.

     The other half of this level is a removal, and it is the part worth reading. Range
     and max-aircraft already had keys and already won, because their macros ran inside
     loadSettings() before applyThemeSettings(). Hide-ground and min-altitude had keys
     that were read, applied, and then unconditionally overwritten sixty lines later by
     their macros — a theme could state both, have both parsed correctly off the card,
     and fly neither. All five macros are gone now rather than reordered.

     An Orb below this level ignores deadZonePx and draws no dead zone unless one was
     welded into its firmware, which is exactly what every theme built before this got.
 35  the theme's name and author as a fourth standing line on the splash (Splash.theme),
     read from theme.json's "name" and "author". UX-028 has always listed both as what
     that screen must carry, and CUT-07 says so by number; the splash carried the version
     and the credits and neither of these. An Orb below this draws nothing where the line
     sits, so every design is refused to it, the way the glass clause at 16 does, rather
     than promising a line the device would not draw.
 36  the config address and the theme line can be switched off (SplashText.show, read for
     those two only). The owner asked for it: the address is the Orb's own web page, which is
     useful and not required, and on a splash designed as a picture it is clutter. The
     version and the credits deliberately never read the flag, so a design cannot remove
     what UX-028 and UX-030 say must be there. An Orb below this draws both lines whatever
     the design says, so a design that turned one off is refused.
 37  a virtual mainspring on the clock (Clock.windOn/windHours/windSound/windNotice). The
     clock runs down over a set number of hours, stops its hands, says so in words, and is
     wound again with five turns of the knob. The owner asked for it after the owner's vintage radio,
     where the AM static between stations turned out to be the thing people talked about:
     a small sensory detail that asks something of you is what makes an object feel alive.
     An Orb below this level ignores all four keys and simply never runs down, which is a
     theme quietly losing its character rather than drawing something wrong, so it is
     warned about instead of refused.
 38  the same mainspring, with its duration in SECONDS (windSecs) rather than hours.
     Level 37 shipped windHours and lived about an hour: the owner asked for a ten second and a
     one minute setting, which no whole number of hours can say, and those two are what
     make the feature testable at all rather than a two day wait per attempt. The level is
     spent rather than the key quietly reused because an Orb on 37 reports a mainspring it
     has and then cannot read the only field that says how long it runs, which is the tool
     lying about what the device agreed to.
 39  sounds a theme brings with it: wind.pcm for the winding click and chime.pcm for the
     hour, raw PCM at the format audio.cpp already streams, converted in the browser where
     there is a real audio stack rather than on a chip that has no business parsing an MP3.
     The owner's, and from the same place the mainspring came from: the detail people talked
     about on the owner's vintage radio was a SOUND, and it belonged to that object rather than to
     a settings menu. A Steam Punk clock and an Aviator chronometer have no more business
     clicking alike than sharing a typeface. An Orb below this level uses its built-in tick
     and its built-in chime, so a design that shipped either is refused.
 40  how many turns of the knob a full wind takes (Clock.windTurns). Five was a constant in
     the firmware, and it is a FEEL rather than a fact: a pocket watch and a chronometer
     should not ask for the same effort. The Orb builds the sentence on its own screen from
     this number, so the words and the gesture cannot drift apart. An Orb below this level
     always asks for five, whatever the design says.
 41  the wind screen as a design rather than a fixed panel: its background, the colour and
     weight of the gauge round the rim, and the words, size, colour and position of its
     three lines. It shipped white on black in the built-in face because it was written
     alongside the no-SD notice and inherited that screen's rules. It is not that kind of
     screen: it appears during ordinary use on a themed clock, and a Steam Punk Orb asking
     to be wound in a factory-looking grey sans is the seam showing. An Orb below this
     level draws the fixed panel whatever the design says.
 42  the wind screen's three lines get a TYPEFACE and margins: font_wind_{title,ask,turns}.bin
     and a left/right band each, the same pair every other text element on the device has.
     41 made the screen a design and left it in the built-in face, which is half a screen by
     the standard docs/adding-a-screen.md sets, and the half that shows. An Orb below this
     level draws the built-in face at the compiled sizes and wraps where it always did.
 43  whether the clock's two text banners draw over the hands or under them
     (Clock.textOverHands). Always under, until now, although the design tools showed the
     opposite, so the one place a designer looks to answer "what is on top" disagreed with
     the glass; this is the control it was pretending to be. An Orb below this level
     draws the hands over the words whatever the design says.
 44  the wind screen gets a background picture, switches for its gauge and each of its
     three lines, and a crank that turns with the knob (wind_bg.png, wind_crank.png). 41
     made it a design and 42 gave it type; this is the rest of what a screen has. An Orb
     below this level draws no picture and no crank, and shows the gauge and all three
     lines whatever the design says.
 49  the News screen's briefing gets a typeface and size of its own (font_intel_brief.bin,
     Intel.briefSize) and a Back button at the foot of the band. It read in the source
     credit's face at the credit's size, which is a caption size, and the owner could neither
     see the story screen in the theme tool nor change how it read. The Back button answers
     the other thing that was asked for: a press has always closed the story, and nothing on
     the screen said so. Also at this level, though it needs no key: the bake now carries
     every font slot, so the Headlines, Ticker, Weather and wind screens draw the theme's
     typeface on the device for the first time (see theme_art_bake.cpp). An Orb below
     this level reads the story in the credit's face, shows no Back button, and closes on
     a press exactly as before. (49 shipped a briefBackOn switch for a day; 50 removed
     it. The owner: "wouldn't you always want to have the back button?" Yes.)
 50  the News screen's marks are placeable: the "more below" chevron under the headlines
     (Intel.morePlace/moreX/moreY), the Back button on the story (backPlace/backX/backY)
     and the story's own "more below" chevron (briefMorePlace/briefMoreX/briefMoreY),
     each either worked out from the band as before or set to a screen coordinate. The
     story also gained the chevrons the list already had: a long story scrolls, and
     nothing said so. An Orb below this level places all three itself and draws no
     chevrons on the story.
 51  the story can hide the News screen's title while it is open (Intel.briefHideTitle),
     so a design can show just the headline and the story. And, no key: the headline
     band stops following the title and the updated line when those are moved. It was
     worked out from wherever they sat, so dragging the updated line down to the bezel
     pushed the headlines after it (the owner: "when I move the update line it screws the
     newsfeed up"). The band now sits where the two lines sit BY DEFAULT unless the
     design sets its own margins, which is the control that was always meant for that.
     An Orb below this level keeps the title up over a story and still moves the band.
 52  fonts as named faces. theme.json's `fonts` maps a text slot to the file it loads, so
     slots that share a typeface and size load one face once (one lv_font_load, one copy in
     PSRAM) instead of one each. The bake also stores the map in flash (`fonts.map`),
     because theme.json is read from the card only and an Orb with no card must still know
     which face each slot loads. An Orb below this level ignores the map and loads
     font_<slot>.bin, so a theme that ships only shared faces draws the compiled face in
     those slots.
 53  colour roles. theme.json's `palette` picks bg, primary, secondary and text; the firmware derives seven
     more (muted, dim, hairline, panel, highlight, onPrimary, alert) and resolves "$role" strings in the style
     files to those colours. Unless theme.json says roleDefaults:false, every colour option a theme leaves out
     takes its default from a role, so a theme can be only a palette. With no theme active the Orb draws the
     built-in palette. An Orb below this level ignores the palette and reads "$role" as a wrong-typed value, so
     such a colour keeps its compiled default.
 54  the app picker and Settings are one wheel, fixed in the firmware. The theme's `settings:` and `menu:`
     blocks are gone, along with the font slots menu_current, menu_prev, menu_next, settings and settings_sel
     (replaced by wheel_sel and wheel_item), the highlight pill, and Settings' default selection. The selected
     row takes palette `primary` and its glow, every other row `muted`. An Orb below this level draws its own
     picker and Settings from the blocks it still reads; a theme built for 54 ships neither.
```
