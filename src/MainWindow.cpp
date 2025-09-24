#include "MainWindow.h"

#include <QAction>
#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QPair>
#include <QPushButton>
#include <QRegularExpression>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextEdit>
#include <QToolBar>
#include <QVBoxLayout>
#include <QVariant>

#include <utility>

namespace {
QString formatTimestampedLine(const QString &line)
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    return QStringLiteral("[%1] %2").arg(timestamp, line);
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("Niseyuki"));
    resize(1280, 800);

    createToolBar();
    setCentralWidget(createCentral());

    statusBar()->showMessage(tr("Ready"));

    connect(&m_encoder, &Encoder::stateChanged, this, &MainWindow::onEncoderStateChanged);
    connect(&m_encoder, &Encoder::progressChanged, this, &MainWindow::onEncoderProgressChanged);
    connect(&m_encoder, &Encoder::statusTextChanged, this, &MainWindow::onEncoderStatusChanged);
    connect(&m_encoder, &Encoder::messageReceived, this, &MainWindow::onEncoderMessageReceived);
    connect(&m_encoder, &Encoder::finished, this, &MainWindow::onEncoderFinished);

    updateStartStopAvailability();
}

void MainWindow::createToolBar()
{
    auto *toolbar = addToolBar(tr("Main"));
    toolbar->setMovable(false);

    auto *addAction = toolbar->addAction(tr("+ Add file"));
    addAction->setShortcut(QKeySequence::Open);
    connect(addAction, &QAction::triggered, this, &MainWindow::onAddFile);

    auto *removeAction = toolbar->addAction(tr("- Remove file"));
    connect(removeAction, &QAction::triggered, this, &MainWindow::onRemoveSelected);

    toolbar->addSeparator();

    m_priorityCombo = new QComboBox(toolbar);
    m_priorityCombo->addItems({tr("Idle"), tr("Below normal"), tr("Normal"), tr("Above"), tr("High"), tr("Real-time")});
    m_priorityCombo->setCurrentIndex(1);
    m_priorityCombo->setItemData(5, QVariant::fromValue(false), Qt::UserRole - 1);
    m_priorityCombo->setItemData(5, tr("Real-time priority is unavailable"), Qt::ToolTipRole);
    toolbar->addWidget(new QLabel(tr("Priority:"), toolbar));
    toolbar->addWidget(m_priorityCombo);

    toolbar->addSeparator();

    auto *settingsAction = toolbar->addAction(tr("⚙️ Settings"));
    settingsAction->setToolTip(tr("Open application settings"));

    auto *aboutAction = toolbar->addAction(tr("ℹ️ About"));
    aboutAction->setToolTip(tr("Show about information"));

    toolbar->addSeparator();

    auto *clusterWidget = new QWidget(toolbar);
    auto *clusterLayout = new QHBoxLayout(clusterWidget);
    clusterLayout->setContentsMargins(0, 0, 0, 0);
    clusterLayout->setSpacing(4);

    m_startButton = new StartButton(clusterWidget);
    connect(m_startButton, &QAbstractButton::clicked, this, &MainWindow::onStartClicked);
    clusterLayout->addWidget(m_startButton);

    m_stopButton = new QPushButton(QStringLiteral("■"), clusterWidget);
    m_stopButton->setFixedSize(48, 48);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    clusterLayout->addWidget(m_stopButton);

    toolbar->addWidget(clusterWidget);
}

QWidget *MainWindow::createCentral()
{
    auto *central = new QWidget(this);
    auto *rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(8);

    auto *splitter = new QSplitter(Qt::Horizontal, central);
    splitter->addWidget(createQueuePanel());

    auto *rightSide = new QWidget(splitter);
    auto *rightLayout = new QVBoxLayout(rightSide);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(8);

    m_tabWidget = new QTabWidget(rightSide);
    m_tabWidget->addTab(createMainTab(), tr("Main"));
    m_tabWidget->addTab(createVideoTab(), tr("Video"));
    m_tabWidget->addTab(createAudioTab(), tr("Audio"));
    m_tabWidget->addTab(createLogoTab(), tr("Logo"));
    m_tabWidget->addTab(createLogTab(), tr("Log"));
    rightLayout->addWidget(m_tabWidget, 1);

    rightLayout->addWidget(createPreviewPanel());

    splitter->addWidget(rightSide);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);

    rootLayout->addWidget(splitter);

    return central;
}

