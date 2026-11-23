// SPDX-License-Identifier: GPL-3.0-only
//
// The DEFLATE decoder against streams produced independently (zlib 1.3,
// raw streams; the bytes below are test data, not code), and against
// damaged input: it must refuse, never read or write out of bounds.
#include "app/inflate.hpp"

#include "check.hpp"

#include <QByteArray>

using namespace eggbert;

namespace {

const char *const emptyStream =
    "0300";

const char *const helloStream =
    "cb48cdc9c957c8402701";

const char *const storedStream =
    "011e00e1ff73746f7265642062797465732c206b657074206173207468657920617265";

const char *const runStream =
    "4b4c1c05a360140c770000";

const char *const dynamicStream =
    "ed98410ed8280c45af04c6185b739d5956aad44dafdfc5e43b23de119a5d8482013f0c4ffcfef9ebc7bf638c7f7efff7"
    "31f5e1faa8e763c6f361fbf9586a71fd13ea954f9ca9c8d3d4e2cf3f339e5eb39e38a6c8b6d592cf3fcb9e5e2b9e38ae"
    "c8ee6ac9e79f6d4faf7d9e38a1c8a196a37f8e7aa5e2942297c61ad6ebd29c37e6ac552cadcbbde7f3c409450ee5e744"
    "8ffef4aa75e7792af326164b745cbc764756cbd13fa95e0582536399465f9a8fe73de7a355a4d6555ae900c1a5fcb832"
    "16cae1893bcf43999f62b144c741f0a8a5f4cf502ff37b6f6c8d75347a6a3e03049756b1b5aed32bb57b3f9bf2e3ca58"
    "28870982a6ccbb58c44ba72b4e9147af4291d52b41d034966bf4e81d851a5c5ac5f666a1bd0182aefc843256d6957be7"
    "3994f9148b293a0b04532def3fe9579cde1bef581afd9d0f08ba56d1eb1aeed7da7b3fbff9e93daf1c1e105cca7cb328"
    "d131d460339d6f9d2a32082e8dd5fba7341f430df63eb43e5bb4d204c1adfcf49e37f3ab2e3acf5d3b3bfdaa2f03c152"
    "4bd769a8d7400d76bd4ff3eb4c4810dc5a459f2dabf72a6ab0cfa8ec8c751580a02bf37d1eda5b5f770df6b99a2f6545"
    "0641d7587d865b9f09a8c1be0baa77669f362018ca4fdf3bdbfcba9b3acf7d7f59fa75c74d101c6ae9bbb2d2affbb4f7"
    "46dfb9697eddcb070443abe8fb3dfac6470db627ec3607e57083e0eecc8bc57e2de5aec1ffb5dc26b34170c364364c26"
    "6032019309103c309984c9144ca660320304274cc660320e93d9309900c103932998cc84c9184cc64130603209931930"
    "1983c93808064c2661321326b360321b0413263361320e9309984c81a0c164364ca660320693d92098301983c9044ca6"
    "60320b040f4c66c264364ca660320b040f4c66c26402263360320e82059371984cc164164c264170c1641226e3309982"
    "c93808164c66c364264c266032068209937198cc80c9044cc6403061320e93993099039359203860320726e330990193"
    "3920e83099099329984cc06416080e984cc2640226b3603213040b267360321b26b3603213040b26736032019371988c"
    "81e084c90c984cc1640e4c264070c3641c26e33099059331109c30990993993099019319203860320326f3bdc97c6f32"
    "df9bccf726f3bdc97c6f32df9bccf726f3bdc97c6f32df9bccf726f3bdc9fc756f327f00";

const char *const multiStream =
    "4acb2c2a2e5128482c2ad15100000000ffff358d310ac0300c03bf143b8ee3d0ef742c14bae4fb1da2db0e219df6fb3d"
    "776bedda070c0860092c053e049d24e824ab92c7309b93843a965ad992c731fb202975ba6bd5539ec01c4152ea0cd76a"
    "4c791273924c3a9355e15998175fedbcff";

const char *const fixedStream =
    "2bcf2fca49313030b02e87300c610c1318c312ca303483328c4ca10c639888094c8d194c9705d41c4398c98646301113"
    "a81a4333a82e434ba8394630938d4c6122165035c646505dc66650734c60269b98c0442ca06a4c8da0ba4ccda1e698c1"
    "4c36838998c3d498c37459c0ccb184996c09b3cb00623b00";


QByteArray bytesOf(const char *hex) { return QByteArray::fromHex(hex); }

// The text the dynamic and fixed streams were made from.
QByteArray worldList(int entries) {
    QByteArray text;
    for (int index = 0; index < entries; ++index) {
        text += QByteArray("world") +
                QByteArray::number((index * index) % 1000).rightJustified(3, '0') + ";";
    }
    return text;
}

bool refuses(const QByteArray &stream, qint64 size) { return !inflateRaw(stream, size); }

} // namespace

