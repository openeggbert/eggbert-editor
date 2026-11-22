// SPDX-License-Identifier: GPL-3.0-only
#include "app/view_transform.hpp"

#include "check.hpp"

#include <cmath>

using namespace eggbert;

namespace {

bool near(double left, double right) {
    return std::abs(left - right) < 1e-9;
}

} // namespace

int main() {
    // World pixel to cell: 64-pixel cells, half-open, nothing outside 0..6399.
    CHECK((cellAtWorld(0, 0).value() == CellPos{0, 0}));
    CHECK((cellAtWorld(63.999, 63.999).value() == CellPos{0, 0}));
    CHECK((cellAtWorld(64, 0).value() == CellPos{1, 0}));
    CHECK((cellAtWorld(0, 64).value() == CellPos{0, 1}));
    CHECK((cellAtWorld(6399.5, 6399.5).value() == CellPos{99, 99}));
    CHECK((cellAtWorld(200, 4500).value() == CellPos{3, 70})); // asymmetric
    CHECK(!cellAtWorld(-0.001, 10).has_value());
    CHECK(!cellAtWorld(10, -0.001).has_value());
    CHECK(!cellAtWorld(6400, 10).has_value());
    CHECK(!cellAtWorld(10, 6400).has_value());

    ViewTransform view;
    view.scale = 0.5;
    view.originX = 100;
    view.originY = -40;
    CHECK(near(view.toScreenX(100), 0));
    CHECK(near(view.toScreenY(-40), 0));
    CHECK(near(view.toScreenX(300), 100));
    CHECK(near(view.toWorldX(100), 300));
    CHECK(near(view.toWorldY(30), 20));
    // Round trip.
    for (const double value : {-500.0, 0.0, 17.25, 6400.0}) {
        CHECK(near(view.toWorldX(view.toScreenX(value)), value));
        CHECK(near(view.toWorldY(view.toScreenY(value)), value));
    }
    CHECK((view.cellAtScreen(0, 20).value() == CellPos{1, 0})); // world (100, 0)
    CHECK(!view.cellAtScreen(0, 0).has_value());                // world (100, -40)

    // Zoom keeps the anchored world point under the anchor.
    const auto anchorWorldX = view.toWorldX(250);
    const auto anchorWorldY = view.toWorldY(120);
    view.zoomAbout(250, 120, 2.0);
    CHECK(near(view.scale, 2.0));
    CHECK(near(view.toWorldX(250), anchorWorldX));
    CHECK(near(view.toWorldY(120), anchorWorldY));
    view.zoomAbout(250, 120, 1000.0);
    CHECK(near(view.scale, ViewTransform::maximumScale));
    CHECK(near(view.toWorldX(250), anchorWorldX));
    view.zoomAbout(250, 120, 0.0);
    CHECK(near(view.scale, ViewTransform::minimumScale));
    CHECK(near(view.toWorldY(120), anchorWorldY));

    // Pan moves content with the pointer.
    view.scale = 0.25;
    view.originX = 400;
    view.originY = 800;
    view.panByScreen(10, -20);
    CHECK(near(view.originX, 360));
    CHECK(near(view.originY, 880));

    // Fit the world into a wide viewport: limited by height and centred.
    view.fit(0, 0, 6400, 6400, 1000, 800, 0);
    CHECK(near(view.scale, 0.125));
    CHECK(near(view.toScreenY(0), 0));
    CHECK(near(view.toScreenY(6400), 800));
    CHECK(near(view.toScreenX(0), 100));
    CHECK(near(view.toScreenX(6400), 900));
    // Fit a selection with a margin.
    view.fit(640, 1280, 128, 64, 400, 300, 22);
    CHECK(near(view.scale, (400.0 - 44) / 128));
    CHECK(near(view.toScreenX(640 + 64), 200));
    CHECK(near(view.toScreenY(1280 + 32), 150));
    // A tiny rectangle is limited by the maximum scale, still centred.
    view.fit(10, 10, 1, 1, 400, 300, 0);
    CHECK(near(view.scale, ViewTransform::maximumScale));
    CHECK(near(view.toScreenX(10.5), 200));

    // Clamp: large content cannot leave gaps; small content is centred.
    view.scale = 1.0;
    view.originX = -500;
    view.originY = 99999;
    view.clampToWorld(640, 480);
    CHECK(near(view.originX, 0));
    CHECK(near(view.originY, 6400 - 480));
    view.scale = 0.0625; // world is 400 screen pixels
    view.originX = 1234;
    view.clampToWorld(1000, 400);
    CHECK(near(view.toScreenX(0), 300));
    CHECK(near(view.toScreenX(6400), 700));
    CHECK(near(view.toScreenY(0), 0));
    return test::finish("view_transform");
}
