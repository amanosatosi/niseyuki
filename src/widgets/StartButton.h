#pragma once

#include <QAbstractButton>
#include <QTimer>

class StartButton : public QAbstractButton
{
    Q_OBJECT
public:
    enum class State {
        Idle,
        Indexing,
        Encoding
    };

    explicit StartButton(QWidget *parent = nullptr);

    void setState(State state);
    void setProgress(double progress);
    void setReducedMotion(bool reducedMotion);

    [[nodiscard]] State state() const noexcept { return m_state; }
    [[nodiscard]] double progress() const noexcept { return m_progress; }
    [[nodiscard]] bool reducedMotion() const noexcept { return m_reducedMotion; }

    QSize sizeHint() const override;

signals:
    void stateChanged(State state);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void updateAnimationTimer();

    State m_state = State::Idle;
    double m_progress = 0.0;
    bool m_reducedMotion = false;
    qreal m_rotation = 0.0;
    QTimer m_timer;
};
