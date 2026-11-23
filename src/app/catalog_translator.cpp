// SPDX-License-Identifier: GPL-3.0-only
#include "app/catalog_translator.hpp"

#include <QFile>
#include <QXmlStreamReader>

namespace eggbert {

CatalogTranslator::CatalogTranslator(QObject *parent) : QTranslator(parent) {}

bool CatalogTranslator::loadCatalog(const QByteArray &ts) {
    messages_.clear();
    messageCount_ = 0;
    language_.clear();
    QHash<QByteArray, QStringList> messages;
    int count = 0;
    QString language;
    QXmlStreamReader xml(ts);
    QByteArray context;
    bool sawRoot = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) {
            continue;
        }
        if (xml.name() == QLatin1String("TS")) {
            sawRoot = true;
            language = xml.attributes().value(QLatin1String("language")).toString().left(2);
        } else if (xml.name() == QLatin1String("name")) {
            context = xml.readElementText().toUtf8();
        } else if (xml.name() == QLatin1String("message")) {
            ++count;
            QByteArray source;
            QStringList forms;
            bool finished = false;
            while (xml.readNextStartElement()) {
                if (xml.name() == QLatin1String("source")) {
                    source = xml.readElementText().toUtf8();
                } else if (xml.name() == QLatin1String("translation")) {
                    finished = xml.attributes().value(QLatin1String("type")) !=
                               QLatin1String("unfinished");
                    QString text;
                    while (!xml.atEnd()) {
                        xml.readNext();
                        if (xml.isEndElement() && xml.name() == QLatin1String("translation")) {
                            break;
                        }
                        if (xml.isStartElement() && xml.name() == QLatin1String("numerusform")) {
                            forms << xml.readElementText();
                        } else if (xml.isCharacters()) {
                            text += xml.text();
                        }
                    }
                    if (forms.isEmpty()) {
                        forms << text;
                    }
                } else {
                    xml.skipCurrentElement();
                }
            }
            const bool usable = finished && !forms.isEmpty() &&
                                std::ranges::none_of(forms, [](const QString &form) {
                                    return form.trimmed().isEmpty();
                                });
            if (usable && !source.isEmpty()) {
                messages.insert(context + '\0' + source, forms);
            }
        }
    }
    if (xml.hasError() || !sawRoot) {
        return false;
    }
    messages_ = std::move(messages);
    messageCount_ = count;
    language_ = language;
    return true;
}

bool CatalogTranslator::loadLanguage(const QString &language) {
    QFile file(QStringLiteral(":/translations/translations/eggbert-editor_%1.ts").arg(language));
    return file.open(QIODevice::ReadOnly) && loadCatalog(file.readAll());
}

QList<CatalogTranslator::Message> CatalogTranslator::translatedMessages() const {
    QList<Message> result;
    for (auto item = messages_.constBegin(); item != messages_.constEnd(); ++item) {
        const auto split = item.key().indexOf('\0');
        result.push_back({item.key().left(split), item.key().mid(split + 1), item.value()});
    }
    return result;
}

int CatalogTranslator::pluralForm(const QString &language, int n) {
    if (language == QLatin1String("cs")) {
        return n == 1 ? 0 : (n >= 2 && n <= 4 ? 1 : 2);
    }
    return n == 1 ? 0 : 1;
}

QString CatalogTranslator::translate(const char *context, const char *sourceText, const char *,
                                     int n) const {
    if (context == nullptr || sourceText == nullptr) {
        return {};
    }
    const auto found = messages_.constFind(QByteArray(context) + '\0' + QByteArray(sourceText));
    if (found == messages_.constEnd()) {
        return {}; // Qt then uses the source text
    }
    const auto &forms = found.value();
    if (n < 0 || forms.size() == 1) {
        return forms.front();
    }
    const auto index = pluralForm(language_, n);
    return forms[std::min<qsizetype>(index, forms.size() - 1)];
}

QStringList availableLanguages() {
    return {QStringLiteral("cs")};
}

QString resolveLanguage(const QString &preference, const QString &systemLocale) {
    const auto wanted =
        preference == QLatin1String("system") || preference.isEmpty() ? systemLocale.left(2)
                                                                       : preference;
    return availableLanguages().contains(wanted) ? wanted : QStringLiteral("en");
}

} // namespace eggbert
