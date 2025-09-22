#pragma once

#include <QDir>
#include <QFileInfo>
#include <QPoint>
#include <QSize>

#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

struct SubtitleInfo {
    QString path;
    QString rendererOverride; // VSFilter, VSFilterMod, libass
};

struct IntroOutroInfo {
    QString introPath;
    QString outroPath;
    QString logoPath;
};

struct AudioSettings {
    QString codec; // AAC, FLAC
    int bitrateKbps = 192;
    QString preferredTrackId;
    float volumeSource = 1.0f;
    float volumeIntro = 1.0f;
    float volumeOutro = 1.0f;
};

struct VideoSettings {
    QString encoder; // x264, x265, etc
    QString preset;
    double qualityValue = 20.0; // CRF or CQ
    QString resizeMode; // None, 1080p, etc
    QSize customSize;
};

struct LogoSettings {
    QString imagePath;
    QString placement; // corners / custom
    QPoint customPosition;
    float opacity = 1.0f;
    QString visibility; // always, intro, outro, timed
    int visibleDuration = 0;
    int visibleInterval = 0;
};

struct CutSettings {
    bool enabled = false;
    QString startTime;
    QString endTime;
};

struct EncodeJob {
    QString videoPath;
    QString subtitlePath;
    SubtitleInfo subtitleInfo;
    IntroOutroInfo introOutroInfo;
    AudioSettings audioSettings;
    VideoSettings videoSettings;
    LogoSettings logoSettings;
    CutSettings cutSettings;
    QString rendererMode = QStringLiteral("Auto");
    bool telegramMode = false;
    QString outputFile;
    QString globalOutputFolder;

    QString resolvedOutputPath() const;
};

inline QString EncodeJob::resolvedOutputPath() const
{
    if (!outputFile.isEmpty()) {
        return outputFile;
    }
    if (!globalOutputFolder.isEmpty()) {
        QFileInfo fi(videoPath);
        const QString baseName = fi.completeBaseName();
        QString candidate = QDir(globalOutputFolder).filePath(baseName + QStringLiteral(".mp4"));
        if (!telegramMode) {
            candidate.chop(4); // remove .mp4
            candidate.append(QStringLiteral(".mkv"));
        }
        return candidate;
    }

    QFileInfo fi(videoPath);
    QString extension = telegramMode ? QStringLiteral(".mp4") : QStringLiteral(".mkv");
    return QDir(fi.absolutePath()).filePath(fi.completeBaseName() + extension);
}
