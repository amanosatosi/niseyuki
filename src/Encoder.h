#pragma once

#include "EncodeJob.h"

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

class Encoder : public QObject
{
    Q_OBJECT
public:
    enum class State {
        Idle,
        Indexing,
        Encoding,
        Stopping
    };

    explicit Encoder(QObject *parent = nullptr);

    void startEncoding(const EncodeJob &job);
    void stopEncoding();

    [[nodiscard]] State state() const noexcept { return m_state; }
    [[nodiscard]] double progress() const noexcept { return m_progress; }
    [[nodiscard]] QString statusText() const { return m_statusText; }

signals:
    void stateChanged(Encoder::State state);
    void progressChanged(double progress);
    void statusTextChanged(const QString &text);
    void messageReceived(const QString &message);
    void finished(bool success);

private slots:
    void handleProcessOutput();
    void handleProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    QStringList buildFfmpegArguments(const EncodeJob &job) const;
    bool parseProgressLine(const QByteArray &line);
    QString resolveFfmpegExecutable() const;
    QString resolveFfprobeExecutable() const;
    QString resolveExecutable(const QString &program) const;
    qint64 probeDurationMs(const QString &videoPath, const QString &ffprobePath) const;
    QStringList buildVideoFilters(const EncodeJob &job) const;
    QStringList buildAudioFilters(const EncodeJob &job) const;
    void emitWarning(const QString &message) const;

    QProcess m_process;
    EncodeJob m_currentJob;
    State m_state = State::Idle;
    double m_progress = 0.0;
    QString m_statusText;
    QString m_ffmpegPath;
    QString m_ffprobePath;
    qint64 m_totalDurationMs = 0;
};
