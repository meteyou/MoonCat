#pragma once
#include <stdint.h>

void wifi_begin();
void wifi_loop();

void wifi_factoryReset();

const char* wifi_moonraker_host();
uint16_t    wifi_moonraker_port();