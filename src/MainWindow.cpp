#include "MainWindow.h"

#include <QAction>
#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
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
#include <QPushButton>
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
    auto *subtitlePath = new QLineEdit(subtitleGroup);
    subtitlePath->setPlaceholderText(tr("No subtitle detected"));
    subtitlePath->setReadOnly(true);
    subtitleLayout->addWidget(subtitlePath, 0, 1, 1, 2);

    subtitleLayout->addWidget(new QLabel(tr("Additional tracks:"), subtitleGroup), 1, 0);
    auto *subtitleList = new QTextEdit(subtitleGroup);
    subtitleList->setPlaceholderText(tr("Add ASS/SSA files as needed"));
    subtitleList->setFixedHeight(80);
    subtitleLayout->addWidget(subtitleList, 1, 1, 1, 2);

    auto *fontFinderButton = new QPushButton(tr("Font Finder"), subtitleGroup);
    subtitleLayout->addWidget(fontFinderButton, 2, 0);
    subtitleLayout->addWidget(new QLabel(tr("Renderer:"), subtitleGroup), 2, 1);
    auto *rendererCombo = new QComboBox(subtitleGroup);
    rendererCombo->addItems({tr("Auto"), tr("VSFilter"), tr("VSFilterMod"), tr("libass")});
    subtitleLayout->addWidget(rendererCombo, 2, 2);
    layout->addWidget(subtitleGroup);

    auto *introGroup = new QGroupBox(tr("Intro / Outro"), widget);
    auto *introLayout = new QFormLayout(introGroup);
    introLayout->addRow(tr("Intro video:"), new QLineEdit(introGroup));
    introLayout->addRow(tr("Outro video:"), new QLineEdit(introGroup));
    introLayout->addRow(tr("Thumbnail (2 frames):"), new QLineEdit(introGroup));
    layout->addWidget(introGroup);

    auto *cutGroup = new QGroupBox(tr("Cut"), widget);
    auto *cutLayout = new QGridLayout(cutGroup);
    auto *cutEnable = new QCheckBox(tr("Enable cut"), cutGroup);
    cutLayout->addWidget(cutEnable, 0, 0, 1, 3);
    cutLayout->addWidget(new QLabel(tr("Start time:"), cutGroup), 1, 0);
    cutLayout->addWidget(new QLineEdit(cutGroup), 1, 1);
    cutLayout->addWidget(new QLabel(tr("End time:"), cutGroup), 1, 2);
    cutLayout->addWidget(new QLineEdit(cutGroup), 1, 3);
    layout->addWidget(cutGroup);

    auto *telegramToggle = new QCheckBox(tr("Telegram Mode (MP4 + AAC)"), widget);
    layout->addWidget(telegramToggle);

    auto *outputLayout = new QHBoxLayout;
    outputLayout->addWidget(new QLabel(tr("Output file:"), widget));
    auto *outputPath = new QLineEdit(widget);
    outputLayout->addWidget(outputPath, 1);
    auto *outputBrowse = new QPushButton(tr("Browse"), widget);
    outputLayout->addWidget(outputBrowse);
    layout->addLayout(outputLayout);

    layout->addStretch(1);
    return widget;
}

QWidget *MainWindow::createVideoTab()
{
    auto *widget = new QWidget(this);
    auto *layout = new QFormLayout(widget);

    auto *encoderCombo = new QComboBox(widget);
    encoderCombo->addItems({tr("x264"), tr("x265"), tr("Intel QSV"), tr("NVENC"), tr("AMD")});
    layout->addRow(tr("Encoder:"), encoderCombo);

    auto *presetCombo = new QComboBox(widget);
    presetCombo->addItems({tr("Very Slow"), tr("Slower"), tr("Slow"), tr("Medium"), tr("Fast"), tr("Faster"), tr("Very Fast")});
    layout->addRow(tr("Preset:"), presetCombo);

    auto *qualitySlider = new QSlider(Qt::Horizontal, widget);
    qualitySlider->setRange(0, 510);
    qualitySlider->setValue(230);
    layout->addRow(tr("Quality (CRF/CQ):"), qualitySlider);

    auto *resizeCombo = new QComboBox(widget);
    resizeCombo->addItems({tr("None"), tr("1080p"), tr("720p"), tr("480p"), tr("Custom")});
    layout->addRow(tr("Resize:"), resizeCombo);

    auto *customSize = new QLineEdit(widget);
    customSize->setPlaceholderText(tr("Width x Height"));
    layout->addRow(tr("Custom size:"), customSize);

    auto *cutInfo = new QLabel(tr("Cut settings mirror the Main tab."), widget);
    cutInfo->setWordWrap(true);
    layout->addRow(QString(), cutInfo);

    return widget;
}

