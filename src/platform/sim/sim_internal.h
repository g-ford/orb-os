#pragma once
// What the simulator's source files share. sim_main.cpp is the entry point and the frame loop; the rest were cut out of it
// along their existing seams (the self-test, the mock host API, the SDL chrome).

int sim_selftest();   // sim_selftest.cpp: runs the headless self-test and returns the process exit code
float host_get_range_km();   // sim_main.cpp: the mock Range setting
void  sim_restart();         // sim_main.cpp: re-exec the simulator, which is what a theme change does on the device
