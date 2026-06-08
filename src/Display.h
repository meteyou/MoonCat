#pragma once

enum class DisplayView { Status };

void display_begin();
void display_setView(DisplayView view);
void display_render();
