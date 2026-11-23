// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "world/layout.hpp"
#include "world/world_probe.hpp"
#include "world/world_view.hpp"

namespace eggbert {

// Replacement of one contiguous byte run. `before` and `after` have the same
// non-zero length; `before` is what the document held when the change was made.
struct ByteChange {
    std::size_t offset = 0;
    std::vector<std::byte> before;
    std::vector<std::byte> after;

    [[nodiscard]] ByteRange range() const { return {offset, after.size()}; }
    friend bool operator==(const ByteChange &, const ByteChange &) = default;
};

// One undoable step: semantic intent plus the exact bytes it replaces.
struct Transaction {
    std::string label;
    // Sorted by offset, non-overlapping.
    std::vector<ByteChange> changes;
    // Consecutive transactions with the same non-zero key (one drag, one
    // run of key repeats) merge into a single undo step.
    std::uint64_t mergeKey = 0;

    [[nodiscard]] bool empty() const { return changes.empty(); }
    [[nodiscard]] std::size_t changedByteCount() const;
};

enum class WriteAccess {
    writable,
    // Revisions 1.0 and 1.1 are read-only in this product.
    readOnlyRevision,
    // The input ends before the layout its revision selects.
    truncated,
    // No recognized revision, so no layout.
    unknownLayout,
};

[[nodiscard]] std::string_view writeAccessReason(WriteAccess access);

enum class EditError {
    readOnly,
    emptyTransaction,
    malformedChange,
    unsortedOrOverlapping,
    outsideLayout,
    protectedRange,
    staleBeforeBytes,
};

struct EditFailure {
    EditError error{};
    // Index of the offending change, when one change is at fault.
    std::size_t changeIndex = 0;
    // First offending byte offset, when applicable.
    std::size_t offset = 0;
};

// How strictly a transaction is checked.
enum class EditPolicy {
    // The rule for every ordinary edit: nothing in a protected range,
    // nothing after the layout.
    guarded,
    // An explicit override by the user (experimental editing): reserved
    // fields, the `world` field and the bytes after the layout may change.
    // The revision field never may, because it selects the layout, and the
    // length never changes.
    expert,
};

// Maximal runs of positions where the two equally sized inputs differ.
[[nodiscard]] std::vector<ByteRange> differingRanges(std::span<const std::byte> left,
                                                     std::span<const std::byte> right);

// Owns one world: the bytes as opened, as last saved and as currently edited.
// The length never changes and bytes change only through validated
// transactions, so everything not explicitly edited is preserved exactly.
class WorldDocument {
public:
    explicit WorldDocument(std::vector<std::byte> bytes);

    [[nodiscard]] std::span<const std::byte> originalBytes() const { return original_; }
    [[nodiscard]] std::span<const std::byte> savedBytes() const { return saved_; }
    [[nodiscard]] std::span<const std::byte> currentBytes() const { return current_; }

    // The layout is fixed for the life of the document because the revision
    // field cannot be edited.
    [[nodiscard]] const WorldProbe &probe() const { return probe_; }
    [[nodiscard]] WorldView view() const { return WorldView(current_, probe_); }
    [[nodiscard]] WorldView originalView() const { return WorldView(original_, probe_); }
    [[nodiscard]] WorldView savedView() const { return WorldView(saved_, probe_); }

    [[nodiscard]] WriteAccess writeAccess() const { return access_; }
    [[nodiscard]] bool isWritable() const { return access_ == WriteAccess::writable; }
    // Ranges no transaction may touch: revision field, reserved fields, the
    // unverified `world` field and the opaque suffix.
    [[nodiscard]] std::span<const ByteRange> protectedRanges() const { return protected_; }

    // Validates the whole transaction before changing anything. On failure
    // the document, including its history, is exactly as before.
    std::expected<void, EditFailure> apply(Transaction transaction,
                                           EditPolicy policy = EditPolicy::guarded);
    // The parts of protected ranges the transaction would change (empty for
    // an ordinary edit); what an expert edit must be confirmed for.
    [[nodiscard]] std::vector<ByteRange> protectedPartsOf(const Transaction &transaction) const;
    // Stops the next transaction from merging into the previous one.
    void endMerge() { mergeOpen_ = false; }

