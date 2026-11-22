// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QPointer>
#include <QWidget>

#include "app/document_tab.hpp"
#include "app/hex_view.hpp"

class QComboBox;
class QLabel;
class QLineEdit;

namespace eggbert {

// Read-only binary inspector: full-file hex view with structural details,
// jumps, search, bookmarks and original/current comparison. Nothing here can
// change a document.
class InspectorPanel final : public QWidget {
    Q_OBJECT

public:
    enum class Compare { current, original, sideBySide };

    explicit InspectorPanel(QWidget *parent = nullptr);

    void setTab(DocumentTab *tab);
    [[nodiscard]] HexView *currentView() const { return currentView_; }
    [[nodiscard]] HexView *originalView() const { return originalView_; }
    [[nodiscard]] std::size_t cursorOffset() const { return currentView_->cursorOffset(); }
    [[nodiscard]] QString detailsText() const;

    // Programmatic navigation; does not emit offsetChosenByUser.
    void goToOffset(std::size_t offset, std::size_t length = 1);
    // Parses "1608", "0x648", "cell 3,70", "big 3,70" or "slot 137".
    bool goToText(const QString &text);
    bool goToSection(const QString &name);
    // Finds the next occurrence after the cursor, wrapping around.
    bool findNext(const QString &pattern, bool ascii);
    void setCompare(Compare compare);
    void setBytesPerRow(int count);
    // Adds or removes a bookmark at the cursor.
    void toggleBookmark();
    [[nodiscard]] std::vector<std::size_t> bookmarks() const;

signals:
    // The user moved the cursor inside the hex view.
    void offsetChosenByUser(qulonglong offset);

private:
    void refresh();
    void adoptDocument();
    void refreshDetails();
    void rebuildSections();
    void rebuildBookmarks();

    QPointer<DocumentTab> tab_;
    HexView *currentView_ = nullptr;
    HexView *originalView_ = nullptr;
    QLineEdit *jumpEdit_ = nullptr;
    QComboBox *sectionCombo_ = nullptr;
    QLineEdit *searchEdit_ = nullptr;
    QComboBox *searchMode_ = nullptr;
    QComboBox *widthCombo_ = nullptr;
    QComboBox *compareCombo_ = nullptr;
    QComboBox *bookmarkCombo_ = nullptr;
    QLabel *details_ = nullptr;
    QLabel *message_ = nullptr;
    bool rebuilding_ = false;
};

} // namespace eggbert
