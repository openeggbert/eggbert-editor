// SPDX-License-Identifier: GPL-3.0-only
//
// Animation on the canvas: the clock, View -> Animate, picking a moving
// object where it is shown, and that time passing never changes a byte.

#include "app/main_window.hpp"
#include "app/world_canvas.hpp"
#include "app/world_renderer.hpp"

#include "check.hpp"
#include "fixture.hpp"
#include "generated_game.hpp"

#include <QAction>
#include <QApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace eggbert;
using namespace eggbert::test;

namespace {

QColor canvasAt(WorldCanvas *canvas, double worldX, double worldY) {
    const auto &transform = canvas->transform();
    return canvas->grab().toImage().pixelColor(
        QPoint(static_cast<int>(transform.toScreenX(worldX)),
               static_cast<int>(transform.toScreenY(worldY))));
}

} // namespace

int main(int argc, char **argv) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QApplication::setOrganizationName("eggbert-editor-tests");
    QApplication::setApplicationName("animation-ui-test");
    QSettings().clear();

    QTemporaryDir dir;
    const auto game = makeGame(dir.filePath("game"));
    auto bytes = blankLargeWorld(fxRevision13);
    put16(bytes, fxCellOffset(fxForegroundOffset, 3, 70), 68); // lava
    put16(bytes, fxCellOffset(fxForegroundOffset, 5, 70), 9);  // a plain tile
    // A fish swimming four cells to the right: no wait, 40 ticks each way.
    const auto base = fxSlotOffset(fxRevision13, 0);
    put16(bytes, base, 17);
    put16(bytes, base + 2, 40);
    put16(bytes, base + 4, 40);
    put32(bytes, base + 12, 10 * 64);
    put32(bytes, base + 16, 72 * 64);
    put32(bytes, base + 20, 14 * 64);
    put32(bytes, base + 24, 72 * 64);
    put32(bytes, base + 28, 10 * 64);
    put32(bytes, base + 32, 72 * 64);
    put16(bytes, base + 36, 1);
    put16(bytes, base + 42, 10);
    put16(bytes, base + 44, 79);
    const auto path = dir.filePath("animated.blp");
    write(path, QByteArray(reinterpret_cast<const char *>(bytes.data()),
                           static_cast<qsizetype>(bytes.size())));

    {
        MainWindow window;
        window.prompts.error = [](const QString &, const QString &) {};
        window.prompts.unsavedChanges = [](DocumentTab *) { return UnsavedChoice::discard; };
        window.resize(1300, 950);
        window.show();
        CHECK(QTest::qWaitForWindowExposed(&window));
        // Animation is on unless switched off.
        CHECK(window.animated());
        CHECK(window.action("viewAnimate") != nullptr);
        CHECK(window.action("viewAnimate")->isChecked());

        auto *tab = window.openPath(path);
        CHECK(tab != nullptr);
        tab->setProfile(GameProfile::speedyBlupi1);
        CHECK(window.loadArtwork(game.root, GameProfile::speedyBlupi1));
        QApplication::processEvents();
        auto *canvas = tab->canvas();
        CHECK(canvas->isAnimated());
        // Look closely at the cells used here.
        canvas->fitCells({2, 68}, {16, 74});
        QApplication::processEvents();

        // The clock runs by itself, 20 times a second.
        const auto before = canvas->animationTick();
        CHECK(QTest::qWaitFor([&] { return canvas->animationTick() >= before + 4; }, 3000));

        // Deterministic from here: restart and step by hand.
        window.setAnimated(false);
        CHECK(!canvas->isAnimated());
        CHECK(!window.action("viewAnimate")->isChecked());
        CHECK(!QSettings().value("view/animate", true).toBool());
        // The games' "1" and "2" pictures mark the ends of the fish's path
        // and are drawn over it, as in the games' editors.
        CHECK(canvasAt(canvas, 10 * 64 + 45, 72 * 64 + 12) == spriteColor(0, 30));
        CHECK(canvasAt(canvas, 14 * 64 + 45, 72 * 64 + 12) == spriteColor(0, 31));
        // What follows looks at the fish itself, so the paths are hidden.
        {
            auto withoutPaths = canvas->renderOptions();
            withoutPaths.showPaths = false;
            canvas->setRenderOptions(withoutPaths);
            QApplication::processEvents();
        }
        // Stored state: lava's first picture, the fish at its stored place.
        CHECK(canvasAt(canvas, 3 * 64 + 30, 70 * 64 + 30) == spriteColor(0, 68));
        // (Sampled away from the path line and the badge at its start.)
        CHECK(canvasAt(canvas, 10 * 64 + 45, 72 * 64 + 12) == spriteColor(1, 79));
        const auto stillTick = canvas->animationTick();
        QTest::qWait(150);
        CHECK_EQ(canvas->animationTick(), stillTick); // no clock while off
        canvas->advanceAnimation(5);
        CHECK_EQ(canvas->animationTick(), stillTick); // and no manual stepping either

        window.action("viewAnimate")->trigger();
        CHECK(window.animated() && canvas->isAnimated());
        CHECK(QSettings().value("view/animate", false).toBool());
        canvas->restartAnimation();
        canvas->hide(); // stops the clock; stepping by hand still works
        canvas->advanceAnimation(6);
        CHECK_EQ(canvas->animationTick(), 6u);
        // Cell (3, 70) at tick 6: (3 * 13 + 70 * 7 + 6 / 3) % 5 = 1.
        CHECK(canvasAt(canvas, 3 * 64 + 30, 70 * 64 + 30) == spriteColor(0, 69));
        CHECK(canvasAt(canvas, 5 * 64 + 30, 70 * 64 + 30) == spriteColor(0, 9));
        // The fish: one tick to set off, then 256 * time / 40 pixels.
        canvas->advanceAnimation(15); // tick 21: time 20, half way
        CHECK((canvas->shownPosition(0) == Point{12 * 64, 72 * 64}));
        CHECK(canvasAt(canvas, 12 * 64 + 45, 72 * 64 + 12) != schematicBackgroundColor());
        CHECK(canvasAt(canvas, 10 * 64 + 45, 72 * 64 + 12) == schematicBackgroundColor()); // it left
        canvas->show();
        QApplication::processEvents();

        // The Object tool picks the fish where it is shown.
        window.setAnimated(true);
        canvas->hide();
        canvas->restartAnimation();
        canvas->advanceAnimation(21);
        canvas->show();
        tab->setTool(Tool::object);
        const auto shown = canvas->shownPosition(0);
        QTest::mouseClick(canvas, Qt::LeftButton, Qt::NoModifier,
                          QPoint(static_cast<int>(canvas->transform().toScreenX(shown.x + 30)),
                                 static_cast<int>(canvas->transform().toScreenY(shown.y + 30))));
        CHECK(tab->selectedSlot().has_value() && *tab->selectedSlot() == 0);
        tab->setSelectedSlot(std::nullopt);
        tab->setTool(Tool::browse);

        // However long it runs, the document is untouched.
        canvas->advanceAnimation(5000);
        CHECK(!tab->document().isModified());
        const auto current = tab->document().currentBytes();
        CHECK(current.size() == bytes.size());
        CHECK(std::equal(bytes.begin(), bytes.end(), current.begin()));
        // An edit elsewhere leaves the fish swimming; editing the fish
        // sends it back to its start.
        const auto swimming = canvas->shownPosition(0);
        canvas->hide();
        {
            EditBuilder builder(tab->document());
            builder.setCell(SectionKind::foreground, 50, 50, 9);
            CHECK(tab->applyTransaction(builder.build("Edit")));
        }
        canvas->advanceAnimation(1);
        CHECK(!(canvas->shownPosition(0) == Point{10 * 64, 72 * 64}) ||
              (swimming == Point{10 * 64, 72 * 64}));
        {
            EditBuilder builder(tab->document());
            builder.setObjectValue(0, ObjectField::timeStopStart, 7);
            CHECK(tab->applyTransaction(builder.build("Edit object")));
        }
        canvas->advanceAnimation(1);
        CHECK((canvas->shownPosition(0) == Point{10 * 64, 72 * 64}));
        window.close();
    }
    // A new window remembers the choice.
    {
        QSettings().setValue("view/animate", false);
        MainWindow window;
        CHECK(!window.animated());
        CHECK(!window.action("viewAnimate")->isChecked());
    }
    return test::finish("animation_ui");
}
