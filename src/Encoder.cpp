#include "Encoder.h"

#include <QRegularExpression>

#include <algorithm>

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
    emit stateChanged(m_state);
    emit progressChanged(m_progress);
    emit statusTextChanged(m_statusText);

    const QString ffmpegExecutable = QStringLiteral("ffmpeg");
    const QStringList args = buildFfmpegArguments(job);

    m_process.setProgram(ffmpegExecutable);
    m_process.setArguments(args);
    m_process.start();

    emit messageReceived(QStringLiteral("Starting ffmpeg with arguments: %1").arg(args.join(' ')));
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
            parseProgressLine(line);
            emit messageReceived(QString::fromUtf8(line));
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
    emit stateChanged(m_state);
    emit progressChanged(m_progress);
    emit statusTextChanged(m_statusText);
    emit finished(success);
}

QStringList Encoder::buildFfmpegArguments(const EncodeJob &job) const
{
    // Placeholder command: simply copy the input to the output with minimal processing.
    QStringList args;
    args << QStringLiteral("-y");
    args << QStringLiteral("-i") << job.videoPath;
    if (!job.subtitlePath.isEmpty()) {
        args << QStringLiteral("-i") << job.subtitlePath;
        args << QStringLiteral("-c:s") << QStringLiteral("copy");
    }

    if (job.telegramMode) {
        args << QStringLiteral("-c:v") << QStringLiteral("libx264");
        args << QStringLiteral("-c:a") << QStringLiteral("aac");
        args << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p");
        args << QStringLiteral("-profile:v") << QStringLiteral("high");
    } else {
        args << QStringLiteral("-c:v") << QStringLiteral("copy");
        args << QStringLiteral("-c:a") << QStringLiteral("copy");
    }

    args << job.resolvedOutputPath();
    return args;
}

void Encoder::parseProgressLine(const QByteArray &line)
{
    static const QRegularExpression frameRegex(QStringLiteral("frame=\\s*(\\d+)"));
    static const QRegularExpression timeRegex(QStringLiteral("time=([0-9:.]+)"));

    auto matchFrame = frameRegex.match(QString::fromUtf8(line));
    auto matchTime = timeRegex.match(QString::fromUtf8(line));

    if (matchFrame.hasMatch() || matchTime.hasMatch()) {
        if (m_state == State::Indexing) {
            m_state = State::Encoding;
            emit stateChanged(m_state);
        }

        if (matchFrame.hasMatch()) {
            bool ok = false;
            const double frame = matchFrame.captured(1).toDouble(&ok);
            if (ok && frame > 0) {
                m_progress = std::min(frame / 1000.0, 1.0);
                emit progressChanged(m_progress);
            }
        }

        if (matchTime.hasMatch()) {
            m_statusText = tr("Encoding (%1)").arg(matchTime.captured(1));
            emit statusTextChanged(m_statusText);
        }
    }
}
