#pragma once

#include "Encoder.h"
#include "widgets/StartButton.h"

#include <QMainWindow>
#include <QPointer>

class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QSpinBox;
class QTableWidget;
class QTabWidget;
class QTextEdit;
class QToolBar;
class QVideoWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onAddFile();
    void onRemoveSelected();
    void onStartClicked();
    void onStopClicked();

    void onEncoderStateChanged(Encoder::State state);
    void onEncoderProgressChanged(double progress);
    void onEncoderStatusChanged(const QString &text);
    void onEncoderMessageReceived(const QString &message);
    void onEncoderFinished(bool success);

private:
    void createToolBar();
    QWidget *createCentral();
    QWidget *createQueuePanel();
    QWidget *createPreviewPanel();
    QWidget *createMainTab();
    QWidget *createVideoTab();
    QWidget *createAudioTab();
    QWidget *createLogoTab();
    QWidget *createLogTab();

    void appendLog(const QString &line);
    void updateStartStopAvailability();

    Encoder m_encoder;
    StartButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QComboBox *m_priorityCombo = nullptr;
    QTabWidget *m_tabWidget = nullptr;
    QTableWidget *m_queueTable = nullptr;
    QTextEdit *m_logView = nullptr;
    QLabel *m_statusLabel = nullptr;
};
