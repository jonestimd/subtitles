#ifndef SUBTITLE_H
#define SUBTITLE_H

#include <QString>

#define TIME_FORMAT "\\d{2}:\\d{2}:\\d{2},\\d{2,3}"

struct Subtitle
{
    const QString imageFile;
    const QString startTime;
    const QString endTime;
    QString text;

    Subtitle(QStringList record);

    bool isValid() const;
};

#endif // SUBTITLE_H