QWidget *MainWindow::createQueuePanel()
{
    auto *panel = new QGroupBox(tr("Queue"), this);
    auto *layout = new QVBoxLayout(panel);

    m_queueTable = new QTableWidget(panel);
    m_queueTable->setColumnCount(3);
    m_queueTable->setHorizontalHeaderLabels({tr("File"), tr("Status"), tr("Output")});
    m_queueTable->horizontalHeader()->setStretchLastSection(true);
    m_queueTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_queueTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_queueTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_queueTable);

    return panel;
}

QWidget *MainWindow::createPreviewPanel()
{
    auto *panel = new QGroupBox(tr("Preview"), this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto *placeholder = new QLabel(tr("Preview player will appear here"), panel);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setMinimumHeight(180);
    placeholder->setStyleSheet(QStringLiteral("QLabel { background-color: palette(base); border: 1px dashed palette(mid); }"));
    layout->addWidget(placeholder, 1);

    auto *controlsRow = new QHBoxLayout;
    controlsRow->setSpacing(4);
    const QStringList controlLabels{tr("Play"), tr("Pause"), tr("Stop"), tr("Speed-"), tr("Speed+"), tr("Prev"), tr("Next"), tr("Go to"), tr("Fullscreen")};
    for (const QString &label : controlLabels) {
        auto *btn = new QPushButton(label, panel);
        btn->setEnabled(false);
        controlsRow->addWidget(btn);
    }
    layout->addLayout(controlsRow);

    return panel;
}

QWidget *MainWindow::createMainTab()
{
    auto *widget = new QWidget(this);
    auto *layout = new QVBoxLayout(widget);
    layout->setSpacing(12);

    auto *subtitleGroup = new QGroupBox(tr("Subtitles"), widget);
    auto *subtitleLayout = new QGridLayout(subtitleGroup);
    subtitleLayout->addWidget(new QLabel(tr("Auto-detected subtitle:"), subtitleGroup), 0, 0);
    m_mainControls.autoSubtitlePath = new QLineEdit(subtitleGroup);
    m_mainControls.autoSubtitlePath->setPlaceholderText(tr("No subtitle detected"));
    m_mainControls.autoSubtitlePath->setReadOnly(true);
    subtitleLayout->addWidget(m_mainControls.autoSubtitlePath, 0, 1, 1, 2);

    subtitleLayout->addWidget(new QLabel(tr("Additional tracks:"), subtitleGroup), 1, 0);
    m_mainControls.additionalSubtitleList = new QTextEdit(subtitleGroup);
    m_mainControls.additionalSubtitleList->setPlaceholderText(tr("Add ASS/SSA files as needed"));
    m_mainControls.additionalSubtitleList->setFixedHeight(80);
    subtitleLayout->addWidget(m_mainControls.additionalSubtitleList, 1, 1, 1, 2);

    auto *fontFinderButton = new QPushButton(tr("Font Finder"), subtitleGroup);
    subtitleLayout->addWidget(fontFinderButton, 2, 0);
    connect(fontFinderButton, &QPushButton::clicked, this, [this]() {
        QMessageBox::information(this,
                                 tr("Font Finder"),
                                 tr("Font gathering is not implemented yet."));
    });

    subtitleLayout->addWidget(new QLabel(tr("Renderer:"), subtitleGroup), 2, 1);
    m_mainControls.rendererCombo = new QComboBox(subtitleGroup);
    m_mainControls.rendererCombo->addItem(tr("Auto"), QStringLiteral("Auto"));
    m_mainControls.rendererCombo->addItem(tr("VSFilter"), QStringLiteral("VSFilter"));
    m_mainControls.rendererCombo->addItem(tr("VSFilterMod"), QStringLiteral("VSFilterMod"));
    m_mainControls.rendererCombo->addItem(tr("libass"), QStringLiteral("libass"));
    subtitleLayout->addWidget(m_mainControls.rendererCombo, 2, 2);
    layout->addWidget(subtitleGroup);

    auto *introGroup = new QGroupBox(tr("Intro / Outro"), widget);
    auto *introLayout = new QFormLayout(introGroup);
    m_mainControls.introPath = new QLineEdit(introGroup);
    introLayout->addRow(tr("Intro video:"), m_mainControls.introPath);
    m_mainControls.outroPath = new QLineEdit(introGroup);
    introLayout->addRow(tr("Outro video:"), m_mainControls.outroPath);
    m_mainControls.thumbnailPath = new QLineEdit(introGroup);
    introLayout->addRow(tr("Thumbnail (2 frames):"), m_mainControls.thumbnailPath);
    layout->addWidget(introGroup);

    auto *cutGroup = new QGroupBox(tr("Cut"), widget);
    auto *cutLayout = new QGridLayout(cutGroup);
    m_mainControls.cutEnable = new QCheckBox(tr("Enable cut"), cutGroup);
    cutLayout->addWidget(m_mainControls.cutEnable, 0, 0, 1, 3);
    cutLayout->addWidget(new QLabel(tr("Start time:"), cutGroup), 1, 0);
    m_mainControls.cutStart = new QLineEdit(cutGroup);
    cutLayout->addWidget(m_mainControls.cutStart, 1, 1);
    cutLayout->addWidget(new QLabel(tr("End time:"), cutGroup), 1, 2);
    m_mainControls.cutEnd = new QLineEdit(cutGroup);
    cutLayout->addWidget(m_mainControls.cutEnd, 1, 3);
    layout->addWidget(cutGroup);

    m_mainControls.telegramToggle = new QCheckBox(tr("Telegram Mode (MP4 + AAC)"), widget);
    layout->addWidget(m_mainControls.telegramToggle);

    auto *outputLayout = new QHBoxLayout;
    outputLayout->addWidget(new QLabel(tr("Output file:"), widget));
    m_mainControls.outputFile = new QLineEdit(widget);
    outputLayout->addWidget(m_mainControls.outputFile, 1);
    auto *outputBrowse = new QPushButton(tr("Browse"), widget);
    outputLayout->addWidget(outputBrowse);
    layout->addLayout(outputLayout);

    connect(outputBrowse, &QPushButton::clicked, this, [this]() {
        if (!m_mainControls.outputFile) {
            return;
        }
        const QString filter = (m_mainControls.telegramToggle && m_mainControls.telegramToggle->isChecked())
                                   ? tr("MP4 files (*.mp4);;All files (*.*)")
                                   : tr("Matroska files (*.mkv);;MP4 files (*.mp4);;All files (*.*)");
        const QString selected = QFileDialog::getSaveFileName(this,
                                                              tr("Select output file"),
                                                              m_mainControls.outputFile->text(),
                                                              filter);
        if (!selected.isEmpty()) {
            m_mainControls.outputFile->setText(selected);
        }
    });

    layout->addStretch(1);
    return widget;
}

QWidget *MainWindow::createVideoTab()
{
    auto *widget = new QWidget(this);
    auto *layout = new QFormLayout(widget);

    m_videoControls.encoderCombo = new QComboBox(widget);
    m_videoControls.encoderCombo->addItem(tr("x264"), QStringLiteral("x264"));
    m_videoControls.encoderCombo->addItem(tr("x265"), QStringLiteral("x265"));
    m_videoControls.encoderCombo->addItem(tr("Intel QSV"), QStringLiteral("qsv"));
    m_videoControls.encoderCombo->addItem(tr("NVENC"), QStringLiteral("nvenc"));
    m_videoControls.encoderCombo->addItem(tr("AMD"), QStringLiteral("amd"));
    layout->addRow(tr("Encoder:"), m_videoControls.encoderCombo);

    m_videoControls.presetCombo = new QComboBox(widget);
    const QVector<QPair<QString, QString>> presets = {
        {tr("Very Slow"), QStringLiteral("veryslow")},
        {tr("Slower"), QStringLiteral("slower")},
        {tr("Slow"), QStringLiteral("slow")},
        {tr("Medium"), QStringLiteral("medium")},
        {tr("Fast"), QStringLiteral("fast")},
        {tr("Faster"), QStringLiteral("faster")},
        {tr("Very Fast"), QStringLiteral("veryfast")}
    };
    for (const auto &pair : presets) {
        m_videoControls.presetCombo->addItem(pair.first, pair.second);
    }
    layout->addRow(tr("Preset:"), m_videoControls.presetCombo);

    m_videoControls.qualitySlider = new QSlider(Qt::Horizontal, widget);
    m_videoControls.qualitySlider->setRange(0, 510);
    m_videoControls.qualitySlider->setValue(230);
    m_videoControls.qualitySlider->setToolTip(tr("Drag for CRF/CQ (0.0 – 51.0)"));
    layout->addRow(tr("Quality (CRF/CQ):"), m_videoControls.qualitySlider);

    m_videoControls.resizeCombo = new QComboBox(widget);
    m_videoControls.resizeCombo->addItem(tr("None"), QStringLiteral("none"));
    m_videoControls.resizeCombo->addItem(tr("1080p"), QStringLiteral("1080p"));
    m_videoControls.resizeCombo->addItem(tr("720p"), QStringLiteral("720p"));
    m_videoControls.resizeCombo->addItem(tr("480p"), QStringLiteral("480p"));
    m_videoControls.resizeCombo->addItem(tr("Custom"), QStringLiteral("custom"));
    layout->addRow(tr("Resize:"), m_videoControls.resizeCombo);

    m_videoControls.customSize = new QLineEdit(widget);
    m_videoControls.customSize->setPlaceholderText(tr("Width x Height"));
    m_videoControls.customSize->setEnabled(false);
    layout->addRow(tr("Custom size:"), m_videoControls.customSize);

    connect(m_videoControls.resizeCombo,
            &QComboBox::currentIndexChanged,
            this,
            [this](int index) {
                if (!m_videoControls.customSize || !m_videoControls.resizeCombo) {
                    return;
                }
                const QVariant value = m_videoControls.resizeCombo->itemData(index);
                const bool enable = value.toString() == QLatin1String("custom");
                m_videoControls.customSize->setEnabled(enable);
            });
    if (m_videoControls.resizeCombo && m_videoControls.customSize) {
        const bool enable = m_videoControls.resizeCombo->currentData().toString() == QLatin1String("custom");
        m_videoControls.customSize->setEnabled(enable);
    }

    auto *cutInfo = new QLabel(tr("Cut settings mirror the Main tab."), widget);
    cutInfo->setWordWrap(true);
    layout->addRow(QString(), cutInfo);

    return widget;
}

QWidget *MainWindow::createAudioTab()
{
    auto *widget = new QWidget(this);
    auto *layout = new QFormLayout(widget);

    m_audioControls.codecCombo = new QComboBox(widget);
    m_audioControls.codecCombo->addItem(tr("AAC (Native)"), QStringLiteral("aac"));
    m_audioControls.codecCombo->addItem(tr("FLAC"), QStringLiteral("flac"));
    layout->addRow(tr("Codec:"), m_audioControls.codecCombo);

    m_audioControls.bitrateCombo = new QComboBox(widget);
    m_audioControls.bitrateCombo->addItem(QStringLiteral("128 kbps"), 128);
    m_audioControls.bitrateCombo->addItem(QStringLiteral("192 kbps"), 192);
    m_audioControls.bitrateCombo->addItem(QStringLiteral("256 kbps"), 256);
    m_audioControls.bitrateCombo->addItem(QStringLiteral("320 kbps"), 320);
    layout->addRow(tr("AAC preset:"), m_audioControls.bitrateCombo);

    m_audioControls.trackCombo = new QComboBox(widget);
    m_audioControls.trackCombo->setEditable(true);
    m_audioControls.trackCombo->setPlaceholderText(tr("Auto-detect Japanese audio"));
    layout->addRow(tr("Track:"), m_audioControls.trackCombo);

    m_audioControls.sourceVolume = new QSlider(Qt::Horizontal, widget);
    m_audioControls.sourceVolume->setRange(0, 200);
    m_audioControls.sourceVolume->setValue(100);
    layout->addRow(tr("Source volume:"), m_audioControls.sourceVolume);

    m_audioControls.introVolume = new QSlider(Qt::Horizontal, widget);
    m_audioControls.introVolume->setRange(0, 200);
    m_audioControls.introVolume->setValue(100);
    layout->addRow(tr("Intro volume:"), m_audioControls.introVolume);

    m_audioControls.outroVolume = new QSlider(Qt::Horizontal, widget);
    m_audioControls.outroVolume->setRange(0, 200);
    m_audioControls.outroVolume->setValue(100);
    layout->addRow(tr("Outro volume:"), m_audioControls.outroVolume);

    return widget;
}

QWidget *MainWindow::createLogoTab()
{
    auto *widget = new QWidget(this);
    auto *layout = new QFormLayout(widget);

    m_logoControls.imagePath = new QLineEdit(widget);
    m_logoControls.imagePath->setPlaceholderText(tr("PNG path"));
    layout->addRow(tr("Logo image:"), m_logoControls.imagePath);

    m_logoControls.placementCombo = new QComboBox(widget);
    m_logoControls.placementCombo->addItem(tr("Top-left"), QStringLiteral("top-left"));
    m_logoControls.placementCombo->addItem(tr("Top-right"), QStringLiteral("top-right"));
    m_logoControls.placementCombo->addItem(tr("Bottom-left"), QStringLiteral("bottom-left"));
    m_logoControls.placementCombo->addItem(tr("Bottom-right"), QStringLiteral("bottom-right"));
    m_logoControls.placementCombo->addItem(tr("Custom"), QStringLiteral("custom"));
    layout->addRow(tr("Placement:"), m_logoControls.placementCombo);

    m_logoControls.opacitySlider = new QSlider(Qt::Horizontal, widget);
    m_logoControls.opacitySlider->setRange(0, 100);
    m_logoControls.opacitySlider->setValue(80);
    layout->addRow(tr("Opacity:"), m_logoControls.opacitySlider);

    m_logoControls.visibilityCombo = new QComboBox(widget);
    m_logoControls.visibilityCombo->addItem(tr("Always"), QStringLiteral("always"));
    m_logoControls.visibilityCombo->addItem(tr("Intro only"), QStringLiteral("intro"));
    m_logoControls.visibilityCombo->addItem(tr("Outro only"), QStringLiteral("outro"));
    m_logoControls.visibilityCombo->addItem(tr("Timed"), QStringLiteral("timed"));
    layout->addRow(tr("Visibility:"), m_logoControls.visibilityCombo);

    auto *timingRow = new QHBoxLayout;
    timingRow->setContentsMargins(0, 0, 0, 0);
    timingRow->setSpacing(6);
    auto *durationLabel = new QLabel(tr("Duration (s):"), widget);
    timingRow->addWidget(durationLabel);
    m_logoControls.durationSpin = new QSpinBox(widget);
    m_logoControls.durationSpin->setRange(1, 30);
    timingRow->addWidget(m_logoControls.durationSpin);
    auto *intervalLabel = new QLabel(tr("Every (min):"), widget);
    timingRow->addWidget(intervalLabel);
    m_logoControls.intervalSpin = new QSpinBox(widget);
    m_logoControls.intervalSpin->setRange(1, 60);
    timingRow->addWidget(m_logoControls.intervalSpin);

    auto *timingContainer = new QWidget(widget);
    timingContainer->setLayout(timingRow);
    layout->addRow(tr("Timed display:"), timingContainer);

    connect(m_logoControls.visibilityCombo,
            &QComboBox::currentIndexChanged,
            this,
            [this](int index) {
                if (!m_logoControls.visibilityCombo || !m_logoControls.durationSpin || !m_logoControls.intervalSpin) {
                    return;
                }
                const QVariant mode = m_logoControls.visibilityCombo->itemData(index);
                const bool timed = mode.toString() == QLatin1String("timed");
                m_logoControls.durationSpin->setEnabled(timed);
                m_logoControls.intervalSpin->setEnabled(timed);
            });
    if (m_logoControls.visibilityCombo && m_logoControls.durationSpin && m_logoControls.intervalSpin) {
        const bool timed = m_logoControls.visibilityCombo->currentData().toString() == QLatin1String("timed");
        m_logoControls.durationSpin->setEnabled(timed);
        m_logoControls.intervalSpin->setEnabled(timed);
    }

    return widget;
}

QWidget *MainWindow::createLogTab()
{
    auto *widget = new QWidget(this);
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);

    m_logView = new QTextEdit(widget);
    m_logView->setReadOnly(true);
    layout->addWidget(m_logView);

    return widget;
}

