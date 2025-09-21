#include "StartButton.h"

#include <QPainter>
#include <QPaintEvent>
#include <QMarginsF>
#include <QPolygonF>
#include <QtMath>

#include <cmath>

#include <algorithm>

namespace {
constexpr int kBaseSize = 48;
constexpr int kRingThickness = 5;
constexpr int kAnimationIntervalMs = 16;
}

StartButton::StartButton(QWidget *parent)
    : QAbstractButton(parent)
{
    setCheckable(false);
    setCursor(Qt::PointingHandCursor);
    setToolTip(tr("Start encoding"));
    connect(&m_timer, &QTimer::timeout, this, [this]() {
        m_rotation = std::fmod(m_rotation + 3.6, 360.0);
        update();
    });
    m_timer.setInterval(kAnimationIntervalMs);
    updateAnimationTimer();
}

void StartButton::setState(State state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    updateAnimationTimer();
    update();
    emit stateChanged(m_state);
}

void StartButton::setProgress(double progress)
{
    const double clamped = std::clamp(progress, 0.0, 1.0);
    if (qFuzzyCompare(m_progress, clamped)) {
        return;
    }
    m_progress = clamped;
    update();
}

void StartButton::setReducedMotion(bool reducedMotion)
{
    if (m_reducedMotion == reducedMotion) {
        return;
    }
    m_reducedMotion = reducedMotion;
    updateAnimationTimer();
}

QSize StartButton::sizeHint() const
{
    return QSize(kBaseSize, kBaseSize);
}

void StartButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF bounds = QRectF(0, 0, width(), height()).marginsRemoved(QMarginsF(4, 4, 4, 4));
    const QPointF center = bounds.center();
    const qreal radius = std::min(bounds.width(), bounds.height()) / 2.0;

    painter.setBrush(palette().window());
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(center, radius, radius);

    QPen ringPen(palette().highlight(), kRingThickness);
    ringPen.setCapStyle(Qt::RoundCap);
    painter.setPen(ringPen);

    const QRectF ringRect(center.x() - radius + kRingThickness / 2.0,
                          center.y() - radius + kRingThickness / 2.0,
                          2 * (radius - kRingThickness / 2.0),
                          2 * (radius - kRingThickness / 2.0));

    const int startAngle = static_cast<int>(m_rotation * 16);

    switch (m_state) {
    case State::Idle: {
        painter.setPen(QPen(palette().mid(), kRingThickness));
        painter.drawArc(ringRect, 0, 360 * 16);
        painter.setPen(Qt::NoPen);
        painter.setBrush(palette().highlight());
        const QPolygonF triangle{
            QPointF(center.x() - radius / 3.0, center.y() - radius / 2.5),
            QPointF(center.x() - radius / 3.0, center.y() + radius / 2.5),
            QPointF(center.x() + radius / 2.0, center.y())
        };
        painter.drawPolygon(triangle);
        break;
    }
    case State::Indexing: {
        const int spanAngle = 270 * 16;
        painter.drawArc(ringRect, startAngle, spanAngle);
        painter.setPen(QPen(palette().mid(), kRingThickness));
        painter.drawArc(ringRect, startAngle + spanAngle, (360 * 16) - spanAngle);

        painter.setBrush(palette().highlight());
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(center, radius / 3.5, radius / 3.5);
        break;
    }
    case State::Encoding: {
        const int spanAngle = static_cast<int>(360 * m_progress * 16);
        painter.drawArc(ringRect, startAngle, spanAngle);
        painter.setPen(QPen(palette().mid(), kRingThickness));
        painter.drawArc(ringRect, startAngle + spanAngle, (360 * 16) - spanAngle);

        painter.setPen(Qt::NoPen);
        painter.setBrush(palette().highlight());
        painter.drawRect(center.x() - radius / 3.5, center.y() - radius / 3.5,
                         radius / 1.75, radius / 1.75);
        break;
    }
    }
}

void StartButton::updateAnimationTimer()
{
    const bool shouldAnimate = !m_reducedMotion && (m_state == State::Indexing || m_state == State::Encoding);
    if (shouldAnimate) {
        if (!m_timer.isActive()) {
            m_timer.start();
        }
    } else {
        if (m_timer.isActive()) {
            m_timer.stop();
        }
        m_rotation = 0.0;
        update();
    }
}
