// SPDX-License-Identifier: GPL-3.0-only
#include "music_player.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryFile>

namespace eggbert {

namespace {

constexpr qint64 maximumMusicBytes = 16 * 1024 * 1024;

QString findSoundFont() {
    for (const char *directory : {"/usr/share/sounds/sf2", "/usr/share/sounds/sf3",
                                  "/usr/share/soundfonts", "/usr/share/sf2"}) {
        const QDir dir(QString::fromLatin1(directory));
        const auto fonts = dir.entryList({QStringLiteral("*.sf2"), QStringLiteral("*.sf3")},
                                         QDir::Files, QDir::Name);
        if (!fonts.isEmpty()) {
            return dir.filePath(fonts.first());
        }
    }
    return {};
}

} // namespace

MusicPlayer::MusicPlayer(QObject *parent) : QObject(parent), backend_(detectBackend()) {
    process_.setProcessChannelMode(QProcess::MergedChannels);
    connect(&process_, &QProcess::stateChanged, this, [this] { emit stateChanged(); });
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && backend_) {
            lastError_ = tr("%1 could not be started.").arg(backend_->name);
        }
        emit stateChanged();
    });
    connect(&process_, &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus status) {
                // A player that gives up at once usually lacks its sound
                // set; show what it said.
                if (!stopping_ && (status != QProcess::NormalExit || exitCode != 0) && backend_) {
                    const auto output = QString::fromLocal8Bit(process_.readAll()).trimmed();
                    lastError_ = tr("%1 stopped with an error%2")
                                     .arg(backend_->name,
                                          output.isEmpty()
                                              ? QStringLiteral(".")
                                              : QStringLiteral(": ") + output.right(300));
                }
                stopping_ = false;
                emit stateChanged();
            });
}

MusicPlayer::~MusicPlayer() {
    disconnect(&process_, nullptr, this, nullptr);
    if (process_.state() != QProcess::NotRunning) {
        process_.kill();
        process_.waitForFinished(1000);
    }
}

std::optional<MusicPlayer::Backend> MusicPlayer::backendFromCommand(const QString &command) {
    auto parts = QProcess::splitCommand(command.trimmed());
    if (parts.isEmpty()) {
        return std::nullopt;
    }
    Backend backend;
    backend.program = parts.takeFirst();
    backend.arguments = parts;
    if (!backend.arguments.contains(QStringLiteral("%f"))) {
        backend.arguments << QStringLiteral("%f");
    }
    backend.name = QFileInfo(backend.program).fileName();
    return backend;
}

std::optional<MusicPlayer::Backend> MusicPlayer::detectBackend() {
    const auto custom = QSettings().value(QStringLiteral("music/command")).toString();
    if (!custom.trimmed().isEmpty()) {
        if (auto backend = backendFromCommand(custom)) {
            return backend;
        }
    }
    if (const auto fluid = QStandardPaths::findExecutable(QStringLiteral("fluidsynth"));
        !fluid.isEmpty()) {
        if (const auto font = findSoundFont(); !font.isEmpty()) {
            return Backend{fluid, {QStringLiteral("-i"), QStringLiteral("-n"), QStringLiteral("-q"),
                                   font, QStringLiteral("%f")},
                           QStringLiteral("FluidSynth")};
        }
    }
    if (const auto timidity = QStandardPaths::findExecutable(QStringLiteral("timidity"));
        !timidity.isEmpty()) {
        return Backend{timidity, {QStringLiteral("%f")}, QStringLiteral("TiMidity++")};
    }
    if (const auto wild = QStandardPaths::findExecutable(QStringLiteral("wildmidi"));
        !wild.isEmpty()) {
        return Backend{wild, {QStringLiteral("%f")}, QStringLiteral("WildMIDI")};
    }
    return std::nullopt;
}

void MusicPlayer::setBackend(std::optional<Backend> backend) {
    stop();
    backend_ = std::move(backend);
    emit stateChanged();
}

QString MusicPlayer::statusText() const {
    if (!backend_) {
        return tr("No MIDI player was found. The games' music is MIDI; install FluidSynth with "
                  "a sound font, TiMidity++ or WildMIDI to listen to it here.");
    }
    if (isPlaying()) {
        return tr("Playing music %1 with %2.").arg(number_).arg(backend_->name);
    }
    if (!lastError_.isEmpty()) {
        return lastError_;
    }
    return tr("Music is played with %1.").arg(backend_->name);
}

bool MusicPlayer::isPlaying() const {
    return process_.state() != QProcess::NotRunning;
}

void MusicPlayer::stop() {
    if (process_.state() == QProcess::NotRunning) {
        return;
    }
    stopping_ = true;
    process_.terminate();
    if (!process_.waitForFinished(500)) {
        process_.kill();
        process_.waitForFinished(1000);
    }
    stopping_ = false;
    copy_.reset();
    emit stateChanged();
}

bool MusicPlayer::play(const QString &musicFile, int musicNumber) {
    stop();
    lastError_.clear();
    if (!backend_) {
        lastError_ = statusText();
        emit stateChanged();
        return false;
    }
    QFile source(musicFile);
    if (musicFile.isEmpty() || !source.open(QIODevice::ReadOnly) ||
        source.size() > maximumMusicBytes) {
        lastError_ = tr("The music file of music %1 was not found in the game directory.")
                         .arg(musicNumber);
        emit stateChanged();
        return false;
    }
    const auto bytes = source.readAll();
    if (!bytes.startsWith("MThd")) {
        lastError_ = tr("The file of music %1 is not a MIDI file.").arg(musicNumber);
        emit stateChanged();
        return false;
    }
    copy_ = std::make_unique<QTemporaryFile>(QDir::tempPath() +
                                             QStringLiteral("/eggbert-music-XXXXXX.mid"));
    if (!copy_->open() || copy_->write(bytes) != bytes.size()) {
        lastError_ = tr("A temporary copy of the music could not be written.");
        copy_.reset();
        emit stateChanged();
        return false;
    }
    copy_->close();
    auto arguments = backend_->arguments;
    arguments.replaceInStrings(QStringLiteral("%f"), copy_->fileName());
    number_ = musicNumber;
    process_.start(backend_->program, arguments, QIODevice::ReadOnly);
    if (!process_.waitForStarted(2000)) {
        lastError_ = tr("%1 could not be started.").arg(backend_->name);
        copy_.reset();
        emit stateChanged();
        return false;
    }
    emit stateChanged();
    return true;
}

} // namespace eggbert
