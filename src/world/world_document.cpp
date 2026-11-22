// SPDX-License-Identifier: GPL-3.0-only
#include "world/world_document.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <utility>

#include "world/le_reader.hpp"

namespace eggbert {

namespace {

WriteAccess accessFor(const WorldProbe &probe) {
    switch (probe.status) {
    case ProbeStatus::revisionUnavailable:
    case ProbeStatus::unknownRevision:
        return WriteAccess::unknownLayout;
    case ProbeStatus::truncated:
        return WriteAccess::truncated;
    case ProbeStatus::sizeMatches:
    case ProbeStatus::surplus:
        break;
    }
    return probe.descriptor->writePolicy == WritePolicy::losslessEdit
               ? WriteAccess::writable
               : WriteAccess::readOnlyRevision;
}

std::vector<ByteRange> protectedRangesFor(const WorldProbe &probe, std::size_t size) {
    std::vector<ByteRange> ranges;
    if (probe.descriptor == nullptr) {
        ranges.push_back({0, size});
        return ranges;
    }
    // Adjacent protected header fields are merged into one range.
    const auto protect = [&](HeaderField field) {
        const auto range = headerField(field).range();
        if (!ranges.empty() && ranges.back().end() == range.offset) {
            ranges.back().size += range.size;
        } else {
            ranges.push_back(range);
        }
    };
    protect(HeaderField::majRev);
    protect(HeaderField::minRev);
    protect(HeaderField::reserve1);
    protect(HeaderField::world);
    protect(HeaderField::reserve2);
    protect(HeaderField::reserve3);
    if (size > probe.descriptor->expectedBytes) {
        ranges.push_back(
            {probe.descriptor->expectedBytes, size - probe.descriptor->expectedBytes});
    }
    return ranges;
}

// Composes `later` onto `earlier`: a byte keeps its earliest `before` and its
// latest `after`; bytes that end where they began drop out.
std::vector<ByteChange> compose(const std::vector<ByteChange> &earlier,
                                const std::vector<ByteChange> &later) {
    std::map<std::size_t, std::pair<std::byte, std::byte>> bytes;
    for (const auto &change : earlier) {
        for (std::size_t index = 0; index < change.after.size(); ++index) {
            bytes[change.offset + index] = {change.before[index], change.after[index]};
        }
    }
    for (const auto &change : later) {
        for (std::size_t index = 0; index < change.after.size(); ++index) {
            const auto position = change.offset + index;
            const auto found = bytes.find(position);
            if (found == bytes.end()) {
                bytes[position] = {change.before[index], change.after[index]};
            } else {
                found->second.second = change.after[index];
            }
        }
    }
    std::vector<ByteChange> result;
    for (const auto &[position, pair] : bytes) {
        if (pair.first == pair.second) {
            continue;
        }
        if (result.empty() || result.back().range().end() != position) {
            result.push_back({position, {}, {}});
        }
        result.back().before.push_back(pair.first);
        result.back().after.push_back(pair.second);
    }
    return result;
}

} // namespace

std::size_t Transaction::changedByteCount() const {
    std::size_t total = 0;
    for (const auto &change : changes) {
        total += change.after.size();
    }
    return total;
}

std::string_view writeAccessReason(WriteAccess access) {
    switch (access) {
    case WriteAccess::writable:
        return "Revision 1.2 or 1.3 with a complete layout: lossless editing is available.";
    case WriteAccess::readOnlyRevision:
        return "Revisions 1.0 and 1.1 are read-only: structured view and inspection only.";
    case WriteAccess::truncated:
        return "The file is truncated: complete sections can be inspected, nothing can be "
               "written.";
    case WriteAccess::unknownLayout:
        return "The revision is not recognized, so no layout is known and nothing can be "
               "written.";
    }
    return {};
}

std::vector<ByteRange> differingRanges(std::span<const std::byte> left,
                                       std::span<const std::byte> right) {
    std::vector<ByteRange> ranges;
    const auto common = std::min(left.size(), right.size());
    std::size_t index = 0;
    while (index < common) {
        if (left[index] == right[index]) {
            ++index;
            continue;
        }
        const auto start = index;
        while (index < common && left[index] != right[index]) {
            ++index;
        }
        ranges.push_back({start, index - start});
    }
    return ranges;
}

WorldDocument::WorldDocument(std::vector<std::byte> bytes)
    : original_(std::move(bytes)), saved_(original_), current_(original_),
      probe_(probeWorld(original_)), access_(accessFor(probe_)),
      protected_(protectedRangesFor(probe_, original_.size())) {}

std::expected<void, EditFailure> WorldDocument::apply(Transaction transaction) {
    if (!isWritable()) {
        return std::unexpected(EditFailure{EditError::readOnly, 0, 0});
    }
    if (transaction.changes.empty()) {
        return std::unexpected(EditFailure{EditError::emptyTransaction, 0, 0});
    }
    const auto layoutEnd = probe_.descriptor->expectedBytes;
    std::size_t previousEnd = 0;
    for (std::size_t index = 0; index < transaction.changes.size(); ++index) {
        const auto &change = transaction.changes[index];
        const auto fail = [&](EditError error, std::size_t offset) {
            return std::unexpected(EditFailure{error, index, offset});
        };
        if (change.after.empty() || change.before.size() != change.after.size()) {
            return fail(EditError::malformedChange, change.offset);
        }
        if (index > 0 && change.offset < previousEnd) {
            return fail(EditError::unsortedOrOverlapping, change.offset);
        }
        for (const auto &range : protected_) {
            if (range.overlaps(change.range()) ||
                (change.offset >= range.offset && range.contains(change.offset))) {
                return fail(EditError::protectedRange, std::max(range.offset, change.offset));
            }
        }
        if (!rangeFits(layoutEnd, change.offset, change.after.size())) {
            return fail(EditError::outsideLayout, std::max(change.offset, layoutEnd));
        }
        for (std::size_t byte = 0; byte < change.before.size(); ++byte) {
            if (current_[change.offset + byte] != change.before[byte]) {
                return fail(EditError::staleBeforeBytes, change.offset + byte);
            }
        }
        previousEnd = change.offset + change.after.size();
    }

    write(transaction.changes, true);

    const bool merge = mergeOpen_ && transaction.mergeKey != 0 && index_ == history_.size() &&
                       index_ > 0 && cleanIndex_ != index_ &&
                       history_.back().mergeKey == transaction.mergeKey;
    if (merge) {
        auto &top = history_.back();
        top.changes = compose(top.changes, transaction.changes);
        if (top.changes.empty()) {
            history_.pop_back();
            --index_;
            mergeOpen_ = false;
        }
        return {};
    }

    history_.resize(index_);
    if (cleanIndex_ && *cleanIndex_ > index_) {
        cleanIndex_.reset();
    }
    mergeOpen_ = transaction.mergeKey != 0;
    history_.push_back(std::move(transaction));
    ++index_;
    return {};
}

bool WorldDocument::undo() {
    if (!canUndo()) {
        return false;
    }
    --index_;
    write(history_[index_].changes, false);
    mergeOpen_ = false;
    return true;
}

bool WorldDocument::redo() {
    if (!canRedo()) {
        return false;
    }
    write(history_[index_].changes, true);
    ++index_;
    mergeOpen_ = false;
    return true;
}

std::string_view WorldDocument::undoLabel() const {
    return canUndo() ? std::string_view(history_[index_ - 1].label) : std::string_view{};
}

std::string_view WorldDocument::redoLabel() const {
    return canRedo() ? std::string_view(history_[index_].label) : std::string_view{};
}

bool WorldDocument::isModified() const {
    return current_ != saved_;
}

void WorldDocument::markSaved() {
    saved_ = current_;
    cleanIndex_ = index_;
    mergeOpen_ = false;
}

std::vector<ByteRange> WorldDocument::changedRangesFromOriginal() const {
    return differingRanges(original_, current_);
}

std::vector<ByteRange> WorldDocument::unsavedRanges() const {
    return differingRanges(saved_, current_);
}

void WorldDocument::write(const std::vector<ByteChange> &changes, bool forward) {
    for (const auto &change : changes) {
        const auto &source = forward ? change.after : change.before;
        std::ranges::copy(source, current_.begin() + static_cast<std::ptrdiff_t>(change.offset));
    }
}

EditBuilder::EditBuilder(const WorldDocument &document)
    : document_(&document),
      working_(document.currentBytes().begin(), document.currentBytes().end()) {}

WorldView EditBuilder::view() const {
    return WorldView(working_, document_->probe());
}

void EditBuilder::put16(std::size_t offset, std::int16_t value) {
    const auto raw = static_cast<std::uint16_t>(value);
    working_[offset] = static_cast<std::byte>(raw & 0xFF);
    working_[offset + 1] = static_cast<std::byte>(raw >> 8);
}

void EditBuilder::put32(std::size_t offset, std::int32_t value) {
    const auto raw = static_cast<std::uint32_t>(value);
    for (std::size_t index = 0; index < 4; ++index) {
        working_[offset + index] = static_cast<std::byte>((raw >> (8 * index)) & 0xFF);
    }
}

void EditBuilder::putPoint(std::size_t offset, Point value) {
    put32(offset, value.x);
    put32(offset + 4, value.y);
}

bool EditBuilder::setCell(SectionKind layer, std::size_t x, std::size_t y, std::int16_t value) {
    const auto target = view().layer(layer);
    if (!target) {
        return false;
    }
    const auto range = target->cellRange(x, y);
    if (!range) {
        return false;
    }
    put16(range->offset, value);
    return true;
}

bool EditBuilder::setMusic(std::int16_t value) {
    if (!view().header()) {
        return false;
    }
    put16(headerField(HeaderField::music).offset, value);
    return true;
}

bool EditBuilder::setRegion(std::int16_t value) {
    if (!view().header()) {
        return false;
    }
    put16(headerField(HeaderField::region).offset, value);
    return true;
}

bool EditBuilder::setPosDecor(Point value) {
    if (!view().header()) {
        return false;
    }
    putPoint(headerField(HeaderField::posDecor).offset, value);
    return true;
}

bool EditBuilder::setDimDecor(Point value) {
    if (!view().header()) {
        return false;
    }
    putPoint(headerField(HeaderField::dimDecor).offset, value);
    return true;
}

bool EditBuilder::setBlupiPos(std::size_t index, Point value) {
    const auto &field = headerField(HeaderField::blupiPos);
    if (!view().header() || index >= field.elementCount) {
        return false;
    }
    putPoint(field.elementRange(index).offset, value);
    return true;
}

bool EditBuilder::setBlupiDir(std::size_t index, std::int32_t value) {
    const auto &field = headerField(HeaderField::blupiDir);
    if (!view().header() || index >= field.elementCount) {
        return false;
    }
    put32(field.elementRange(index).offset, value);
    return true;
}

bool EditBuilder::setName(std::span<const std::byte> text) {
    const auto &field = headerField(HeaderField::name);
    if (!view().header() || text.size() >= field.elementCount ||
        std::ranges::find(text, std::byte{0}) != text.end()) {
        return false;
    }
    std::ranges::copy(text, working_.begin() + static_cast<std::ptrdiff_t>(field.offset));
    working_[field.offset + text.size()] = std::byte{0};
    return true;
}

bool EditBuilder::setObjectValue(std::size_t slot, ObjectField field, std::int16_t value) {
    const auto pool = view().objects();
    if (!pool || objectField(field).type != WireType::int16) {
        return false;
    }
    const auto range = pool->fieldRange(slot, field);
    if (!range) {
        return false;
    }
    put16(range->offset, value);
    return true;
}

bool EditBuilder::setObjectPoint(std::size_t slot, ObjectField field, Point value) {
    const auto pool = view().objects();
    if (!pool || objectField(field).type != WireType::point) {
        return false;
    }
    const auto range = pool->fieldRange(slot, field);
    if (!range) {
        return false;
    }
    putPoint(range->offset, value);
    return true;
}

bool EditBuilder::setObjectRecord(std::size_t slot, const ObjectRecord &record) {
    const auto pool = view().objects();
    if (!pool || slot >= pool->slotCount()) {
        return false;
    }
    setObjectValue(slot, ObjectField::type, record.type);
    setObjectValue(slot, ObjectField::stepAdvance, record.stepAdvance);
    setObjectValue(slot, ObjectField::stepRecede, record.stepRecede);
    setObjectValue(slot, ObjectField::timeStopStart, record.timeStopStart);
    setObjectValue(slot, ObjectField::timeStopEnd, record.timeStopEnd);
    setObjectValue(slot, ObjectField::unknown1, record.unknown1);
    setObjectPoint(slot, ObjectField::posStart, record.posStart);
    setObjectPoint(slot, ObjectField::posEnd, record.posEnd);
    setObjectPoint(slot, ObjectField::posCurrent, record.posCurrent);
    setObjectValue(slot, ObjectField::step, record.step);
    setObjectValue(slot, ObjectField::time, record.time);
    setObjectValue(slot, ObjectField::phase, record.phase);
    setObjectValue(slot, ObjectField::channel, record.channel);
    setObjectValue(slot, ObjectField::icon, record.icon);
    setObjectValue(slot, ObjectField::unknown2, record.unknown2);
    return true;
}

bool EditBuilder::copyObjectSlot(std::size_t fromSlot, std::size_t toSlot) {
    const auto pool = view().objects();
    if (!pool) {
        return false;
    }
    const auto from = pool->slotRange(fromSlot);
    const auto to = pool->slotRange(toSlot);
    if (!from || !to) {
        return false;
    }
    if (fromSlot != toSlot) {
        std::copy_n(working_.begin() + static_cast<std::ptrdiff_t>(from->offset), from->size,
                    working_.begin() + static_cast<std::ptrdiff_t>(to->offset));
    }
    return true;
}

bool EditBuilder::hasChanges() const {
    return !std::ranges::equal(working_, document_->currentBytes());
}

Transaction EditBuilder::build(std::string label, std::uint64_t mergeKey) const {
    Transaction transaction;
    transaction.label = std::move(label);
    transaction.mergeKey = mergeKey;
    const auto current = document_->currentBytes();
    for (const auto &range : differingRanges(current, working_)) {
        ByteChange change;
        change.offset = range.offset;
        change.before.assign(current.begin() + static_cast<std::ptrdiff_t>(range.offset),
                             current.begin() + static_cast<std::ptrdiff_t>(range.end()));
        change.after.assign(working_.begin() + static_cast<std::ptrdiff_t>(range.offset),
                            working_.begin() + static_cast<std::ptrdiff_t>(range.end()));
        transaction.changes.push_back(std::move(change));
    }
    return transaction;
}

} // namespace eggbert
