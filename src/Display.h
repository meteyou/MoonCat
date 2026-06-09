#pragma once

enum class DisplayView { Status, WifiGuide };

void display_begin();
void display_render();
void display_setView(DisplayView view);
void display_setWifiAP(const char* ssid);
