// SPDX-License-Identifier: GPL-3.0-only
//
// The music player starts and stops an external MIDI player. The test uses
// a stand-in command; no sound is produced and no game file is used.

#include "app/music_player.hpp"

#include "check.hpp"
#include "fixture.hpp"
#include "generated_game.hpp"

#include <QCoreApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace eggbert;
using namespace eggbert::test;

int main(int argc, char **argv) {
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName("eggbert-editor-tests");
    QCoreApplication::setApplicationName("music-player-test");
    QSettings().clear();

    QTemporaryDir dir;
    Game game;
    game.root = dir.filePath("game");
    QDir().mkpath(game.root);
    addMusic(game);
    const auto midi = game.root + "/SOUND/MUSIC000.BLP";
    const auto notMidi = game.root + "/SOUND/MUSIC001.BLP";
    const auto digest = directoryDigest(game.root);

    // Command lines.
    {
        const auto backend = MusicPlayer::backendFromCommand("  myplayer -q \"two words\" %f ");
        CHECK(backend.has_value());
        CHECK_EQ(backend->program, QString("myplayer"));
        CHECK((backend->arguments == QStringList{"-q", "two words", "%f"}));
        // Without a place for the file it goes last.
        CHECK((MusicPlayer::backendFromCommand("p -x")->arguments == QStringList{"-x", "%f"}));
        CHECK(!MusicPlayer::backendFromCommand("   ").has_value());
        // The user's command takes precedence over anything installed.
        QSettings().setValue("music/command", "/bin/echo %f");
        CHECK(MusicPlayer::detectBackend().has_value());
        CHECK_EQ(MusicPlayer::detectBackend()->program, QString("/bin/echo"));
        QSettings().remove("music/command");
    }

    MusicPlayer player;
    int changes = 0;
    QObject::connect(&player, &MusicPlayer::stateChanged, [&] { ++changes; });

    // No player at all.
    player.setBackend(std::nullopt);
    CHECK(!player.available());
    CHECK(!player.play(midi, 1));
    CHECK(!player.isPlaying());
    CHECK(player.statusText().contains("No MIDI player"));

    // A stand-in player that keeps running until it is stopped. It receives
    // a copy with a .mid name, never the game's own file.
    const auto log = dir.filePath("played.txt");
    player.setBackend(
        MusicPlayer::Backend{EGGBERT_STANDIN_PLAYER, {"--log", log, "%f"}, "Stand-in"});
    CHECK(player.available());
    CHECK(player.play(midi, 1));
    CHECK(player.isPlaying());
    CHECK_EQ(player.playingNumber(), 1);
    CHECK(player.statusText().contains("Playing music 1 with Stand-in"));
    CHECK(QTest::qWaitFor([&] { return QFile::exists(log); }, 3000));
    QFile played(log);
    CHECK(played.open(QIODevice::ReadOnly));
    const auto playedPath = QString::fromLocal8Bit(played.readAll()).trimmed();
    CHECK(playedPath.endsWith(".mid"));
    CHECK(QFileInfo(playedPath) != QFileInfo(midi));
    CHECK(QFile::exists(playedPath));

    // Starting another one stops the first; stopping ends it.
    CHECK(player.play(midi, 3));
    CHECK_EQ(player.playingNumber(), 3);
    player.stop();
    CHECK(!player.isPlaying());
    CHECK_EQ(player.playingNumber(), 0);
    CHECK(!QFile::exists(playedPath)); // the temporary copy is gone
    CHECK(changes > 0);

    // Without a MIDI player the editor renders the music itself: the player
    // then receives a WAV file made by the editor's own synthesizer.
    {
        const QByteArray tune("MThd\0\0\0\x06\0\0\0\x01\x01\xE0MTrk\0\0\0\x0D"
                              "\0\x90\x3C\x50\x83\x60\x80\x3C\0\0\xFF\x2F\0",
                              35);
        const auto tunePath = dir.filePath("tune.blp");
        QFile tuneFile(tunePath);
        CHECK(tuneFile.open(QIODevice::WriteOnly));
        tuneFile.write(tune);
        tuneFile.close();
        CHECK(MusicPlayer::durationSeconds(tunePath) == 1); // half a second, rounded
        CHECK(!MusicPlayer::durationSeconds(notMidi).has_value());
        CHECK(!MusicPlayer::durationSeconds(dir.filePath("missing.blp")).has_value());

        const auto waveLog = dir.filePath("played-wave.txt");
        MusicPlayer::Backend own{EGGBERT_STANDIN_PLAYER, {"--log", waveLog, "%f"}, "Own"};
        own.rendersSound = true;
        player.setBackend(own);
        CHECK(player.play(tunePath, 2));
        CHECK(player.isPlaying());
        CHECK(QTest::qWaitFor([&] { return QFile::exists(waveLog); }, 3000));
        QString wavePath;
        {
            QFile logged(waveLog);
            CHECK(logged.open(QIODevice::ReadOnly));
            wavePath = QString::fromLocal8Bit(logged.readAll()).trimmed();
        }
        CHECK(wavePath.endsWith(".wav"));
        QByteArray sound;
        {
            // Read and closed again before the player is stopped: an open
            // file cannot be removed on Windows.
            QFile wave(wavePath);
            CHECK(wave.open(QIODevice::ReadOnly));
            sound = wave.readAll();
        }
        CHECK(sound.startsWith("RIFF") && sound.mid(8, 4) == "WAVE");
        // Half a second of note and the tail, 22,050 samples a second.
        CHECK(sound.size() > 44 + 2 * 22'050 / 2);
        CHECK(sound.mid(44).count('\0') < sound.size() - 44);
        player.stop();
        CHECK(!player.isPlaying());
        CHECK(!QFile::exists(wavePath));
        // A file that begins like MIDI but is damaged is refused with the reason.
        const auto brokenPath = dir.filePath("broken.blp");
        QFile broken(brokenPath);
        CHECK(broken.open(QIODevice::WriteOnly));
        broken.write(tune.left(30));
        broken.close();
        CHECK(!player.play(brokenPath, 5));
        CHECK(player.lastError().contains("Music 5 cannot be played"));
        player.setBackend(
            MusicPlayer::Backend{EGGBERT_STANDIN_PLAYER, {"--log", log, "%f"}, "Stand-in"});
    }

    // Files that are missing or not MIDI are refused before anything runs.
    CHECK(!player.play(notMidi, 2));
    CHECK(player.lastError().contains("not a MIDI file"));
    CHECK(!player.play(game.root + "/SOUND/MUSIC009.BLP", 10));
    CHECK(player.lastError().contains("not found"));
    CHECK(!player.play(QString(), 4));
    CHECK(!player.isPlaying());

    // A player that fails at once reports what it said.
    player.setBackend(MusicPlayer::Backend{EGGBERT_STANDIN_PLAYER, {"--fail", "%f"}, "Broken"});
    CHECK(player.play(midi, 1));
    CHECK(QTest::qWaitFor([&] { return !player.isPlaying(); }, 3000));
    CHECK(player.statusText().contains("Broken stopped with an error"));
    CHECK(player.statusText().contains("no sound set"));

    // A program that does not exist.
    player.setBackend(MusicPlayer::Backend{dir.filePath("no-such-player"), {"%f"}, "Missing"});
    CHECK(!player.play(midi, 1));
    CHECK(player.lastError().contains("could not be started"));

    CHECK(directoryDigest(game.root) == digest);
    return test::finish("music_player");
}
