// SPDX-License-Identifier: GPL-3.0-only
#include "music_player.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>

#include "app/core_texts.hpp"
#include "app/midi_synth.hpp"

#include <string_view>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <mmsystem.h>
#endif

namespace eggbert {

namespace {

#ifdef Q_OS_WIN
// One command to Windows' media control interface; the answer, when wanted.
bool mediaControl(const QString &command, QString *answer = nullptr) {
    wchar_t buffer[128] = {};
    const auto error = mciSendStringW(reinterpret_cast<const wchar_t *>(command.utf16()), buffer,
                                      127, nullptr);
    if (answer != nullptr) {
        *answer = QString::fromWCharArray(buffer);
    }
    return error == 0;
}
constexpr char nativeAlias[] = "eggberteditormusic";
#endif

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
    nativeTimer_ = new QTimer(this);
    nativeTimer_->setInterval(500);
    connect(nativeTimer_, &QTimer::timeout, this, [this] {
#ifdef Q_OS_WIN
        QString mode;
        if (nativePlaying_ &&
            (!mediaControl(QStringLiteral("status %1 mode").arg(QLatin1String(nativeAlias)),
                           &mode) ||
             mode != QLatin1String("playing"))) {
            stopNative();
            emit stateChanged();
        }
#endif
    });
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
#ifdef Q_OS_WIN
    return Backend{QString(), {}, QStringLiteral("Windows"), false, true};
#endif
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
    // No MIDI player: the editor renders the music itself and only needs
    // something that plays a WAV file.
    for (const char *player : {"afplay", "pw-play", "paplay", "aplay"}) {
        if (const auto found = QStandardPaths::findExecutable(QString::fromLatin1(player));
            !found.isEmpty()) {
            return Backend{found,
                           player == std::string_view("aplay")
                               ? QStringList{QStringLiteral("-q"), QStringLiteral("%f")}
                               : QStringList{QStringLiteral("%f")},
                           tr("the editor's own synthesizer (through %1)")
                               .arg(QString::fromLatin1(player)),
                           true, false};
        }
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
        return tr("No MIDI player and no sound player was found. The editor can turn the "
                  "games' music into sound itself, but it needs a program that plays sound: "
                  "pw-play, paplay or aplay (PipeWire, PulseAudio or ALSA utilities), or a MIDI "
                  "player such as FluidSynth or TiMidity++.");
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
    return nativePlaying_ || process_.state() != QProcess::NotRunning;
}

void MusicPlayer::stopNative() {
#ifdef Q_OS_WIN
    mediaControl(QStringLiteral("stop %1").arg(QLatin1String(nativeAlias)));
    mediaControl(QStringLiteral("close %1").arg(QLatin1String(nativeAlias)));
#endif
    nativeTimer_->stop();
    nativePlaying_ = false;
    copy_.reset();
}

std::optional<int> MusicPlayer::durationSeconds(const QString &musicFile) {
    QFile file(musicFile);
    if (musicFile.isEmpty() || !file.open(QIODevice::ReadOnly) ||
        file.size() > static_cast<qint64>(midiLargestFile)) {
        return std::nullopt;
    }
    const auto bytes = file.readAll();
    const auto song = parseMidi({reinterpret_cast<const std::byte *>(bytes.constData()),
                                 static_cast<std::size_t>(bytes.size())});
    if (!song) {
        return std::nullopt;
    }
    return static_cast<int>(song->duration + 0.5);
}

QString MusicPlayer::trackLabel(int number, const QString &musicFile) {
    if (number == 0) {
        return tr("No music");
    }
    if (const auto seconds = durationSeconds(musicFile)) {
        return tr("Music %1 - %2:%3")
            .arg(number)
            .arg(*seconds / 60)
            .arg(*seconds % 60, 2, 10, QLatin1Char('0'));
    }
    return tr("Music %1").arg(number);
}

void MusicPlayer::stop() {
    if (nativePlaying_) {
        stopNative();
        emit stateChanged();
        return;
    }
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
    // What is played: the MIDI file itself, or its sound from the editor's
    // own synthesizer.
    auto played = bytes;
    if (backend_->rendersSound) {
        const auto song = parseMidi({reinterpret_cast<const std::byte *>(bytes.constData()),
                                     static_cast<std::size_t>(bytes.size())});
        if (!song) {
            lastError_ = tr("Music %1 cannot be played: %2")
                             .arg(musicNumber)
                             .arg(coreText(song.error()));
            emit stateChanged();
            return false;
        }
        const auto wave = wavFile(renderMidi(*song));
        played = QByteArray(reinterpret_cast<const char *>(wave.data()),
                            static_cast<qsizetype>(wave.size()));
    }
    copy_ = std::make_unique<QTemporaryFile>(
        QDir::tempPath() + (backend_->rendersSound ? QStringLiteral("/eggbert-music-XXXXXX.wav")
                                                   : QStringLiteral("/eggbert-music-XXXXXX.mid")));
    if (!copy_->open() || copy_->write(played) != played.size()) {
        lastError_ = tr("A temporary copy of the music could not be written.");
        copy_.reset();
        emit stateChanged();
        return false;
    }
    copy_->close();
    number_ = musicNumber;
    if (backend_->native) {
#ifdef Q_OS_WIN
        const auto alias = QLatin1String(nativeAlias);
        if (mediaControl(QStringLiteral("open \"%1\" type sequencer alias %2")
                             .arg(QDir::toNativeSeparators(copy_->fileName()), alias)) &&
            mediaControl(QStringLiteral("play %1").arg(alias))) {
            nativePlaying_ = true;
            nativeTimer_->start();
            emit stateChanged();
            return true;
        }
        mediaControl(QStringLiteral("close %1").arg(alias));
#endif
        lastError_ = tr("%1 could not play the music.").arg(backend_->name);
        copy_.reset();
        emit stateChanged();
        return false;
    }
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
