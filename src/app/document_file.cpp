// SPDX-License-Identifier: GPL-3.0-only
#include "document_file.hpp"

#include "app/core_texts.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <algorithm>

namespace eggbert {

namespace {

QString tr(const char *text) {
    return QCoreApplication::translate("DocumentFile", text);
}

struct ReadOutcome {
    DiskState state = DiskState::unreadable;
    QByteArray bytes;
    QString detail;
};

// Reads at most `limit` + 1 bytes so an oversized file is detected without
// loading it.
ReadOutcome readFile(const QString &path, qint64 limit) {
    ReadOutcome outcome;
    QFile file(path);
    if (!file.exists()) {
        outcome.state = DiskState::missing;
        return outcome;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        outcome.detail = file.errorString();
        return outcome;
    }
    outcome.bytes = file.read(limit + 1);
    if (file.error() != QFileDevice::NoError) {
        outcome.detail = file.errorString();
        outcome.bytes.clear();
        return outcome;
    }
    outcome.state = DiskState::modified; // refined by the caller's comparison
    return outcome;
}

bool sameBytes(const QByteArray &left, std::span<const std::byte> right) {
    return static_cast<std::size_t>(left.size()) == right.size() &&
           std::equal(right.begin(), right.end(),
                      reinterpret_cast<const std::byte *>(left.constData()));
}

bool samePath(const QString &left, const QString &right) {
    const QFileInfo a(left);
    const QFileInfo b(right);
    if (a.exists() && b.exists()) {
        return a.canonicalFilePath() == b.canonicalFilePath();
    }
    return a.absoluteFilePath() == b.absoluteFilePath();
}

} // namespace

QByteArray toByteArray(std::span<const std::byte> bytes) {
    return QByteArray(reinterpret_cast<const char *>(bytes.data()),
                      static_cast<qsizetype>(bytes.size()));
}

std::vector<std::byte> toByteVector(const QByteArray &bytes) {
    const auto *begin = reinterpret_cast<const std::byte *>(bytes.constData());
    return std::vector<std::byte>(begin, begin + bytes.size());
}

DocumentFile::DocumentFile(QString path, std::vector<std::byte> bytes)
    : path_(std::move(path)), document_(std::move(bytes)) {}

std::expected<std::unique_ptr<DocumentFile>, LoadFailure> DocumentFile::load(const QString &path) {
    auto outcome = readFile(path, maximumBytes);
    if (outcome.state == DiskState::missing) {
        return std::unexpected(LoadFailure{LoadError::cannotOpen, tr("The file does not exist.")});
    }
    if (outcome.state == DiskState::unreadable) {
        return std::unexpected(LoadFailure{LoadError::cannotOpen, outcome.detail});
    }
    if (outcome.bytes.size() > maximumBytes) {
        return std::unexpected(LoadFailure{
            LoadError::tooLarge, tr("The file is larger than %1 bytes.").arg(maximumBytes)});
    }
    const std::string_view text(outcome.bytes.constData(),
                                static_cast<std::size_t>(outcome.bytes.size()));
    if (looksLikeTextWorld(text)) {
        auto world = TextWorld::parse(text);
        auto image = makeTextWorldImage(world);
        if (!image) {
            return std::unexpected(LoadFailure{
                LoadError::textWorldShape,
                tr("This Mobile Eggbert world can only be edited as text: %1")
                    .arg(coreText(image.error()))});
        }
        std::unique_ptr<DocumentFile> file(
            new DocumentFile(QFileInfo(path).absoluteFilePath(), image->bytes));
        file->text_ = std::move(world);
        file->image_ = std::move(*image);
        file->diskText_ = outcome.bytes;
        return file;
    }
    return std::unique_ptr<DocumentFile>(
        new DocumentFile(QFileInfo(path).absoluteFilePath(), toByteVector(outcome.bytes)));
}

DiskState DocumentFile::diskState() const {
    const auto outcome = readFile(path_, maximumBytes);
    if (outcome.state != DiskState::modified) {
        return outcome.state;
    }
    if (text_) {
        return outcome.bytes == diskText_ ? DiskState::unchanged : DiskState::modified;
    }
    return sameBytes(outcome.bytes, document_.savedBytes()) ? DiskState::unchanged
                                                            : DiskState::modified;
}

SaveResult DocumentFile::save(const SaveHooks &hooks) {
    const auto state = diskState();
    if (state != DiskState::unchanged) {
        SaveResult result;
        result.error = SaveError::conflict;
        result.diskState = state;
        result.detail = state == DiskState::missing
                            ? tr("The file no longer exists on disk.")
                            : state == DiskState::modified
                                  ? tr("The file was changed on disk by something else.")
                                  : tr("The file on disk cannot be read to check for changes.");
        return result;
    }
    return write(path_, hooks);
}

SaveResult DocumentFile::saveAs(const QString &newPath, bool overwriteExisting,
                                const SaveHooks &hooks) {
    if (samePath(newPath, path_)) {
        return save(hooks);
    }
    if (QFileInfo::exists(newPath) && !overwriteExisting) {
        SaveResult result;
        result.error = SaveError::destinationExists;
        result.detail = tr("The destination already exists.");
        return result;
    }
    auto result = write(newPath, hooks);
    if (result.ok()) {
        path_ = QFileInfo(newPath).absoluteFilePath();
    }
    return result;
}

QByteArray DocumentFile::savedContent() const {
    return text_ ? diskText_ : toByteArray(document_.savedBytes());
}

std::expected<QByteArray, QString> DocumentFile::currentContent() const {
    if (!text_) {
        return toByteArray(document_.currentBytes());
    }
    auto text = *text_;
    auto image = image_;
    if (const auto stored = storeTextWorldImage(text, image, document_.currentBytes()); !stored) {
        return std::unexpected(coreText(stored.error()));
    }
    return QByteArray::fromStdString(text.write());
}

std::expected<std::vector<std::byte>, QString>
DocumentFile::documentBytesOf(const QByteArray &content) const {
    const auto *data = reinterpret_cast<const std::byte *>(content.constData());
    if (!text_) {
        return std::vector<std::byte>(data, data + content.size());
    }
    const std::string_view view(content.constData(), static_cast<std::size_t>(content.size()));
    if (!looksLikeTextWorld(view)) {
        return std::unexpected(tr("It is not a Mobile Eggbert text world."));
    }
    auto image = makeTextWorldImage(TextWorld::parse(view));
    if (!image) {
        return std::unexpected(coreText(image.error()));
    }
    return std::move(image->bytes);
}

SaveResult DocumentFile::write(const QString &destination, const SaveHooks &hooks) {
    SaveResult result;
    // A text world: the changed values go into a copy of the text, which
    // replaces the kept one only when the file was written.
    auto text = text_;
    auto image = image_;
    QByteArray bytes;
    if (text) {
        if (const auto stored = storeTextWorldImage(*text, image, document_.currentBytes());
            !stored) {
            result.error = SaveError::notStorable;
            result.detail = coreText(stored.error());
            return result;
        }
        bytes = QByteArray::fromStdString(text->write());
    } else {
        bytes = toByteArray(document_.currentBytes());
    }

    QSaveFile file(destination);
    // Never fall back to truncating and rewriting the destination in place.
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        result.error = SaveError::openFailed;
        result.detail = file.errorString();
        return result;
    }
    if (file.write(bytes) != bytes.size()) {
        result.error = SaveError::writeFailed;
        result.detail = file.errorString();
        file.cancelWriting();
        return result;
    }
    if (hooks.beforeCommit && !hooks.beforeCommit()) {
        file.cancelWriting();
        result.error = SaveError::abortedBeforeCommit;
        result.detail = tr("The save was abandoned before replacing the file.");
        return result;
    }
    if (!file.commit()) {
        result.error = SaveError::commitFailed;
        result.detail = file.errorString();
        return result;
    }
    if (text) {
        text_ = std::move(text);
        image_ = std::move(image);
        diskText_ = bytes;
    }
    document_.markSaved();
    return result;
}

} // namespace eggbert