int main() {
    // One stream per block type and feature.
    {
        const auto empty = inflateRaw(bytesOf(emptyStream), 0);
        CHECK(empty.has_value() && empty->isEmpty());
        const auto hello = inflateRaw(bytesOf(helloStream), 23);
        CHECK(hello.has_value() && *hello == "hello hello hello hello");
        const auto stored = inflateRaw(bytesOf(storedStream), 30);
        CHECK(stored.has_value() && *stored == "stored bytes, kept as they are");
        // A match that overlaps what it writes (distance 1).
        const auto run = inflateRaw(bytesOf(runStream), 1000);
        CHECK(run.has_value() && *run == QByteArray(1000, 'a'));
        const auto text = worldList(900);
        CHECK_EQ(text.size(), qsizetype{8100});
        const auto dynamic = inflateRaw(bytesOf(dynamicStream), text.size());
        CHECK(dynamic.has_value() && *dynamic == text);
        const auto fixed = inflateRaw(bytesOf(fixedStream), 300);
        CHECK(fixed.has_value() && *fixed == text.left(300));
        // Several blocks, an empty stored one among them.
        const auto multi = inflateRaw(bytesOf(multiStream), 312);
        CHECK(multi.has_value() && *multi == "first part, " + text.left(300));
    }
    // The stated size is exact: neither more nor less is accepted, and
    // nothing may follow the stream.
    {
        const auto hello = bytesOf(helloStream);
        CHECK(refuses(hello, 22));
        CHECK(refuses(hello, 24));
        CHECK(refuses(hello, 0));
        CHECK(refuses(hello, -1));
        CHECK(refuses(hello + QByteArray(1, '\0'), 23));
        CHECK(refuses(bytesOf(storedStream), 29));
        CHECK(refuses(bytesOf(runStream), 999));
        const auto more = inflateRaw(bytesOf(runStream), 999);
        CHECK(!more.has_value() && more.error().contains("more than its stated size"));
        const auto fewer = inflateRaw(bytesOf(runStream), 1001);
        CHECK(!fewer.has_value() && fewer.error().contains("less than its stated size"));
    }
    // Damaged streams, each refused for its own reason.
    {
        CHECK(refuses(QByteArray(), 0));
        const auto farBack = inflateRaw(QByteArray::fromHex("030200"), 3);
        CHECK(!farBack.has_value() && farBack.error().contains("damaged"));
        CHECK(refuses(QByteArray::fromHex("07"), 0)); // block type 3
        CHECK(refuses(QByteArray::fromHex("010500fafe0102030405"), 5)); // length check
        const auto cut = inflateRaw(QByteArray::fromHex("010500faff010203"), 5);
        CHECK(!cut.has_value() && cut.error().contains("ends too early"));
        CHECK(inflateRaw(QByteArray::fromHex("010500faff0102030405"), 5).has_value());
    }
    // Every proper prefix of a stream is refused as ending too early.
    {
        const struct {
            const char *hex;
            qint64 size;
        } streams[] = {{helloStream, 23}, {storedStream, 30}, {dynamicStream, 8100},
                       {multiStream, 312}, {runStream, 1000}};
        for (const auto &stream : streams) {
            const auto whole = bytesOf(stream.hex);
            for (qsizetype length = 0; length < whole.size(); ++length) {
                const auto result = inflateRaw(whole.left(length), stream.size);
                CHECK(!result.has_value());
                if (result.has_value() || !result.error().contains("ends too early")) {
                    CHECK(!"a prefix was not reported as ending too early");
                    break;
                }
            }
        }
    }
    // Every single-bit change either is refused or yields exactly the stated
    // number of bytes; under the sanitizers this also shows that no change
    // makes the decoder touch memory it must not.
    {
        const struct {
            const char *hex;
            qint64 size;
        } streams[] = {{helloStream, 23}, {runStream, 1000}, {fixedStream, 300},
                       {dynamicStream, 8100}, {multiStream, 312}, {storedStream, 30}};
        int refused = 0;
        int accepted = 0;
        for (const auto &stream : streams) {
            const auto whole = bytesOf(stream.hex);
            for (qsizetype index = 0; index < whole.size(); ++index) {
                for (int bit = 0; bit < 8; ++bit) {
                    auto changed = whole;
                    changed[index] = static_cast<char>(changed[index] ^ (1 << bit));
                    const auto result = inflateRaw(changed, stream.size);
                    if (result) {
                        CHECK_EQ(result->size(), qsizetype{stream.size});
                        ++accepted;
                    } else {
                        ++refused;
                    }
                }
            }
        }
        CHECK(refused > 1000);
        CHECK(accepted > 0); // a changed literal still decodes: the checksum is the archive's job
    }
    return test::finish("inflate");
}
