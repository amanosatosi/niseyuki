#pragma once

#include "EncodeJob.h"

#include <QObject>
#include <QProcess>

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
    void parseProgressLine(const QByteArray &line);

    QProcess m_process;
    EncodeJob m_currentJob;
    State m_state = State::Idle;
    double m_progress = 0.0;
    QString m_statusText;
};