EncodeJob MainWindow::buildJobFromUi(const QString &videoPath) const
{
    EncodeJob job;
    job.videoPath = videoPath;
    job.subtitlePath = detectSubtitleFor(videoPath);
    job.subtitleInfo.path = job.subtitlePath;
    if (m_mainControls.additionalSubtitleList) {
        const QStringList entries = m_mainControls.additionalSubtitleList->toPlainText()
                                         .split(QRegularExpression(QStringLiteral("[\\r\\n]+")),
                                                Qt::SkipEmptyParts);
        for (const QString &entry : entries) {
            const QString trimmed = entry.trimmed();
            if (!trimmed.isEmpty()) {
                job.additionalSubtitles.append(trimmed);
            }
        }
    }

    if (m_mainControls.rendererCombo) {
        const QString renderer = m_mainControls.rendererCombo->currentData().toString();
        job.rendererMode = renderer.isEmpty() ? QStringLiteral("Auto") : renderer;
        if (!renderer.isEmpty() && renderer != QStringLiteral("Auto")) {
            job.subtitleInfo.rendererOverride = renderer;
        }
    } else {
        job.rendererMode = QStringLiteral("Auto");
    }

    if (m_mainControls.introPath) {
        job.introOutroInfo.introPath = m_mainControls.introPath->text().trimmed();
    }
    if (m_mainControls.outroPath) {
        job.introOutroInfo.outroPath = m_mainControls.outroPath->text().trimmed();
    }
    if (m_mainControls.thumbnailPath) {
        job.introOutroInfo.thumbnailPath = m_mainControls.thumbnailPath->text().trimmed();
    }

    if (m_mainControls.cutEnable && m_mainControls.cutEnable->isChecked()) {
        job.cutSettings.enabled = true;
        if (m_mainControls.cutStart) {
            job.cutSettings.startTime = m_mainControls.cutStart->text().trimmed();
        }
        if (m_mainControls.cutEnd) {
            job.cutSettings.endTime = m_mainControls.cutEnd->text().trimmed();
        }
    }

    job.telegramMode = m_mainControls.telegramToggle && m_mainControls.telegramToggle->isChecked();
    if (m_mainControls.outputFile) {
        job.outputFile = m_mainControls.outputFile->text().trimmed();
    }

    if (m_videoControls.encoderCombo) {
        job.videoSettings.encoder = m_videoControls.encoderCombo->currentData().toString();
    }
    if (m_videoControls.presetCombo) {
        job.videoSettings.preset = m_videoControls.presetCombo->currentData().toString();
    }
    if (m_videoControls.qualitySlider) {
        job.videoSettings.qualityValue = m_videoControls.qualitySlider->value() / 10.0;
    }
    if (m_videoControls.resizeCombo) {
        job.videoSettings.resizeMode = m_videoControls.resizeCombo->currentData().toString();
        if (job.videoSettings.resizeMode == QStringLiteral("custom") && m_videoControls.customSize) {
            const QString text = m_videoControls.customSize->text().trimmed();
            QRegularExpression re(QStringLiteral("^(\\d+)\\s*[xX]\\s*(\\d+)$"));
            const auto match = re.match(text);
            if (match.hasMatch()) {
                bool okW = false;
                bool okH = false;
                const int width = match.captured(1).toInt(&okW);
                const int height = match.captured(2).toInt(&okH);
                if (okW && okH && width > 0 && height > 0) {
                    job.videoSettings.customSize = QSize(width, height);
                } else {
                    job.videoSettings.resizeMode = QStringLiteral("none");
                }
            } else {
                job.videoSettings.resizeMode = QStringLiteral("none");
            }
        }
    }

    if (m_audioControls.codecCombo) {
        job.audioSettings.codec = m_audioControls.codecCombo->currentData().toString();
    }
    if (m_audioControls.bitrateCombo) {
        job.audioSettings.bitrateKbps = m_audioControls.bitrateCombo->currentData().toInt();
    }
    if (m_audioControls.trackCombo) {
        job.audioSettings.preferredTrackId = m_audioControls.trackCombo->currentText().trimmed();
    }
    job.audioSettings.volumeSource = m_audioControls.sourceVolume ? static_cast<float>(m_audioControls.sourceVolume->value()) / 100.0f : 1.0f;
    job.audioSettings.volumeIntro = m_audioControls.introVolume ? static_cast<float>(m_audioControls.introVolume->value()) / 100.0f : 1.0f;
    job.audioSettings.volumeOutro = m_audioControls.outroVolume ? static_cast<float>(m_audioControls.outroVolume->value()) / 100.0f : 1.0f;

    if (m_logoControls.imagePath) {
        job.logoSettings.imagePath = m_logoControls.imagePath->text().trimmed();
    }
    job.logoSettings.placement = m_logoControls.placementCombo ? m_logoControls.placementCombo->currentData().toString() : QStringLiteral("top-left");
    job.logoSettings.opacity = m_logoControls.opacitySlider ? static_cast<float>(m_logoControls.opacitySlider->value()) / 100.0f : 0.8f;
    job.logoSettings.visibility = m_logoControls.visibilityCombo ? m_logoControls.visibilityCombo->currentData().toString() : QStringLiteral("always");
    job.logoSettings.visibleDuration = m_logoControls.durationSpin ? m_logoControls.durationSpin->value() : 0;
    job.logoSettings.visibleInterval = m_logoControls.intervalSpin ? m_logoControls.intervalSpin->value() : 0;

    return job;
}

