// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QByteArray>
#include <QString>

#include <expected>

namespace eggbert {

// Decompresses a raw DEFLATE stream (RFC 1951), the compression ZIP
// programs use, for campaign packs. The input is untrusted: every read is
// bounds-checked, the output never grows beyond `size`, and anything else
// than exactly `size` bytes from exactly the whole input is an error.
[[nodiscard]] std::expected<QByteArray, QString> inflateRaw(const QByteArray &packed,
                                                            qint64 size);

} // namespace eggbert
