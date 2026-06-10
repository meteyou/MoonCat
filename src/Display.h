#pragma once

enum class DisplayView { Offline, Standby, Status, WifiGuide };

void display_begin();
void display_makeDirty();
void display_render();
void display_setView(DisplayView view);
void display_setWifiAP(const char* ssid);
void display_setMoonrakerTarget(const char* host, uint16_t port);
void display_computeView();