QString MainWindow::detectSubtitleFor(const QString &videoPath) const
{
    const QFileInfo videoInfo(videoPath);
    if (!videoInfo.exists()) {
        return QString();
    }

    const QDir dir = videoInfo.dir();
    const QString baseName = videoInfo.completeBaseName();
    const QStringList extensions{QStringLiteral("ass"), QStringLiteral("ssa"), QStringLiteral("srt")};
    for (const QString &ext : extensions) {
        const QString candidate = dir.filePath(baseName + QLatin1Char('.') + ext);
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
        const QString upperCandidate = dir.filePath(baseName + QLatin1Char('.') + ext.toUpper());
        if (QFileInfo::exists(upperCandidate)) {
            return upperCandidate;
        }
    }
    return QString();
}

void MainWindow::updateQueueRowDisplay(int row)
{
    if (!m_queueTable || row < 0 || row >= m_queueTable->rowCount() || row >= m_jobs.size()) {
        return;
    }

    auto *outputItem = m_queueTable->item(row, 2);
    if (!outputItem) {
        outputItem = new QTableWidgetItem;
        m_queueTable->setItem(row, 2, outputItem);
    }
    const QString outputPath = m_jobs.at(row).resolvedOutputPath();
    outputItem->setText(outputPath);
    outputItem->setToolTip(outputPath);
}

