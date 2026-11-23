// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

#include <memory>
#include <optional>

class QTemporaryFile;
class QTimer;

namespace eggbert {

// Plays one of a game's music files (standard MIDI files under another
// name) for listening while choosing. Three ways, in this order: Windows'
// own MIDI playback, as the games use it; a MIDI player installed on the
// system (FluidSynth, TiMidity++, WildMIDI or a command of the user's); or
// the editor's own small synthesizer (midi_synth.hpp), whose sound only
// needs a program that sends a WAV file to the speakers, which every desktop
// has. The game's file is never touched; a temporary copy is played.
class MusicPlayer final : public QObject {
    Q_OBJECT

public:
    // A command able to play a MIDI file; "%f" in the arguments stands for
    // the file.
    struct Backend {
        QString program;
        QStringList arguments;
        // Shown to the user, e.g. "TiMidity++".
        QString name;
        // True when the command plays sound the editor renders itself (a WAV
        // file) instead of the MIDI file.
        bool rendersSound = false;
        // True for Windows' own MIDI playback; no command is started.
        bool native = false;

        friend bool operator==(const Backend &, const Backend &) = default;
    };

    explicit MusicPlayer(QObject *parent = nullptr);
    ~MusicPlayer() override;

    // Looks for a usable player: the command stored in the settings key
    // "music/command" (program and arguments, "%f" for the file), Windows'
    // own playback, FluidSynth with a General MIDI sound font, TiMidity++,
    // WildMIDI, and last a sound player for the editor's own synthesizer
    // (afplay, pw-play, paplay, aplay).
    [[nodiscard]] static std::optional<Backend> detectBackend();
    // Splits a command line the way a shell would for simple quoting.
    [[nodiscard]] static std::optional<Backend> backendFromCommand(const QString &command);

    void setBackend(std::optional<Backend> backend);
    [[nodiscard]] const std::optional<Backend> &backend() const { return backend_; }
    [[nodiscard]] bool available() const { return backend_.has_value(); }
    // One sentence for the user about what plays, or why nothing can.
    [[nodiscard]] QString statusText() const;

    // Starts playing a copy of `musicFile`; false (with lastError) when no
    // player is available, the file cannot be read or is not a MIDI file,
    // or the player cannot be started. Stops what was playing before.
    bool play(const QString &musicFile, int musicNumber);
    void stop();
    [[nodiscard]] bool isPlaying() const;
    // The stored music number being played, or 0.
    [[nodiscard]] int playingNumber() const { return isPlaying() ? number_ : 0; }
    [[nodiscard]] QString lastError() const { return lastError_; }

    // How long a music file plays, in seconds; nothing when it is missing
    // or not a MIDI file the editor can read.
    [[nodiscard]] static std::optional<int> durationSeconds(const QString &musicFile);
    // What a list shows for a music number: "No music" for 0, otherwise the
    // number and, when its file can be read, how long it plays.
    [[nodiscard]] static QString trackLabel(int number, const QString &musicFile);

signals:
    void stateChanged();

private:
    std::optional<Backend> backend_;
    QProcess process_;
    std::unique_ptr<QTemporaryFile> copy_;
    int number_ = 0;
    QString lastError_;
    bool stopping_ = false;
    // Windows' own playback has no process to watch: it is asked now and
    // then whether it still plays.
    bool nativePlaying_ = false;
    QTimer *nativeTimer_ = nullptr;
    void stopNative();
};

} // namespace eggbert
