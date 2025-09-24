#include "Encoder.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStringList>
#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace {
QString formatTimecode(qint64 ms)
{
    if (ms <= 0) {
        return QStringLiteral("00:00:00");
    }
    const qint64 totalSeconds = ms / 1000;
    const qint64 hours = totalSeconds / 3600;
    const int minutes = static_cast<int>((totalSeconds % 3600) / 60);
    const int seconds = static_cast<int>(totalSeconds % 60);
    return QStringLiteral("%1:%2:%3")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

QString formatSeconds(double seconds)
{
    if (seconds < 0.0) {
        seconds = 0.0;
    }
    return QString::number(seconds, 'f', seconds >= 10.0 ? 2 : 3);
}

bool parseTimeToSeconds(const QString &text, double &secondsOut)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    bool ok = false;
    double numeric = trimmed.toDouble(&ok);
    if (ok) {
        secondsOut = numeric;
        return true;
    }

    const QStringList parts = trimmed.split(QLatin1Char(':'));
    if (parts.isEmpty()) {
        return false;
    }

    double multiplier = 1.0;
    double total = 0.0;
    for (int i = parts.size() - 1; i >= 0; --i) {
        const double value = parts.at(i).toDouble(&ok);
        if (!ok) {
            return false;
        }
        total += value * multiplier;
        multiplier *= 60.0;
    }
    secondsOut = total;
    return true;
}

QString sanitizeFilterPath(const QString &path)
{
    QString sanitized = QDir::toNativeSeparators(path);
    sanitized.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    sanitized.replace(QLatin1Char('\''), QStringLiteral("\\'"));
    return sanitized;
}

QString videoCodecForJob(const EncodeJob &job)
{
    if (job.telegramMode) {
        return QStringLiteral("libx264");
    }
    const QString encoder = job.videoSettings.encoder.toLower();
    if (encoder == QLatin1String("x265")) {
        return QStringLiteral("libx265");
    }
    if (encoder == QLatin1String("qsv")) {
        return QStringLiteral("h264_qsv");
    }
    if (encoder == QLatin1String("nvenc")) {
        return QStringLiteral("h264_nvenc");
    }
    if (encoder == QLatin1String("amd")) {
        return QStringLiteral("h264_amf");
    }
    return QStringLiteral("libx264");
}

QString presetForJob(const EncodeJob &job)
{
    const QString encoder = job.videoSettings.encoder.toLower();
    const QString preset = job.videoSettings.preset;

    if (encoder == QLatin1String("nvenc")) {
        if (preset == QLatin1String("veryslow") || preset == QLatin1String("slower")) {
            return QStringLiteral("p1");
        }
        if (preset == QLatin1String("slow")) {
            return QStringLiteral("p2");
        }
        if (preset == QLatin1String("medium")) {
            return QStringLiteral("p4");
        }
        if (preset == QLatin1String("fast")) {
            return QStringLiteral("p5");
        }
        if (preset == QLatin1String("faster")) {
            return QStringLiteral("p6");
        }
        if (preset == QLatin1String("veryfast")) {
            return QStringLiteral("p7");
        }
        return QStringLiteral("p4");
    }

    if (encoder == QLatin1String("amd")) {
        if (preset == QLatin1String("veryslow") || preset == QLatin1String("slower") || preset == QLatin1String("slow")) {
            return QStringLiteral("quality");
        }
        if (preset == QLatin1String("medium") || preset == QLatin1String("fast")) {
            return QStringLiteral("balanced");
        }
        return QStringLiteral("speed");
    }

    if (encoder == QLatin1String("qsv")) {
        if (preset == QLatin1String("veryslow") || preset == QLatin1String("slower")) {
            return QStringLiteral("veryslow");
        }
        if (preset == QLatin1String("slow")) {
            return QStringLiteral("slow");
        }
        if (preset == QLatin1String("fast")) {
            return QStringLiteral("fast");
        }
        if (preset == QLatin1String("faster") || preset == QLatin1String("veryfast")) {
            return QStringLiteral("veryfast");
        }
        return QStringLiteral("medium");
    }

    return preset.isEmpty() ? QStringLiteral("medium") : preset;
}

QStringList quoteArguments(const QStringList &args)
{
    QStringList quoted;
    quoted.reserve(args.size());
    for (const QString &arg : args) {
        if (arg.contains(QLatin1Char(' ')) || arg.contains(QLatin1Char('"'))) {
            QString escaped = arg;
            escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
            quoted << QStringLiteral("\"%1\"").arg(escaped);
        } else {
            quoted << arg;
        }
    }
    return quoted;
}
} // namespace