void MainWindow::appendLog(const QString &line)
{
    if (!m_logView) {
        return;
    }
    m_logView->append(formatTimestampedLine(line));
}

void MainWindow::updateStartStopAvailability()
{
    const bool hasJobs = m_queueTable && m_queueTable->rowCount() > 0 && !m_jobs.isEmpty();
    const bool isIdle = m_encoder.state() == Encoder::State::Idle;
    if (m_startButton) {
        m_startButton->setEnabled(hasJobs && isIdle);
    }
    if (m_stopButton) {
        m_stopButton->setEnabled(!isIdle);
    }
}

void MainWindow::onAddFile()
{
    const QString file = QFileDialog::getOpenFileName(this, tr("Select video"));
    if (file.isEmpty()) {
        return;
    }

    EncodeJob job = buildJobFromUi(file);
    if (m_mainControls.autoSubtitlePath) {
        m_mainControls.autoSubtitlePath->setText(job.subtitlePath);
    }

    const int row = m_queueTable->rowCount();
    m_queueTable->insertRow(row);

    auto *fileItem = new QTableWidgetItem(QFileInfo(file).fileName());
    fileItem->setData(Qt::UserRole, file);
    fileItem->setToolTip(file);
    m_queueTable->setItem(row, 0, fileItem);

    m_queueTable->setItem(row, 1, new QTableWidgetItem(tr("Pending")));

    m_jobs.append(std::move(job));
    updateQueueRowDisplay(row);

    appendLog(tr("Added job: %1").arg(file));
    updateStartStopAvailability();
}

