// SPDX-License-Identifier: GPL-3.0-only
#include "inflate.hpp"

#include <QCoreApplication>

#include <array>
#include <optional>

namespace eggbert {

namespace {

constexpr int longestCode = 15;
constexpr int mostLiteralCodes = 286;
constexpr int mostDistanceCodes = 30;
constexpr int fixedLiteralCodes = 288;

// Lengths and distances of RFC 1951, section 3.2.5: base value and number
// of extra bits per symbol.
constexpr std::array<quint16, 29> lengthBase{3,  4,  5,  6,  7,  8,  9,  10, 11,  13,
                                             15, 17, 19, 23, 27, 31, 35, 43, 51,  59,
                                             67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr std::array<quint8, 29> lengthExtra{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                             2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr std::array<quint16, 30> distanceBase{1,    2,    3,    4,    5,    7,    9,    13,
                                               17,   25,   33,   49,   65,   97,   129,  193,
                                               257,  385,  513,  769,  1025, 1537, 2049, 3073,
                                               4097, 6145, 8193, 12289, 16385, 24577};
constexpr std::array<quint8, 30> distanceExtra{0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5, 6,
                                               6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
// The order in which the lengths of the code-length code are stored.
constexpr std::array<quint8, 19> codeLengthOrder{16, 17, 18, 0, 8,  7, 9,  6, 10, 5,
                                                 11, 4,  12, 3, 13, 2, 14, 1, 15};

// Bits are taken from the least significant end of each byte.
class BitReader {
public:
    explicit BitReader(const QByteArray &input) : input_(input) {}

    // The next `count` (0..16) bits, or -1 when the input ends first.
    int bits(int count) {
        while (held_ < count) {
            if (position_ >= input_.size()) {
                return -1;
            }
            buffer_ |= static_cast<quint32>(static_cast<quint8>(input_[position_++])) << held_;
            held_ += 8;
        }
        const auto value = static_cast<int>(buffer_ & ((1u << count) - 1u));
        buffer_ >>= count;
        held_ -= count;
        return value;
    }
    // Drops the rest of the current byte (before a stored block).
    void alignToByte() {
        buffer_ = 0;
        held_ = 0;
    }
    [[nodiscard]] qsizetype position() const { return position_; }
    [[nodiscard]] qsizetype remaining() const { return input_.size() - position_; }
    // Whole bytes, only valid when aligned.
    [[nodiscard]] const char *take(qsizetype count) {
        const auto *data = input_.constData() + position_;
        position_ += count;
        return data;
    }

private:
    const QByteArray &input_;
    qsizetype position_ = 0;
    quint32 buffer_ = 0;
    int held_ = 0;
};

// A canonical Huffman code: how many codes have each length, and the
// symbols ordered by code.
struct Code {
    std::array<quint16, longestCode + 1> count{};
    std::array<quint16, fixedLiteralCodes> symbol{};
};

// Builds a code from the lengths of `symbols` symbols. Returns 0 for a
// complete code, a positive number for an incomplete one and a negative
// number for an over-subscribed (impossible) one.
int build(Code &code, const quint8 *lengths, int symbols) {
    code.count.fill(0);
    for (int index = 0; index < symbols; ++index) {
        ++code.count[lengths[index]];
    }
    if (code.count[0] == symbols) {
        return 0; // no codes at all: complete, but nothing can be decoded
    }
    int left = 1;
    for (int length = 1; length <= longestCode; ++length) {
        left <<= 1;
        left -= code.count[static_cast<std::size_t>(length)];
        if (left < 0) {
            return left;
        }
    }
    std::array<quint16, longestCode + 1> offset{};
    for (std::size_t length = 1; length < longestCode; ++length) {
        offset[length + 1] = static_cast<quint16>(offset[length] + code.count[length]);
    }
    for (int index = 0; index < symbols; ++index) {
        if (lengths[index] != 0) {
            code.symbol[offset[lengths[index]]++] = static_cast<quint16>(index);
        }
    }
    return left;
}

// The next symbol, -1 when the input ends, -2 when the bits are no code.
int decode(BitReader &reader, const Code &code) {
    int value = 0;
    int first = 0;
    int index = 0;
    for (std::size_t length = 1; length <= longestCode; ++length) {
        const auto bit = reader.bits(1);
        if (bit < 0) {
            return -1;
        }
        value |= bit;
        const int count = code.count[length];
        if (value - count < first) {
            return code.symbol[static_cast<std::size_t>(index + (value - first))];
        }
        index += count;
        first += count;
        first <<= 1;
        value <<= 1;
    }
    return -2;
}

// Looked up when used, so that a translator installed after start applies.
struct Message {
    const char *text;
    operator QString() const { return QCoreApplication::translate("eggbert::Files", text); }
};
constexpr Message truncated{
    QT_TRANSLATE_NOOP("eggbert::Files", "The compressed data ends too early.")};
constexpr Message damaged{QT_TRANSLATE_NOOP("eggbert::Files", "The compressed data is damaged.")};
constexpr Message tooLong{
    QT_TRANSLATE_NOOP("eggbert::Files", "The compressed data holds more than its stated size.")};

// Literals, lengths and distances until the end-of-block symbol.
std::optional<QString> codedBlock(BitReader &reader, QByteArray &out, qint64 size,
                                  const Code &literals, const Code &distances) {
    for (;;) {
        const auto symbol = decode(reader, literals);
        if (symbol < 0) {
            return symbol == -1 ? truncated : damaged;
        }
        if (symbol < 256) {
            if (out.size() >= size) {
                return tooLong;
            }
            out.append(static_cast<char>(symbol));
            continue;
        }
        if (symbol == 256) {
            return std::nullopt;
        }
        const auto lengthIndex = static_cast<std::size_t>(symbol - 257);
        if (lengthIndex >= lengthBase.size()) {
            return damaged;
        }
        const auto more = reader.bits(lengthExtra[lengthIndex]);
        const auto distanceSymbol = more < 0 ? -1 : decode(reader, distances);
        if (distanceSymbol < 0) {
            return more < 0 || distanceSymbol == -1 ? truncated : damaged;
        }
        const auto distanceIndex = static_cast<std::size_t>(distanceSymbol);
        if (distanceIndex >= distanceBase.size()) {
            return damaged;
        }
        const auto further = reader.bits(distanceExtra[distanceIndex]);
        if (further < 0) {
            return truncated;
        }
        const qsizetype length = lengthBase[lengthIndex] + more;
        const qsizetype distance = distanceBase[distanceIndex] + further;
        if (distance > out.size()) {
            return damaged; // points before the start of the output
        }
        if (out.size() + length > size) {
            return tooLong;
        }
        // Byte by byte: the copy may overlap what it writes.
        for (qsizetype copied = 0; copied < length; ++copied) {
            out.append(out[out.size() - distance]);
        }
    }
}

std::optional<QString> storedBlock(BitReader &reader, QByteArray &out, qint64 size) {
    reader.alignToByte();
    if (reader.remaining() < 4) {
        return truncated;
    }
    const auto *header = reinterpret_cast<const quint8 *>(reader.take(4));
    const quint32 length = header[0] | (static_cast<quint32>(header[1]) << 8);
    const quint32 inverse = header[2] | (static_cast<quint32>(header[3]) << 8);
    if ((length ^ 0xFFFFu) != inverse) {
        return damaged;
    }
    if (reader.remaining() < static_cast<qsizetype>(length)) {
        return truncated;
    }
    if (out.size() + static_cast<qsizetype>(length) > size) {
        return tooLong;
    }
    out.append(reader.take(static_cast<qsizetype>(length)), static_cast<qsizetype>(length));
    return std::nullopt;
}

std::optional<QString> fixedBlock(BitReader &reader, QByteArray &out, qint64 size) {
    std::array<quint8, fixedLiteralCodes> lengths{};
    for (std::size_t symbol = 0; symbol < lengths.size(); ++symbol) {
        lengths[symbol] = symbol < 144 ? 8 : symbol < 256 ? 9 : symbol < 280 ? 7 : 8;
    }
    Code literals;
    build(literals, lengths.data(), fixedLiteralCodes);
    std::array<quint8, mostDistanceCodes> five{};
    five.fill(5);
    Code distances;
    build(distances, five.data(), mostDistanceCodes);
    return codedBlock(reader, out, size, literals, distances);
}

std::optional<QString> dynamicBlock(BitReader &reader, QByteArray &out, qint64 size) {
    const auto literalBits = reader.bits(5);
    const auto distanceBits = reader.bits(5);
    const auto lengthCodeBits = reader.bits(4);
    if (literalBits < 0 || distanceBits < 0 || lengthCodeBits < 0) {
        return truncated;
    }
    const int literalCount = literalBits + 257;
    const int distanceCount = distanceBits + 1;
    const int lengthCodeCount = lengthCodeBits + 4;
    if (literalCount > mostLiteralCodes || distanceCount > mostDistanceCodes) {
        return damaged;
    }
    std::array<quint8, mostLiteralCodes + mostDistanceCodes> lengths{};
    for (int index = 0; index < lengthCodeCount; ++index) {
        const auto value = reader.bits(3);
        if (value < 0) {
            return truncated;
        }
        lengths[codeLengthOrder[static_cast<std::size_t>(index)]] = static_cast<quint8>(value);
    }
    Code lengthCode;
    if (build(lengthCode, lengths.data(), 19) != 0) {
        return damaged; // the code-length code must be complete
    }
    lengths.fill(0);
    int index = 0;
    while (index < literalCount + distanceCount) {
        const auto symbol = decode(reader, lengthCode);
        if (symbol < 0) {
            return symbol == -1 ? truncated : damaged;
        }
        if (symbol < 16) {
            lengths[static_cast<std::size_t>(index++)] = static_cast<quint8>(symbol);
            continue;
        }
        quint8 repeated = 0;
        int times = 0;
        if (symbol == 16) {
            if (index == 0) {
                return damaged; // nothing to repeat
            }
            repeated = lengths[static_cast<std::size_t>(index - 1)];
            times = reader.bits(2);
            times = times < 0 ? -1 : times + 3;
        } else if (symbol == 17) {
            times = reader.bits(3);
            times = times < 0 ? -1 : times + 3;
        } else {
            times = reader.bits(7);
            times = times < 0 ? -1 : times + 11;
        }
        if (times < 0) {
            return truncated;
        }
        if (index + times > literalCount + distanceCount) {
            return damaged;
        }
        while (times-- > 0) {
            lengths[static_cast<std::size_t>(index++)] = repeated;
        }
    }
    if (lengths[256] == 0) {
        return damaged; // a block without an end code cannot end
    }
    // An incomplete code is allowed only when it is a single one-bit code.
    Code literals;
    auto left = build(literals, lengths.data(), literalCount);
    if (left < 0 || (left > 0 && literalCount != literals.count[0] + literals.count[1])) {
        return damaged;
    }
    Code distances;
    left = build(distances, lengths.data() + literalCount, distanceCount);
    if (left < 0 || (left > 0 && distanceCount != distances.count[0] + distances.count[1])) {
        return damaged;
    }
    return codedBlock(reader, out, size, literals, distances);
}

} // namespace

std::expected<QByteArray, QString> inflateRaw(const QByteArray &packed, qint64 size) {
    if (size < 0) {
        return std::unexpected(damaged);
    }
    QByteArray out;
    out.reserve(static_cast<qsizetype>(size));
    BitReader reader(packed);
    for (;;) {
        const auto last = reader.bits(1);
        const auto type = reader.bits(2);
        if (last < 0 || type < 0) {
            return std::unexpected(truncated);
        }
        std::optional<QString> problem;
        switch (type) {
        case 0:
            problem = storedBlock(reader, out, size);
            break;
        case 1:
            problem = fixedBlock(reader, out, size);
            break;
        case 2:
            problem = dynamicBlock(reader, out, size);
            break;
        default:
            problem = damaged;
        }
        if (problem) {
            return std::unexpected(*problem);
        }
        if (last == 1) {
            break;
        }
    }
    if (out.size() != size) {
        return std::unexpected(
            QCoreApplication::translate("eggbert::Files", "The compressed data holds less than its stated size."));
    }
    if (reader.remaining() != 0) {
        return std::unexpected(QCoreApplication::translate("eggbert::Files", "Bytes follow the end of the compressed data."));
    }
    return out;
}

} // namespace eggbert
