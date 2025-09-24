#pragma once

#include "Encoder.h"
#include "widgets/StartButton.h"

#include <QMainWindow>
#include <QPointer>
#include <QVector>

class QCheckBox;
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
    struct MainTabControls {
        QLineEdit *autoSubtitlePath = nullptr;
        QTextEdit *additionalSubtitleList = nullptr;
        QComboBox *rendererCombo = nullptr;
        QLineEdit *introPath = nullptr;
        QLineEdit *outroPath = nullptr;
        QLineEdit *thumbnailPath = nullptr;
        QCheckBox *cutEnable = nullptr;
        QLineEdit *cutStart = nullptr;
        QLineEdit *cutEnd = nullptr;
        QCheckBox *telegramToggle = nullptr;
        QLineEdit *outputFile = nullptr;
    };

    struct VideoTabControls {
        QComboBox *encoderCombo = nullptr;
        QComboBox *presetCombo = nullptr;
        QSlider *qualitySlider = nullptr;
        QComboBox *resizeCombo = nullptr;
        QLineEdit *customSize = nullptr;
    };

    struct AudioTabControls {
        QComboBox *codecCombo = nullptr;
        QComboBox *bitrateCombo = nullptr;
        QComboBox *trackCombo = nullptr;
        QSlider *sourceVolume = nullptr;
        QSlider *introVolume = nullptr;
        QSlider *outroVolume = nullptr;
    };

    struct LogoTabControls {
        QLineEdit *imagePath = nullptr;
        QComboBox *placementCombo = nullptr;
        QSlider *opacitySlider = nullptr;
        QComboBox *visibilityCombo = nullptr;
        QSpinBox *durationSpin = nullptr;
        QSpinBox *intervalSpin = nullptr;
    };

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
    EncodeJob buildJobFromUi(const QString &videoPath) const;
    QString detectSubtitleFor(const QString &videoPath) const;
    void updateQueueRowDisplay(int row);

    Encoder m_encoder;
    StartButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QComboBox *m_priorityCombo = nullptr;
    QTabWidget *m_tabWidget = nullptr;
    QTableWidget *m_queueTable = nullptr;
    QTextEdit *m_logView = nullptr;
    QLabel *m_statusLabel = nullptr;
    MainTabControls m_mainControls;
    VideoTabControls m_videoControls;
    AudioTabControls m_audioControls;
    LogoTabControls m_logoControls;
    QVector<EncodeJob> m_jobs;
    int m_activeRow = -1;
};
