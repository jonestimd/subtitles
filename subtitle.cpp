#include "subtitle.h"
#include <QList>
#include <QRegularExpression>

Q_GLOBAL_STATIC(QRegularExpression, timeFormat, "^" TIME_FORMAT "$");

Subtitle::Subtitle(QStringList record)
    : imageFile{record.length() > 0 ? record[0] : ""}
    , startTime{record.length() > 1 ? record[1] : ""}
    , endTime{record.length() > 2 ? record[2] : ""}
    , text() {}

bool Subtitle::isValid() const {
    return imageFile.length() > 4 && imageFile.endsWith(".bmp")
        && timeFormat->match(startTime).hasMatch()
        && timeFormat->match(endTime).hasMatch();
}
