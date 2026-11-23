// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QHash>
#include <QStringList>
#include <QTranslator>

namespace eggbert {

// A translator that reads Qt's `.ts` catalogue (XML) directly, so the build
// needs no Linguist tools. Messages without a finished translation fall
// back to the English source text, as with any QTranslator.
class CatalogTranslator final : public QTranslator {
    Q_OBJECT

public:
    explicit CatalogTranslator(QObject *parent = nullptr);

    // Replaces the catalogue; false (and an empty catalogue) when the text
    // is not a well-formed `.ts` file.
    bool loadCatalog(const QByteArray &ts);
    // Loads the catalogue of a language ("cs") from the program's
    // resources; false when there is none.
    bool loadLanguage(const QString &language);

    [[nodiscard]] QString translate(const char *context, const char *sourceText,
                                    const char *disambiguation = nullptr,
                                    int n = -1) const override;
    [[nodiscard]] bool isEmpty() const override { return messages_.isEmpty(); }

    // Messages in the catalogue, and how many have a translation.
    [[nodiscard]] int messageCount() const { return messageCount_; }
    [[nodiscard]] int translatedCount() const { return static_cast<int>(messages_.size()); }
    [[nodiscard]] const QString &language() const { return language_; }

    struct Message {
        QByteArray context;
        QByteArray source;
        QStringList forms;
    };
    // Every message that has a translation, for checks.
    [[nodiscard]] QList<Message> translatedMessages() const;

    // Index of the plural form for a count in a language ("cs": one, two to
    // four, others; anything else: one, others).
    [[nodiscard]] static int pluralForm(const QString &language, int n);

private:
    // Key: context, a zero byte, source text. Value: the plural forms (one
    // entry for an ordinary message).
    QHash<QByteArray, QStringList> messages_;
    int messageCount_ = 0;
    QString language_;
};

// Languages the program ships catalogues for, besides English.
[[nodiscard]] QStringList availableLanguages();
// The language to use for a preference ("system", "en", or a language
// code): "en" when none applies.
[[nodiscard]] QString resolveLanguage(const QString &preference, const QString &systemLocale);

} // namespace eggbert
