# Data source — ADS-B feed

## The feed: adsb.lol (free, no key)
Independent ADS-B aggregator, **free and non-commercial**. Be a good citizen: poll gently (the firmware polls every `POLL_INTERVAL_MS`, 10 s) and send a descriptive `User-Agent` (`ORB_USER_AGENT` in `src/config.h`). The host is `ADSB_PRIMARY_HOST` in `src/config.h`.

There is **no fallback host**. airplanes.live used to be listed as the primary and adsb.lol as its fallback, but as of 2026-08-23 airplanes.live answers 403 to every request until you ask them for permission, and the nearest free alternative (opendata.adsb.fi) redirects HTTP to HTTPS, which the device cannot follow. The feed is single-sourced, and `src/config.h` explains why it is fetched over plain HTTP.

### Endpoint (position + radius)
```
GET http://api.adsb.lol/v2/point/{lat}/{lon}/{radius_nm}
```
- `lat`, `lon`: decimal degrees (our home coords).
- `radius_nm`: nautical miles (max 250). Convert from our range: `nm = km * 0.539957`.

### Response (readsb format)
JSON object; the aircraft list is under key **`ac`** (older/raw readsb files use `aircraft` — handle both). Each entry includes (keys omitted when unavailable):

| Field          | Meaning                                  | Use |
|----------------|------------------------------------------|-----|
| `hex`          | 24-bit ICAO id (may start with `~`)      | stable key / de-dupe |
| `flight`       | callsign (8 chars)                       | label |
| `lat`,`lon`    | position, decimal degrees                | project to screen |
| `alt_baro`     | barometric altitude (ft) or `"ground"`   | altitude color |
| `track`        | ground track, ° from true N             | glyph rotation |
| `true_heading` | heading, ° (fallback when no `track`)   | glyph rotation |
| `gs`           | ground speed (kt)                        | detail card |
| `baro_rate`    | vertical rate (fpm, ±)                   | V/S arrow |
| `squawk`       | transponder code                         | emergency detect (7500/7600/7700) |
| `seen_pos`     | seconds since last position fix          | stale/expiry + trail |
| `t` / `type`   | aircraft type (e.g. B738) when present   | detail card |
| `dbFlags`      | bitfield (military, etc.)                | "interesting" alerts |

### Not used: OpenSky
Now requires OAuth2 client-credentials and has tighter anonymous limits — awkward for an always-on embedded device. Keep as a documented alternative only.

## On-device math (implemented in src/core/geo.h)
For each aircraft, given home `(lat0, lon0)` and range `R_km` (outer ring):
1. **Distance** via haversine (km).
2. **Bearing** from home to aircraft (° from N, clockwise).
3. **Project** to screen: `r_px = (dist_km / R_km) * R_px_outer`; with north-up,
   `x = cx + r_px * sin(bearing)`, `y = cy - r_px * cos(bearing)`.
4. Drop aircraft beyond `R_km` (or clamp to the rim with a "beyond range" marker).