void MainWindow::onRemoveSelected()
{
    const auto selected = m_queueTable->selectionModel()->selectedRows();
    QList<int> rows;
    rows.reserve(selected.size());
    for (const QModelIndex &idx : selected) {
        rows.prepend(idx.row());
    }
    for (int row : rows) {
        appendLog(tr("Removed job: %1").arg(m_queueTable->item(row, 0)->text()));
        m_queueTable->removeRow(row);
        if (row >= 0 && row < m_jobs.size()) {
            m_jobs.removeAt(row);
        }
        if (m_activeRow == row) {
            m_activeRow = -1;
        } else if (m_activeRow > row) {
            --m_activeRow;
        }
    }
    updateStartStopAvailability();
}

void MainWindow::onStartClicked()
{
    if (m_encoder.state() != Encoder::State::Idle) {
        return;
    }
    if (m_queueTable->rowCount() == 0 || m_jobs.isEmpty()) {
        QMessageBox::information(this, tr("No jobs"), tr("Add a file before starting."));
        return;
    }

    const int row = 0;
    if (!m_queueTable->item(row, 0)) {
        QMessageBox::warning(this, tr("Missing file"), tr("The selected job has no source path."));
        return;
    }

    const QString sourcePath = m_queueTable->item(row, 0)->data(Qt::UserRole).toString();
    if (sourcePath.isEmpty() || row >= m_jobs.size()) {
        QMessageBox::warning(this, tr("Missing file"), tr("The selected job has no source path."));
        return;
    }

    m_jobs[row] = buildJobFromUi(sourcePath);
    if (m_mainControls.autoSubtitlePath) {
        m_mainControls.autoSubtitlePath->setText(m_jobs[row].subtitlePath);
    }
    updateQueueRowDisplay(row);

    appendLog(tr("Starting encode: %1").arg(sourcePath));
    if (auto *statusItem = m_queueTable->item(row, 1)) {
        statusItem->setText(tr("Indexing"));
    }
    m_activeRow = row;
    m_encoder.startEncoding(m_jobs.at(row));
    updateStartStopAvailability();
}

