// SPDX-License-Identifier: GPL-3.0-only
//
// Raw byte edits and the expert policy: an ordinary apply keeps refusing
// protected ranges; an explicit expert apply may change reserved fields and
// the suffix but never the revision field or the length; everything is a
// transaction with exact undo.
#include "world/world_document.hpp"

#include "check.hpp"
#include "fixture.hpp"

using namespace eggbert;
using namespace eggbert::test;

namespace {

Bytes bytesOf(std::initializer_list<unsigned> values) {
    Bytes result;
    for (const auto value : values) {
        result.push_back(static_cast<std::byte>(value));
    }
    return result;
}

Transaction raw(const WorldDocument &document, std::size_t offset, const Bytes &bytes) {
    EditBuilder builder(document);
    CHECK(builder.setBytes(offset, bytes));
    return builder.build("Edit bytes");
}

} // namespace

int main() {
    const auto original = patternedWorld(fxRevision13, 16); // 16 bytes after the layout
    WorldDocument document{Bytes(original)};
    CHECK(document.isWritable());
    const auto size = original.size();

    // setBytes checks only that the bytes exist.
    {
        EditBuilder builder(document);
        CHECK(!builder.setBytes(0, {}));
        CHECK(!builder.setBytes(size, bytesOf({1})));
        CHECK(!builder.setBytes(size - 1, bytesOf({1, 2})));
        CHECK(!builder.hasChanges());
        CHECK(builder.setBytes(size - 1, bytesOf({1})));
    }

    // An ordinary field: both policies accept it and it is one undo step.
    {
        const auto before = toVector(document.currentBytes());
        const auto transaction = raw(document, 222, bytesOf({7, 0})); // music
        CHECK(document.protectedPartsOf(transaction).empty());
        CHECK(document.apply(transaction).has_value());
        CHECK_EQ(document.view().header()->music(), 7);
        CHECK(document.undo());
        CHECK(toVector(document.currentBytes()) == before);
    }

    // A reserved field: refused by default, accepted as an expert edit.
    {
        const auto before = toVector(document.currentBytes());
        const auto transaction = raw(document, 4, bytesOf({0xAA, 0xBB, 0xCC, 0xDD})); // reserve1[0]
        const auto parts = document.protectedPartsOf(transaction);
        CHECK_EQ(parts.size(), std::size_t{1});
        CHECK(parts.front().offset >= 4 && parts.front().end() <= 8);
        const auto refused = document.apply(transaction);
        CHECK(!refused.has_value() && refused.error().error == EditError::protectedRange);
        CHECK(toVector(document.currentBytes()) == before);
        const auto position = document.historyIndex();
        CHECK(document.apply(transaction, EditPolicy::expert).has_value());
        CHECK_EQ(document.historyIndex(), position + 1); // one undo step
        auto expected = before;
        expected[4] = std::byte{0xAA};
        expected[5] = std::byte{0xBB};
        expected[6] = std::byte{0xCC};
        expected[7] = std::byte{0xDD};
        CHECK(toVector(document.currentBytes()) == expected);
        CHECK(document.isModified());
        CHECK(document.undo());
        CHECK(toVector(document.currentBytes()) == before);
        CHECK(document.redo());
        CHECK(toVector(document.currentBytes()) == expected);
        CHECK(document.undo());
    }

    // A change running from an editable field into a protected one: the
    // protected part is reported exactly.
    {
        const auto transaction = raw(document, 224, bytesOf({1, 0, 0xEE, 0xEE})); // region, reserve2[0]
        const auto parts = document.protectedPartsOf(transaction);
        CHECK_EQ(parts.size(), std::size_t{1});
        CHECK(parts.front() == (ByteRange{226, 2}));
        CHECK(!document.apply(transaction).has_value());
        CHECK(document.apply(transaction, EditPolicy::expert).has_value());
        CHECK(document.undo());
    }

    // The bytes after the layout.
    {
        const auto before = toVector(document.currentBytes());
        const auto transaction = raw(document, size - 3, bytesOf({1, 2, 3}));
        CHECK_EQ(document.protectedPartsOf(transaction).size(), std::size_t{1});
        CHECK(!document.apply(transaction).has_value());
        CHECK(document.apply(transaction, EditPolicy::expert).has_value());
        CHECK_EQ(document.currentBytes().size(), size); // the length never changes
        CHECK(document.undo());
        CHECK(toVector(document.currentBytes()) == before);
    }

    // The revision field is never writable: it selects the layout.
    for (const std::size_t offset : {0u, 1u, 2u, 3u}) {
        const auto before = toVector(document.currentBytes());
        const auto value = static_cast<unsigned>(before[offset]) ^ 0x55u;
        const auto transaction = raw(document, offset, bytesOf({value}));
        const auto refused = document.apply(transaction, EditPolicy::expert);
        CHECK(!refused.has_value() && refused.error().error == EditError::protectedRange);
        CHECK(toVector(document.currentBytes()) == before);
    }
    {
        // Starting inside it and running out of it.
        const auto transaction = raw(document, 3, bytesOf({0x7E, 0x7E}));
        CHECK(!document.apply(transaction, EditPolicy::expert).has_value());
    }
    CHECK(toVector(document.currentBytes()) == original);
    CHECK(!document.isModified());

    // A read-only document stays read-only for expert edits too.
    {
        WorldDocument old{patternedWorld(fxRevision10)};
        const auto transaction = raw(old, 222, bytesOf({7, 0}));
        const auto refused = old.apply(transaction, EditPolicy::expert);
        CHECK(!refused.has_value() && refused.error().error == EditError::readOnly);
        auto truncated = patternedWorld(fxRevision13);
        truncated.resize(30'000);
        WorldDocument cut{std::move(truncated)};
        const auto second = raw(cut, 222, bytesOf({7, 0}));
        CHECK(!cut.apply(second, EditPolicy::expert).has_value());
    }
    return test::finish("expert_edit");
}