Encoder::Encoder(QObject *parent)
    : QObject(parent)
{
    connect(&m_process, &QProcess::readyReadStandardError, this, &Encoder::handleProcessOutput);
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &Encoder::handleProcessOutput);
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &Encoder::handleProcessFinished);
}

void Encoder::startEncoding(const EncodeJob &job)
{
    if (m_state != State::Idle) {
        return;
    }

    m_currentJob = job;
    m_progress = 0.0;
    m_statusText = tr("Indexing");
    m_state = State::Indexing;
    m_totalDurationMs = 0;
    emit stateChanged(m_state);
    emit progressChanged(m_progress);
    emit statusTextChanged(m_statusText);

    m_ffmpegPath = resolveFfmpegExecutable();
    if (m_ffmpegPath.isEmpty()) {
        emitWarning(tr("Unable to locate bundled ffmpeg executable."));
        m_state = State::Idle;
        emit stateChanged(m_state);
        emit finished(false);
        return;
    }

    m_ffprobePath = resolveFfprobeExecutable();
    if (m_currentJob.durationMs > 0) {
        m_totalDurationMs = m_currentJob.durationMs;
    } else if (!m_ffprobePath.isEmpty()) {
        m_totalDurationMs = probeDurationMs(m_currentJob.videoPath, m_ffprobePath);
        m_currentJob.durationMs = m_totalDurationMs;
    } else {
        emitWarning(tr("ffprobe not found; progress percentage may be limited."));
    }

    if (!m_currentJob.introOutroInfo.introPath.isEmpty() || !m_currentJob.introOutroInfo.outroPath.isEmpty()) {
        emitWarning(tr("Intro/outro stitching is not implemented yet and will be ignored."));
    }
    if (!m_currentJob.introOutroInfo.thumbnailPath.isEmpty()) {
        emitWarning(tr("Thumbnail injection is not implemented yet and will be ignored."));
    }
    if (!m_currentJob.logoSettings.imagePath.isEmpty()) {
        emitWarning(tr("Logo overlay is not implemented yet and will be ignored."));
    }
    if (!m_currentJob.additionalSubtitles.isEmpty()) {
        emitWarning(tr("Additional subtitle tracks are not implemented yet and will be ignored."));
    }

    const QStringList args = buildFfmpegArguments(m_currentJob);

    m_process.setProgram(m_ffmpegPath);
    m_process.setArguments(args);
    m_process.setProcessChannelMode(QProcess::SeparateChannels);

    m_process.start();
    if (!m_process.waitForStarted(5000)) {
        emitWarning(tr("Failed to start ffmpeg: %1").arg(m_process.errorString()));
        m_state = State::Idle;
        emit stateChanged(m_state);
        emit finished(false);
        return;
    }

    QStringList printableArgs = args;
    printableArgs.prepend(QDir::toNativeSeparators(m_ffmpegPath));
    emit messageReceived(tr("Starting ffmpeg: %1").arg(quoteArguments(printableArgs).join(QLatin1Char(' '))));
}

void Encoder::stopEncoding()
{
    if (m_state == State::Idle) {
        return;
    }

    m_state = State::Stopping;
    emit stateChanged(m_state);

    if (m_process.state() != QProcess::NotRunning) {
        m_process.write("q\n");
        if (!m_process.waitForFinished(2000)) {
            m_process.kill();
        }
    }
}

void Encoder::handleProcessOutput()
{
    auto forwardData = [&](const QByteArray &data) {
        const QList<QByteArray> lines = data.split('\n');
        for (const QByteArray &line : lines) {
            if (line.trimmed().isEmpty()) {
                continue;
            }
            const bool handled = parseProgressLine(line);
            if (!handled) {
                emit messageReceived(QString::fromUtf8(line));
            }
        }
    };

    forwardData(m_process.readAllStandardOutput());
    forwardData(m_process.readAllStandardError());
}

void Encoder::handleProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    const bool success = (exitCode == 0 && status == QProcess::NormalExit);
    m_state = State::Idle;
    m_progress = 0.0;
    m_statusText = success ? tr("Completed") : tr("Failed");
    m_ffmpegPath.clear();
    m_ffprobePath.clear();
    m_totalDurationMs = 0;
    emit stateChanged(m_state);
    emit progressChanged(m_progress);
    emit statusTextChanged(m_statusText);
    emit finished(success);
}

