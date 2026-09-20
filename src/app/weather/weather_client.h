#pragma once

#include "weather.h"

// Fetch current conditions and a seven-day forecast from Open-Meteo. Built on the device
// (HTTPClient) and in the simulator (libcurl).
bool weather_fetch(double lat, double lon, WeatherSnapshot &out);