void MainWindow::onStopClicked()
{
    if (m_encoder.state() == Encoder::State::Idle) {
        return;
    }
    appendLog(tr("Stopping encode"));
    m_encoder.stopEncoding();
}

void MainWindow::onEncoderStateChanged(Encoder::State state)
{
    switch (state) {
    case Encoder::State::Idle:
        m_startButton->setState(StartButton::State::Idle);
        m_startButton->setProgress(0.0);
        m_startButton->setToolTip(tr("Start encoding"));
        statusBar()->showMessage(tr("Idle"));
        if (m_activeRow >= 0 && m_activeRow < m_queueTable->rowCount()) {
            if (auto *item = m_queueTable->item(m_activeRow, 1)) {
                item->setText(tr("Pending"));
            }
        }
        break;
    case Encoder::State::Indexing:
        m_startButton->setState(StartButton::State::Indexing);
        m_startButton->setToolTip(tr("Indexing"));
        statusBar()->showMessage(tr("Indexing"));
        if (m_activeRow >= 0 && m_activeRow < m_queueTable->rowCount()) {
            if (auto *item = m_queueTable->item(m_activeRow, 1)) {
                item->setText(tr("Indexing"));
            }
        }
        break;
    case Encoder::State::Encoding:
        m_startButton->setState(StartButton::State::Encoding);
        m_startButton->setToolTip(tr("Encoding"));
        statusBar()->showMessage(tr("Encoding"));
        if (m_activeRow >= 0 && m_activeRow < m_queueTable->rowCount()) {
            if (auto *item = m_queueTable->item(m_activeRow, 1)) {
                item->setText(tr("Encoding"));
            }
        }
        break;
    case Encoder::State::Stopping:
        statusBar()->showMessage(tr("Stopping"));
        if (m_activeRow >= 0 && m_activeRow < m_queueTable->rowCount()) {
            if (auto *item = m_queueTable->item(m_activeRow, 1)) {
                item->setText(tr("Stopping"));
            }
        }
        break;
    }
    updateStartStopAvailability();
}

void MainWindow::onEncoderProgressChanged(double progress)
{
    if (m_startButton) {
        m_startButton->setProgress(progress);
        m_startButton->setToolTip(tr("Encoding %1%").arg(QString::number(progress * 100.0, 'f', 1)));
    }
}

void MainWindow::onEncoderStatusChanged(const QString &text)
{
    statusBar()->showMessage(text);
}

void MainWindow::onEncoderMessageReceived(const QString &message)
{
    appendLog(message);
}

void MainWindow::onEncoderFinished(bool success)
{
    appendLog(success ? tr("Encode complete") : tr("Encode failed"));
    if (m_activeRow >= 0 && m_activeRow < m_queueTable->rowCount()) {
        if (auto *item = m_queueTable->item(m_activeRow, 1)) {
            item->setText(success ? tr("Done") : tr("Failed"));
        }
    }
    m_activeRow = -1;
    updateStartStopAvailability();
}
