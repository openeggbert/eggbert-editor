// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <string>
#include <vector>

#include "world/text_world.hpp"

namespace eggbert {

// An editable Mobile Eggbert text world: the file's text, edits that each
// replace one token (see TextWorld), and undo and redo that restore the
// exact earlier text. Nothing here knows about BLP worlds.
class TextWorldDocument {
public:
    // Stored tile numbers the editor writes: -1 (an empty entry) to 32767.
    static constexpr long long smallestTile = -1;
    static constexpr long long largestTile = 32'767;
    // Undo steps kept.
    static constexpr std::size_t historyLimit = 500;

    explicit TextWorldDocument(std::string text);

    [[nodiscard]] const TextWorld &world() const { return world_; }
    [[nodiscard]] std::string text() const { return world_.write(); }
    // True when the text differs from what was loaded or last saved.
    [[nodiscard]] bool isModified() const { return world_.write() != saved_; }
    void markSaved() { saved_ = world_.write(); }

    // Each edit is one undo step. False, changing nothing, when the token
    // does not exist, the value is out of range, or it is already stored.
    bool setTile(SectionKind layer, std::size_t x, std::size_t y, long long value);
    bool setHeaderValue(std::string_view name, long long value);
    bool setHeaderPoint(std::string_view name, Point value);
    bool setObjectValue(std::size_t index, std::string_view name, long long value);
    bool setObjectPoint(std::size_t index, std::string_view name, Point value);

    [[nodiscard]] bool canUndo() const { return !undo_.empty(); }
    [[nodiscard]] bool canRedo() const { return !redo_.empty(); }
    bool undo();
    bool redo();
    // What the next undo would take back, e.g. "Decor (3, 4)".
    [[nodiscard]] std::string undoLabel() const;

private:
    struct Step {
        std::string label;
        std::string text;
    };
    template <typename Edit> bool apply(std::string label, Edit edit);

    TextWorld world_;
    std::string saved_;
    std::vector<Step> undo_;
    std::vector<Step> redo_;
};

} // namespace eggbert