QStringList Encoder::buildFfmpegArguments(const EncodeJob &job) const
{
    QStringList args;
    args << QStringLiteral("-hide_banner");
    args << QStringLiteral("-y");
    args << QStringLiteral("-progress") << QStringLiteral("pipe:1");
    args << QStringLiteral("-nostats");

    double startSeconds = 0.0;
    double endSeconds = 0.0;
    const bool hasStart = job.cutSettings.enabled && parseTimeToSeconds(job.cutSettings.startTime, startSeconds);
    const bool hasEnd = job.cutSettings.enabled && parseTimeToSeconds(job.cutSettings.endTime, endSeconds);

    if (hasStart) {
        const QString startToken = job.cutSettings.startTime.trimmed().isEmpty()
            ? formatSeconds(startSeconds)
            : job.cutSettings.startTime.trimmed();
        args << QStringLiteral("-ss") << startToken;
    }

    args << QStringLiteral("-i") << job.videoPath;

    if (hasEnd) {
        if (hasStart && endSeconds > startSeconds) {
            args << QStringLiteral("-t") << formatSeconds(endSeconds - startSeconds);
        } else if (!hasStart && endSeconds > 0.0) {
            const QString endToken = job.cutSettings.endTime.trimmed().isEmpty()
                ? formatSeconds(endSeconds)
                : job.cutSettings.endTime.trimmed();
            args << QStringLiteral("-to") << endToken;
        }
    }

    args << QStringLiteral("-map") << QStringLiteral("0:v:0");

    QString audioMap = QStringLiteral("0:a:0");
    if (!job.audioSettings.preferredTrackId.trimmed().isEmpty()) {
        audioMap = job.audioSettings.preferredTrackId.trimmed();
        if (!audioMap.startsWith(QStringLiteral("0:"))) {
            audioMap = QStringLiteral("0:%1").arg(audioMap);
        }
    }
    args << QStringLiteral("-map") << audioMap;

    const QStringList videoFilters = buildVideoFilters(job);
    if (!videoFilters.isEmpty()) {
        args << QStringLiteral("-vf") << videoFilters.join(QLatin1Char(','));
    }

    const QStringList audioFilters = buildAudioFilters(job);
    if (!audioFilters.isEmpty()) {
        args << QStringLiteral("-af") << audioFilters.join(QLatin1Char(','));
    }

    const QString videoCodec = videoCodecForJob(job);
    args << QStringLiteral("-c:v") << videoCodec;

    const QString preset = presetForJob(job);
    if (!preset.isEmpty()) {
        if (videoCodec == QLatin1String("h264_amf")) {
            args << QStringLiteral("-quality") << preset;
        } else {
            args << QStringLiteral("-preset") << preset;
        }
    }

    const double quality = std::clamp(job.videoSettings.qualityValue, 0.0, 51.0);
    if (videoCodec == QLatin1String("libx264") || videoCodec == QLatin1String("libx265")) {
        args << QStringLiteral("-crf") << QString::number(quality, 'f', 1);
    } else if (videoCodec == QLatin1String("h264_nvenc")) {
        args << QStringLiteral("-cq") << QString::number(quality, 'f', 1);
        args << QStringLiteral("-b:v") << QStringLiteral("0");
    } else if (videoCodec == QLatin1String("h264_qsv")) {
        args << QStringLiteral("-global_quality") << QString::number(static_cast<int>(std::round(quality)));
        args << QStringLiteral("-look_ahead") << QStringLiteral("1");
    } else if (videoCodec == QLatin1String("h264_amf")) {
        args << QStringLiteral("-q:v") << QString::number(quality, 'f', 1);
    }

    QString audioCodec = job.audioSettings.codec.isEmpty() ? QStringLiteral("aac") : job.audioSettings.codec.toLower();
    if (job.telegramMode) {
        audioCodec = QStringLiteral("aac");
    }
    args << QStringLiteral("-c:a") << audioCodec;

    if (audioCodec == QLatin1String("aac")) {
        const int bitrate = job.audioSettings.bitrateKbps > 0 ? job.audioSettings.bitrateKbps : 192;
        args << QStringLiteral("-b:a") << QStringLiteral("%1k").arg(bitrate);
        args << QStringLiteral("-profile:a") << QStringLiteral("aac_low");
    }

    if (job.telegramMode) {
        args << QStringLiteral("-movflags") << QStringLiteral("+faststart");
        args << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p");
        args << QStringLiteral("-profile:v") << QStringLiteral("high");
        args << QStringLiteral("-level:v") << QStringLiteral("4.1");
    }

    args << QStringLiteral("-map_metadata") << QStringLiteral("-1");
    args << QStringLiteral("-sn");

    args << QDir::toNativeSeparators(job.resolvedOutputPath());
    return args;
}