    [[nodiscard]] bool canUndo() const { return index_ > 0; }
    [[nodiscard]] bool canRedo() const { return index_ < history_.size(); }
    bool undo();
    bool redo();
    [[nodiscard]] std::string_view undoLabel() const;
    [[nodiscard]] std::string_view redoLabel() const;
    [[nodiscard]] std::size_t historySize() const { return history_.size(); }
    [[nodiscard]] std::size_t historyIndex() const { return index_; }
    [[nodiscard]] std::span<const Transaction> history() const { return history_; }
    // History position that equals the saved bytes; nullopt once a new edit
    // has discarded the branch that contained it.
    [[nodiscard]] std::optional<std::size_t> cleanIndex() const { return cleanIndex_; }

    // True when the current bytes differ from the last saved bytes.
    [[nodiscard]] bool isModified() const;
    // Records that the current bytes are now on disk.
    void markSaved();

    [[nodiscard]] std::vector<ByteRange> changedRangesFromOriginal() const;
    [[nodiscard]] std::vector<ByteRange> unsavedRanges() const;

private:
    void write(const std::vector<ByteChange> &changes, bool forward);

    std::vector<std::byte> original_;
    std::vector<std::byte> saved_;
    std::vector<std::byte> current_;
    WorldProbe probe_;
    WriteAccess access_ = WriteAccess::unknownLayout;
    std::vector<ByteRange> protected_;
    std::vector<Transaction> history_;
    std::size_t index_ = 0;
    std::optional<std::size_t> cleanIndex_ = 0;
    bool mergeOpen_ = false;
};

// Builds a minimal transaction from semantic edits. Edits accumulate in a
// private working copy, so later edits and reads see earlier ones; build()
// emits only the bytes that actually differ from the document.
class EditBuilder {
public:
    explicit EditBuilder(const WorldDocument &document);

    // Decoded view of the working copy, including pending edits.
    [[nodiscard]] WorldView view() const;

    // Every setter returns false, changing nothing, when its target does not
    // exist in this document (absent layer, index out of range, ...).
    bool setCell(SectionKind layer, std::size_t x, std::size_t y, std::int16_t value);
    bool setMusic(std::int16_t value);
    bool setRegion(std::int16_t value);
    bool setPosDecor(Point value);
    bool setDimDecor(Point value);
    bool setBlupiPos(std::size_t index, Point value);
    bool setBlupiDir(std::size_t index, std::int32_t value);
    // Writes the text and one terminating NUL. Bytes after the terminator
    // are left exactly as stored. Fails when the text is longer than 99
    // bytes or contains a NUL.
    bool setName(std::span<const std::byte> text);
    // Scalar object fields; fails for the three POINT fields.
    bool setObjectValue(std::size_t slot, ObjectField field, std::int16_t value);
    // POINT object fields; fails for scalar fields.
    bool setObjectPoint(std::size_t slot, ObjectField field, Point value);
    bool setObjectRecord(std::size_t slot, const ObjectRecord &record);
    // Copies all 48 raw bytes, opaque fields included, between slots.
    bool copyObjectSlot(std::size_t fromSlot, std::size_t toSlot);
    // Raw bytes at an absolute offset, anywhere in the document. Whether
    // the result may be applied is decided by WorldDocument::apply.
    bool setBytes(std::size_t offset, std::span<const std::byte> bytes);

    [[nodiscard]] bool hasChanges() const;
    [[nodiscard]] Transaction build(std::string label, std::uint64_t mergeKey = 0) const;

private:
    void put16(std::size_t offset, std::int16_t value);
    void put32(std::size_t offset, std::int32_t value);
    void putPoint(std::size_t offset, Point value);

    const WorldDocument *document_;
    std::vector<std::byte> working_;
};

} // namespace eggbert
