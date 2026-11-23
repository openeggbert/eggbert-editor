// SPDX-License-Identifier: GPL-3.0-only
#include "app/qt_texts.hpp"

#include <QCoreApplication>

namespace eggbert {

namespace {

// Contexts and texts exactly as Qt 6 looks them up (qplatformtheme.cpp,
// qmessagebox.cpp, qlineedit.cpp, qwidgettextcontrol.cpp,
// qabstractspinbox.cpp, qdockwidget.cpp, qscrollbar.cpp).
constexpr QtText texts[] = {
    {"QPlatformTheme", QT_TRANSLATE_NOOP("QPlatformTheme", "OK")},
    {"QPlatformTheme", QT_TRANSLATE_NOOP("QPlatformTheme", "Save")},
    {"QPlatformTheme", QT_TRANSLATE_NOOP("QPlatformTheme", "Save All")},
    {"QPlatformTheme", QT_TRANSLATE_NOOP("QPlatformTheme", "Open")},
    {"QPlatformTheme", QT_TRANSLATE_NOOP("QPlatformTheme", "&Yes")},
    {"QPlatformTheme", QT_TRANSLATE_NOOP("QPlatformTheme", "Yes to &All")},
    {"QPlatformTheme", QT_TRANSLATE_NOOP("QPlatformTheme", "&No")},
    {"QPlatformTheme", QT_TRANSLATE_NOOP("QPlatformTheme", "N&o to All")},
    {"QPlatformTheme", QT_TRANSLATE_NOOP("QPlatformTheme", "Abort")},
    {"QPlatformTheme", QT_TRANSLATE_NOOP("QPlatformTheme", "Retry")},
    {"QPlatformTheme", QT_TRANSLATE_NOOP("QPlatformTheme", "Ignore")},
    {"QPlatformTheme", QT_TRANSLATE_NOOP("QPlatformTheme", "Close")},
    {"QPlatformTheme", QT_TRANSLATE_NOOP("QPlatformTheme", "Cancel")},
    {"QPlatformTheme", QT_TRANSLATE_NOOP("QPlatformTheme", "Discard")},
    {"QPlatformTheme", QT_TRANSLATE_NOOP("QPlatformTheme", "Help")},
    {"QPlatformTheme", QT_TRANSLATE_NOOP("QPlatformTheme", "Apply")},
    {"QPlatformTheme", QT_TRANSLATE_NOOP("QPlatformTheme", "Reset")},
    {"QPlatformTheme", QT_TRANSLATE_NOOP("QPlatformTheme", "Restore Defaults")},
    {"QMessageBox", QT_TRANSLATE_NOOP("QMessageBox", "Show Details...")},
    {"QMessageBox", QT_TRANSLATE_NOOP("QMessageBox", "Hide Details...")},
    {"QLineEdit", QT_TRANSLATE_NOOP("QLineEdit", "&Undo")},
    {"QLineEdit", QT_TRANSLATE_NOOP("QLineEdit", "&Redo")},
    {"QLineEdit", QT_TRANSLATE_NOOP("QLineEdit", "Cu&t")},
    {"QLineEdit", QT_TRANSLATE_NOOP("QLineEdit", "&Copy")},
    {"QLineEdit", QT_TRANSLATE_NOOP("QLineEdit", "&Paste")},
    {"QLineEdit", QT_TRANSLATE_NOOP("QLineEdit", "Delete")},
    {"QLineEdit", QT_TRANSLATE_NOOP("QLineEdit", "Select All")},
    {"QWidgetTextControl", QT_TRANSLATE_NOOP("QWidgetTextControl", "&Undo")},
    {"QWidgetTextControl", QT_TRANSLATE_NOOP("QWidgetTextControl", "&Redo")},
    {"QWidgetTextControl", QT_TRANSLATE_NOOP("QWidgetTextControl", "Cu&t")},
    {"QWidgetTextControl", QT_TRANSLATE_NOOP("QWidgetTextControl", "&Copy")},
    {"QWidgetTextControl", QT_TRANSLATE_NOOP("QWidgetTextControl", "Copy &Link Location")},
    {"QWidgetTextControl", QT_TRANSLATE_NOOP("QWidgetTextControl", "&Paste")},
    {"QWidgetTextControl", QT_TRANSLATE_NOOP("QWidgetTextControl", "Delete")},
    {"QWidgetTextControl", QT_TRANSLATE_NOOP("QWidgetTextControl", "Select All")},
    {"QAbstractSpinBox", QT_TRANSLATE_NOOP("QAbstractSpinBox", "&Select All")},
    {"QAbstractSpinBox", QT_TRANSLATE_NOOP("QAbstractSpinBox", "&Step up")},
    {"QAbstractSpinBox", QT_TRANSLATE_NOOP("QAbstractSpinBox", "Step &down")},
    {"QDockWidget", QT_TRANSLATE_NOOP("QDockWidget", "Float")},
    {"QDockWidget", QT_TRANSLATE_NOOP("QDockWidget", "Undocks and re-attaches the dock widget")},
    {"QDockWidget", QT_TRANSLATE_NOOP("QDockWidget", "Close")},
    {"QDockWidget", QT_TRANSLATE_NOOP("QDockWidget", "Closes the dock widget")},
    {"QScrollBar", QT_TRANSLATE_NOOP("QScrollBar", "Scroll here")},
    {"QScrollBar", QT_TRANSLATE_NOOP("QScrollBar", "Left edge")},
    {"QScrollBar", QT_TRANSLATE_NOOP("QScrollBar", "Top")},
    {"QScrollBar", QT_TRANSLATE_NOOP("QScrollBar", "Right edge")},
    {"QScrollBar", QT_TRANSLATE_NOOP("QScrollBar", "Bottom")},
    {"QScrollBar", QT_TRANSLATE_NOOP("QScrollBar", "Page left")},
    {"QScrollBar", QT_TRANSLATE_NOOP("QScrollBar", "Page up")},
    {"QScrollBar", QT_TRANSLATE_NOOP("QScrollBar", "Page right")},
    {"QScrollBar", QT_TRANSLATE_NOOP("QScrollBar", "Page down")},
    {"QScrollBar", QT_TRANSLATE_NOOP("QScrollBar", "Scroll left")},
    {"QScrollBar", QT_TRANSLATE_NOOP("QScrollBar", "Scroll up")},
    {"QScrollBar", QT_TRANSLATE_NOOP("QScrollBar", "Scroll right")},
    {"QScrollBar", QT_TRANSLATE_NOOP("QScrollBar", "Scroll down")},
};

} // namespace

std::span<const QtText> qtTexts() {
    return texts;
}

} // namespace eggbert