QStringList Encoder::buildVideoFilters(const EncodeJob &job) const
{
    QStringList filters;

    const QString resizeMode = job.videoSettings.resizeMode.toLower();
    if (resizeMode == QLatin1String("1080p")) {
        filters << QStringLiteral("scale=-2:1080:flags=lanczos");
    } else if (resizeMode == QLatin1String("720p")) {
        filters << QStringLiteral("scale=-2:720:flags=lanczos");
    } else if (resizeMode == QLatin1String("480p")) {
        filters << QStringLiteral("scale=-2:480:flags=lanczos");
    } else if (resizeMode == QLatin1String("custom")) {
        if (job.videoSettings.customSize.isValid()) {
            filters << QStringLiteral("scale=%1:%2:flags=lanczos")
                          .arg(job.videoSettings.customSize.width())
                          .arg(job.videoSettings.customSize.height());
        } else {
            emitWarning(tr("Custom resize requested but size is invalid; keeping source resolution."));
        }
    }

    if (!job.subtitlePath.isEmpty()) {
        const QString renderer = job.rendererMode.isEmpty() ? QStringLiteral("Auto") : job.rendererMode;
        if (renderer == QLatin1String("VSFilter") || renderer == QLatin1String("VSFilterMod")) {
            emitWarning(tr("%1 renderer is unavailable; using libass via ffmpeg subtitles filter.").arg(renderer));
        }
        filters << QStringLiteral("subtitles='%1'").arg(sanitizeFilterPath(job.subtitlePath));
    }

    return filters;
}

QStringList Encoder::buildAudioFilters(const EncodeJob &job) const
{
    QStringList filters;
    if (std::fabs(static_cast<double>(job.audioSettings.volumeSource) - 1.0) > 0.01) {
        filters << QStringLiteral("volume=%1").arg(QString::number(job.audioSettings.volumeSource, 'f', 2));
    }
    return filters;
}

QString Encoder::resolveFfmpegExecutable() const
{
    const QByteArray overrideValue = qgetenv("NISEYUKI_FFMPEG");
    if (!overrideValue.isEmpty()) {
        const QFileInfo overrideInfo(QString::fromLocal8Bit(overrideValue));
        if (overrideInfo.exists() && overrideInfo.isFile()) {
            return overrideInfo.absoluteFilePath();
        }
    }
    return resolveExecutable(QStringLiteral("ffmpeg"));
}

QString Encoder::resolveFfprobeExecutable() const
{
    const QByteArray overrideValue = qgetenv("NISEYUKI_FFPROBE");
    if (!overrideValue.isEmpty()) {
        const QFileInfo overrideInfo(QString::fromLocal8Bit(overrideValue));
        if (overrideInfo.exists() && overrideInfo.isFile()) {
            return overrideInfo.absoluteFilePath();
        }
    }
    return resolveExecutable(QStringLiteral("ffprobe"));
}

QString Encoder::resolveExecutable(const QString &program) const
{
    QString baseName = program;
#ifdef Q_OS_WIN
    if (!baseName.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
        baseName.append(QStringLiteral(".exe"));
    }
#endif

    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates{
        QDir(appDir).filePath(QStringLiteral("ffmpeg/bin/%1").arg(baseName)),
        QDir(appDir).filePath(QStringLiteral("ffmpeg/%1").arg(baseName)),
        QDir(appDir).filePath(baseName),
        program,
        baseName
    };

    for (const QString &candidate : candidates) {
        const QFileInfo info(candidate);
        if (info.exists() && info.isFile()) {
            return info.absoluteFilePath();
        }
    }
    return QString();
}

qint64 Encoder::probeDurationMs(const QString &videoPath, const QString &ffprobePath) const
{
    if (ffprobePath.isEmpty()) {
        return 0;
    }

    QProcess probe;
    QStringList args{
        QStringLiteral("-v"), QStringLiteral("error"),
        QStringLiteral("-show_entries"), QStringLiteral("format=duration"),
        QStringLiteral("-of"), QStringLiteral("default=noprint_wrappers=1:nokey=1"),
        videoPath
    };
    probe.start(ffprobePath, args);
    if (!probe.waitForFinished(8000)) {
        probe.kill();
        probe.waitForFinished();
        emitWarning(tr("ffprobe timed out while reading duration."));
        return 0;
    }

    const QString output = QString::fromUtf8(probe.readAllStandardOutput()).trimmed();
    bool ok = false;
    const double seconds = output.toDouble(&ok);
    if (ok && seconds > 0.0) {
        return static_cast<qint64>(seconds * 1000.0);
    }
    return 0;
}

