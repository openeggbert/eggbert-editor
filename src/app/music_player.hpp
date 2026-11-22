// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

#include <memory>
#include <optional>

class QTemporaryFile;

namespace eggbert {

// Plays one of a game's music files (standard MIDI files under another
// name) for listening while choosing. The editor contains no synthesizer:
// it starts a MIDI player installed on the system and stops it again. The
// game's file is never touched; a temporary copy with a .mid name is played.
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

        friend bool operator==(const Backend &, const Backend &) = default;
    };

    explicit MusicPlayer(QObject *parent = nullptr);
    ~MusicPlayer() override;

    // Looks for a usable player: the command stored in the settings key
    // "music/command" (program and arguments, "%f" for the file), then
    // FluidSynth with a General MIDI sound font, TiMidity++ and WildMIDI.
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

signals:
    void stateChanged();

private:
    std::optional<Backend> backend_;
    QProcess process_;
    std::unique_ptr<QTemporaryFile> copy_;
    int number_ = 0;
    QString lastError_;
    bool stopping_ = false;
};

} // namespace eggbert