QWidget *MainWindow::createAudioTab()
{
    auto *widget = new QWidget(this);
    auto *layout = new QFormLayout(widget);

    auto *codecCombo = new QComboBox(widget);
    codecCombo->addItems({tr("AAC (Native)"), tr("FLAC")});
    layout->addRow(tr("Codec:"), codecCombo);

    auto *bitrateCombo = new QComboBox(widget);
    bitrateCombo->addItems({QStringLiteral("128 kbps"), QStringLiteral("192 kbps"), QStringLiteral("256 kbps"), QStringLiteral("320 kbps")});
    layout->addRow(tr("AAC preset:"), bitrateCombo);

    auto *trackCombo = new QComboBox(widget);
    trackCombo->setEditable(true);
    trackCombo->setPlaceholderText(tr("Auto-detect Japanese audio"));
    layout->addRow(tr("Track:"), trackCombo);

    auto *sourceVolume = new QSlider(Qt::Horizontal, widget);
    sourceVolume->setRange(0, 200);
    sourceVolume->setValue(100);
    layout->addRow(tr("Source volume:"), sourceVolume);

    auto *introVolume = new QSlider(Qt::Horizontal, widget);
    introVolume->setRange(0, 200);
    introVolume->setValue(100);
    layout->addRow(tr("Intro volume:"), introVolume);

    auto *outroVolume = new QSlider(Qt::Horizontal, widget);
    outroVolume->setRange(0, 200);
    outroVolume->setValue(100);
    layout->addRow(tr("Outro volume:"), outroVolume);

    return widget;
}

QWidget *MainWindow::createLogoTab()
{
    auto *widget = new QWidget(this);
    auto *layout = new QFormLayout(widget);

    auto *logoPicker = new QLineEdit(widget);
    logoPicker->setPlaceholderText(tr("PNG path"));
    layout->addRow(tr("Logo image:"), logoPicker);

    auto *placementCombo = new QComboBox(widget);
    placementCombo->addItems({tr("Top-left"), tr("Top-right"), tr("Bottom-left"), tr("Bottom-right"), tr("Custom")});
    layout->addRow(tr("Placement:"), placementCombo);

    auto *opacitySlider = new QSlider(Qt::Horizontal, widget);
    opacitySlider->setRange(0, 100);
    opacitySlider->setValue(80);
    layout->addRow(tr("Opacity:"), opacitySlider);

    auto *visibilityCombo = new QComboBox(widget);
    visibilityCombo->addItems({tr("Always"), tr("Intro only"), tr("Outro only"), tr("Timed")});
    layout->addRow(tr("Visibility:"), visibilityCombo);

    auto *timingRow = new QHBoxLayout;
    auto *durationSpin = new QSpinBox(widget);
    durationSpin->setRange(1, 30);
    timingRow->addWidget(new QLabel(tr("Duration (s):"), widget));
    timingRow->addWidget(durationSpin);
    auto *intervalSpin = new QSpinBox(widget);
    intervalSpin->setRange(1, 30);
    timingRow->addWidget(new QLabel(tr("Every (min):"), widget));
    timingRow->addWidget(intervalSpin);

    auto *timingContainer = new QWidget(widget);
    timingContainer->setLayout(timingRow);
    layout->addRow(tr("Timed display:"), timingContainer);

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

void MainWindow::appendLog(const QString &line)
{
    if (!m_logView) {
        return;
    }
    m_logView->append(formatTimestampedLine(line));
}

void MainWindow::updateStartStopAvailability()
{
    const bool hasJobs = m_queueTable && m_queueTable->rowCount() > 0;
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

    const int row = m_queueTable->rowCount();
    m_queueTable->insertRow(row);

    auto *fileItem = new QTableWidgetItem(QFileInfo(file).fileName());
    fileItem->setData(Qt::UserRole, file);
    m_queueTable->setItem(row, 0, fileItem);

    m_queueTable->setItem(row, 1, new QTableWidgetItem(tr("Pending")));
    m_queueTable->setItem(row, 2, new QTableWidgetItem(QString()));

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
    }
    updateStartStopAvailability();
}

void MainWindow::onStartClicked()
{
    if (m_encoder.state() != Encoder::State::Idle) {
        return;
    }
    if (m_queueTable->rowCount() == 0) {
        QMessageBox::information(this, tr("No jobs"), tr("Add a file before starting."));
        return;
    }

    const QString sourcePath = m_queueTable->item(0, 0)->data(Qt::UserRole).toString();
    if (sourcePath.isEmpty()) {
        QMessageBox::warning(this, tr("Missing file"), tr("The selected job has no source path."));
        return;
    }

    EncodeJob job;
    job.videoPath = sourcePath;
    job.telegramMode = false;
    job.outputFile.clear();

    appendLog(tr("Starting encode: %1").arg(sourcePath));
    m_encoder.startEncoding(job);
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
        break;
    case Encoder::State::Indexing:
        m_startButton->setState(StartButton::State::Indexing);
        m_startButton->setToolTip(tr("Indexing"));
        statusBar()->showMessage(tr("Indexing"));
        break;
    case Encoder::State::Encoding:
        m_startButton->setState(StartButton::State::Encoding);
        m_startButton->setToolTip(tr("Encoding"));
        statusBar()->showMessage(tr("Encoding"));
        break;
    case Encoder::State::Stopping:
        statusBar()->showMessage(tr("Stopping"));
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
    if (m_queueTable->rowCount() > 0) {
        m_queueTable->item(0, 1)->setText(success ? tr("Done") : tr("Failed"));
    }
    updateStartStopAvailability();
}
