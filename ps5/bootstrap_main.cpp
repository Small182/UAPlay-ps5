/*
 * UAPlay PS5 early native bootstrap. Not the UAPlay UI or player.
 * Uses the GPL-3.0-or-later demo renderer from drakmor/ps5-native-app-boilerplate.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "demo_renderer.hpp"

namespace {
void draw_scene(ps5::demo::Canvas &canvas) noexcept {
    using ps5::demo::Color;
    canvas.clear(Color::background);
    canvas.rectangle(90, 240, 1740, 660, Color::panel);
    canvas.rectangle(90, 238, 1740, 6, Color::cyan);
    canvas.text(120, 95, "UAPLAY PS5", 13, Color::white);
    canvas.text(120, 315, "NATIVE BOOTSTRAP", 6, Color::cyan);
    canvas.text(120, 425, "GRAPHICS / TITLE FOLDER TEST", 4, Color::white);
    canvas.text(120, 525, "CATALOG NOT CONNECTED", 4, Color::yellow);
    canvas.text(120, 610, "VIDEO PLAYER NOT CONNECTED", 4, Color::yellow);
    canvas.text(120, 760, "DEVELOPMENT BUILD - NOT A RELEASE", 3, Color::white);
}
}

int main() {
    ps5::demo::run(draw_scene, "UAPlay PS5 bootstrap");
}
