#pragma once
#include <functional>
#include <string>
#include <vector>
// What the simulator's source files share. sim_main.cpp is the entry point and the frame loop; the rest were cut out of it
// along their existing seams (the self-test, the mock host API, the SDL chrome).

int sim_selftest();   // sim_selftest.cpp: runs the headless self-test and returns the process exit code
float host_get_range_km();   // sim_main.cpp: the mock Range setting
void  sim_restart();         // sim_main.cpp: re-exec the simulator, which is what a theme change does on the device

using SimStep = std::function<void()>;
// sim_swipeshot.cpp: the scripted swipes of --swipeshot, one entry per step, and whether any check failed.
const std::vector<SimStep> &sim_swipe_plan(const std::string &screenshotPrefix);
bool sim_swipe_failed();
void  sim_save_frame(const char *path);   // sim_main.cpp: write the current frame to a BMP (every *shot mode uses it)
void  sim_apply_home_location(const char *name, double lat, double lon);   // sim_main.cpp: re-centre the mock radar and refetch
