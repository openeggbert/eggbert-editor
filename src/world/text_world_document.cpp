// SPDX-License-Identifier: GPL-3.0-only
#include "world/text_world_document.hpp"

namespace eggbert {

TextWorldDocument::TextWorldDocument(std::string text)
    : world_(TextWorld::parse(text)), saved_(std::move(text)) {}

template <typename Edit> bool TextWorldDocument::apply(std::string label, Edit edit) {
    auto before = world_.write();
    auto changed = world_;
    if (!edit(changed) || changed.write() == before) {
        return false;
    }
    world_ = std::move(changed);
    undo_.push_back({std::move(label), std::move(before)});
    if (undo_.size() > historyLimit) {
        undo_.erase(undo_.begin());
    }
    redo_.clear();
    return true;
}

bool TextWorldDocument::setTile(SectionKind layer, std::size_t x, std::size_t y, long long value) {
    if (value < smallestTile || value > largestTile) {
        return false;
    }
    return apply(std::string(layer == SectionKind::bigDecor ? "BigDecor" : "Decor") + " (" +
                     std::to_string(x) + ", " + std::to_string(y) + ")",
                 [&](TextWorld &world) { return world.setTile(layer, x, y, value); });
}

bool TextWorldDocument::setHeaderValue(std::string_view name, long long value) {
    return apply(std::string(name),
                 [&](TextWorld &world) { return world.setHeaderValue(name, value); });
}

bool TextWorldDocument::setHeaderPoint(std::string_view name, Point value) {
    return apply(std::string(name),
                 [&](TextWorld &world) { return world.setHeaderPoint(name, value); });
}

bool TextWorldDocument::setObjectValue(std::size_t index, std::string_view name, long long value) {
    return apply("object " + std::to_string(index + 1) + " " + std::string(name),
                 [&](TextWorld &world) { return world.setObjectValue(index, name, value); });
}

bool TextWorldDocument::setObjectPoint(std::size_t index, std::string_view name, Point value) {
    return apply("object " + std::to_string(index + 1) + " " + std::string(name),
                 [&](TextWorld &world) { return world.setObjectPoint(index, name, value); });
}

bool TextWorldDocument::undo() {
    if (undo_.empty()) {
        return false;
    }
    redo_.push_back({undo_.back().label, world_.write()});
    world_ = TextWorld::parse(undo_.back().text);
    undo_.pop_back();
    return true;
}

bool TextWorldDocument::redo() {
    if (redo_.empty()) {
        return false;
    }
    undo_.push_back({redo_.back().label, world_.write()});
    world_ = TextWorld::parse(redo_.back().text);
    redo_.pop_back();
    return true;
}

std::string TextWorldDocument::undoLabel() const {
    return undo_.empty() ? std::string() : undo_.back().label;
}

} // namespace eggbert