void Encoder::emitWarning(const QString &message) const
{
    emit messageReceived(QStringLiteral("[warn] %1").arg(message));
}

bool Encoder::parseProgressLine(const QByteArray &line)
{
    const QString text = QString::fromUtf8(line).trimmed();
    if (text.isEmpty()) {
        return true;
    }

    const int equalsIndex = text.indexOf(QLatin1Char('='));
    if (equalsIndex > 0) {
        const QString key = text.left(equalsIndex).trimmed();
        const QString value = text.mid(equalsIndex + 1).trimmed();

        if (key == QLatin1String("out_time_ms")) {
            bool ok = false;
            const qint64 outTimeMicro = value.toLongLong(&ok);
            if (ok) {
                const qint64 outTimeMs = outTimeMicro / 1000;
                if (m_state == State::Indexing) {
                    m_state = State::Encoding;
                    emit stateChanged(m_state);
                }
                if (m_totalDurationMs > 0) {
                    const double ratio = static_cast<double>(outTimeMs) / static_cast<double>(m_totalDurationMs);
                    const double newProgress = std::clamp(ratio, 0.0, 1.0);
                    if (std::fabs(newProgress - m_progress) > 0.0005) {
                        m_progress = newProgress;
                        emit progressChanged(m_progress);
                    }
                }
                m_statusText = tr("Encoding (%1)").arg(formatTimecode(outTimeMs));
                emit statusTextChanged(m_statusText);
            }
            return true;
        }

        if (key == QLatin1String("out_time")) {
            double seconds = 0.0;
            if (parseTimeToSeconds(value, seconds)) {
                if (m_state == State::Indexing) {
                    m_state = State::Encoding;
                    emit stateChanged(m_state);
                }
                const qint64 outTimeMs = static_cast<qint64>(seconds * 1000.0);
                if (m_totalDurationMs > 0) {
                    const double ratio = static_cast<double>(outTimeMs) / static_cast<double>(m_totalDurationMs);
                    const double newProgress = std::clamp(ratio, 0.0, 1.0);
                    if (std::fabs(newProgress - m_progress) > 0.0005) {
                        m_progress = newProgress;
                        emit progressChanged(m_progress);
                    }
                }
                m_statusText = tr("Encoding (%1)").arg(formatTimecode(outTimeMs));
                emit statusTextChanged(m_statusText);
            }
            return true;
        }

        if (key == QLatin1String("progress")) {
            if (value == QLatin1String("end")) {
                m_progress = 1.0;
                emit progressChanged(m_progress);
            }
            return true;
        }

        if (key == QLatin1String("frame")) {
            if (m_state == State::Indexing) {
                m_state = State::Encoding;
                emit stateChanged(m_state);
            }
            if (m_totalDurationMs == 0) {
                m_statusText = tr("Encoding (frame %1)").arg(value);
                emit statusTextChanged(m_statusText);
            }
            return true;
        }

        if (key == QLatin1String("speed")) {
            m_statusText = tr("Encoding speed %1").arg(value);
            emit statusTextChanged(m_statusText);
            return true;
        }

        return false;
    }

    static const QRegularExpression frameRegex(QStringLiteral("frame=\\s*(\\d+)"));
    static const QRegularExpression timeRegex(QStringLiteral("time=([0-9:.]+)"));

    const QRegularExpressionMatch matchFrame = frameRegex.match(text);
    const QRegularExpressionMatch matchTime = timeRegex.match(text);

    bool handled = false;
    if (matchTime.hasMatch()) {
        double seconds = 0.0;
        if (parseTimeToSeconds(matchTime.captured(1), seconds)) {
            if (m_state == State::Indexing) {
                m_state = State::Encoding;
                emit stateChanged(m_state);
            }
            const qint64 outTimeMs = static_cast<qint64>(seconds * 1000.0);
            if (m_totalDurationMs > 0) {
                const double ratio = static_cast<double>(outTimeMs) / static_cast<double>(m_totalDurationMs);
                m_progress = std::clamp(ratio, 0.0, 1.0);
                emit progressChanged(m_progress);
            }
            m_statusText = tr("Encoding (%1)").arg(formatTimecode(outTimeMs));
            emit statusTextChanged(m_statusText);
            handled = true;
        }
    }

    if (matchFrame.hasMatch()) {
        handled = true;
        if (m_state == State::Indexing) {
            m_state = State::Encoding;
            emit stateChanged(m_state);
        }
        if (m_totalDurationMs == 0) {
            m_statusText = tr("Encoding (frame %1)").arg(matchFrame.captured(1));
            emit statusTextChanged(m_statusText);
        }
    }

    return handled;
}
