#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QGroupBox>
#include <QFrame>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), isPaused(false) {
    setWindowTitle("Téléchargeur Multi-Thread Rapide");
    setMinimumSize(900, 700);

    setupUI();
    setupStyles();

    downloadManager = new DownloadManager(this);

    // Connections
    connect(downloadButton, &QPushButton::clicked, this, &MainWindow::startDownload);
    connect(pauseButton, &QPushButton::clicked, this, &MainWindow::pauseDownload);
    connect(cancelButton, &QPushButton::clicked, this, &MainWindow::cancelDownload);

    connect(browseButton, &QPushButton::clicked, this, [this]() {
        QString filePath = QFileDialog::getSaveFileName(this, "Enregistrer le fichier", "", "Tous les fichiers (*)");
        if (!filePath.isEmpty()) {
            savePathInput->setText(filePath);
        }
    });

    connect(downloadManager, &DownloadManager::progressUpdated, this, &MainWindow::onDownloadProgress);
    connect(downloadManager, &DownloadManager::downloadFinished, this, &MainWindow::onDownloadFinished);
    connect(downloadManager, &DownloadManager::logMessage, this, &MainWindow::onLogMessage);
    connect(downloadManager, &DownloadManager::threadProgressUpdated, this, &MainWindow::onThreadProgress);
    connect(downloadManager, &DownloadManager::speedUpdated, this, [this](const QString &speed) {
        speedLabel->setText("Vitesse: " + speed);
    });
    connect(downloadManager, &DownloadManager::fileSizeReceived, this, [this](const QString &size) {
        fileSizeLabel->setText("Taille: " + size);
    });
}

MainWindow::~MainWindow() {}

void MainWindow::setupUI() {
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // ===== HEADER =====
    QLabel *titleLabel = new QLabel("⚡ TÉLÉCHARGEUR MULTI-THREAD Fast⚡DOM'S", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #2196F3; padding: 10px;");
    mainLayout->addWidget(titleLabel);

    // ===== URL SECTION =====
    QGroupBox *urlGroup = new QGroupBox("Configuration du téléchargement", this);
    QVBoxLayout *urlLayout = new QVBoxLayout(urlGroup);

    QLabel *urlLabel = new QLabel("🌐 URL du fichier:", this);
    urlLabel->setStyleSheet("font-weight: bold; font-size: 13px;");
    urlInput = new QLineEdit(this);
    urlInput->setPlaceholderText("https://exemple.com/fichier.zip");
    urlInput->setMinimumHeight(35);

    urlLayout->addWidget(urlLabel);
    urlLayout->addWidget(urlInput);

    QLabel *pathLabel = new QLabel("💾 Chemin de sauvegarde:", this);
    pathLabel->setStyleSheet("font-weight: bold; font-size: 13px;");
    QHBoxLayout *pathLayout = new QHBoxLayout();
    savePathInput = new QLineEdit(this);
    savePathInput->setPlaceholderText("C:/Downloads/fichier.zip");
    savePathInput->setMinimumHeight(35);
    browseButton = new QPushButton("📁 Parcourir", this);
    browseButton->setMinimumHeight(35);
    browseButton->setMinimumWidth(120);
    pathLayout->addWidget(savePathInput);
    pathLayout->addWidget(browseButton);

    urlLayout->addWidget(pathLabel);
    urlLayout->addLayout(pathLayout);

    mainLayout->addWidget(urlGroup);

    // ===== CONTROL BUTTONS =====
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);

    downloadButton = new QPushButton("▶ DÉMARRER", this);
    downloadButton->setMinimumHeight(45);
    downloadButton->setMinimumWidth(150);

    pauseButton = new QPushButton("⏸ PAUSE", this);
    pauseButton->setMinimumHeight(45);
    pauseButton->setMinimumWidth(150);
    pauseButton->setEnabled(false);

    cancelButton = new QPushButton("✖ ANNULER", this);
    cancelButton->setMinimumHeight(45);
    cancelButton->setMinimumWidth(150);
    cancelButton->setEnabled(false);

    buttonLayout->addWidget(downloadButton);
    buttonLayout->addWidget(pauseButton);
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addStretch();

    mainLayout->addLayout(buttonLayout);

    // ===== PROGRESS SECTION =====
    QGroupBox *progressGroup = new QGroupBox("Progression globale", this);
    QVBoxLayout *progressLayout = new QVBoxLayout(progressGroup);

    globalProgressBar = new QProgressBar(this);
    globalProgressBar->setRange(0, 100);
    globalProgressBar->setValue(0);
    globalProgressBar->setMinimumHeight(30);
    globalProgressBar->setTextVisible(true);

    progressLayout->addWidget(globalProgressBar);

    // Status info
    QHBoxLayout *statusLayout = new QHBoxLayout();
    statusLabel = new QLabel("⏳ En attente...", this);
    speedLabel = new QLabel("Vitesse: 0 MB/s", this);
    fileSizeLabel = new QLabel("Taille: -", this);
    elapsedTimeLabel = new QLabel("Temps: 00:00", this);

    statusLayout->addWidget(statusLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(fileSizeLabel);
    statusLayout->addWidget(speedLabel);
    statusLayout->addWidget(elapsedTimeLabel);

    progressLayout->addLayout(statusLayout);
    mainLayout->addWidget(progressGroup);

    // ===== THREAD TABLE =====
    QGroupBox *threadGroup = new QGroupBox("État des threads (10 threads parallèles)", this);
    QVBoxLayout *threadLayout = new QVBoxLayout(threadGroup);

    threadTable = new QTableWidget(10, 3, this);
    threadTable->setHorizontalHeaderLabels({"Thread", "Progression", "État"});
    threadTable->horizontalHeader()->setStretchLastSection(true);
    threadTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    threadTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    threadTable->setColumnWidth(0, 80);
    threadTable->setMaximumHeight(280);
    threadTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    threadTable->setSelectionMode(QAbstractItemView::NoSelection);

    // Initialize table
    for (int i = 0; i < 10; i++) {
        threadTable->setItem(i, 0, new QTableWidgetItem(QString("Thread %1").arg(i)));

        QProgressBar *bar = new QProgressBar();
        bar->setRange(0, 100);
        bar->setValue(0);
        bar->setTextVisible(true);
        bar->setMaximumHeight(20);
        threadTable->setCellWidget(i, 1, bar);

        threadTable->setItem(i, 2, new QTableWidgetItem("⏳ En attente"));
    }

    threadLayout->addWidget(threadTable);
    mainLayout->addWidget(threadGroup);

    // ===== LOG OUTPUT =====
    QGroupBox *logGroup = new QGroupBox("Journal d'activité", this);
    QVBoxLayout *logLayout = new QVBoxLayout(logGroup);

    logOutput = new QTextEdit(this);
    logOutput->setReadOnly(true);
    logOutput->setMaximumHeight(120);

    logLayout->addWidget(logOutput);
    mainLayout->addWidget(logGroup);
}

void MainWindow::setupStyles() {
    setStyleSheet(R"(
        QMainWindow {
            background-color: #f5f5f5;
        }
        QGroupBox {
            font-weight: bold;
            border: 2px solid #2196F3;
            border-radius: 8px;
            margin-top: 10px;
            padding-top: 10px;
            background-color: white;
        }
        QGroupBox::title {
            color: #2196F3;
            subcontrol-origin: margin;
            left: 15px;
            padding: 0 5px;
        }
        QLineEdit {
            border: 2px solid #ddd;
            border-radius: 5px;
            padding: 8px;
            font-size: 13px;
            background-color: white;
            color : black;
        }
        QLineEdit:focus {
            border: 2px solid #2196F3;
        }
        QPushButton {
            border: none;
            border-radius: 5px;
            padding: 10px;
            font-weight: bold;
            font-size: 13px;
        }
        QProgressBar {
            border: 2px solid #ddd;
            border-radius: 5px;
            text-align: center;
            font-weight: bold;
        }
        QProgressBar::chunk {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #4CAF50, stop:1 #8BC34A);
            border-radius: 3px;
        }
        QTextEdit {
            border: 2px solid #ddd;
            border-radius: 5px;
            background-color: #fafafa;
            font-family: 'Courier New', monospace;
            font-size: 11px;
            color : black;
        }
        QTableWidget {
            border: 2px solid #ddd;
            border-radius: 5px;
            background-color: white;
            gridline-color: #e0e0e0;
        }
        QHeaderView::section {
            background-color: #2196F3;
            color: white;
            padding: 5px;
            border: none;
            font-weight: bold;
        }
    )");

    downloadButton->setStyleSheet("background-color: #4CAF50; color: white;");
    pauseButton->setStyleSheet("background-color: #FF9800; color: white;");
    cancelButton->setStyleSheet("background-color: #f44336; color: white;");
    browseButton->setStyleSheet("background-color: #2196F3; color: white;");
}

void MainWindow::startDownload() {
    QString url = urlInput->text().trimmed();
    QString savePath = savePathInput->text().trimmed();

    if (url.isEmpty() || savePath.isEmpty()) {
        QMessageBox::warning(this, "⚠ Erreur", "Veuillez remplir tous les champs.");
        return;
    }

    downloadButton->setEnabled(false);
    pauseButton->setEnabled(true);
    cancelButton->setEnabled(true);

    globalProgressBar->setValue(0);
    statusLabel->setText("🚀 Démarrage...");
    logOutput->clear();

    // Reset thread table
    for (int i = 0; i < 10; i++) {
        QProgressBar *bar = qobject_cast<QProgressBar*>(threadTable->cellWidget(i, 1));
        if (bar) bar->setValue(0);
        threadTable->item(i, 2)->setText("⏳ En attente");
    }

    downloadManager->startDownload(url, savePath);
}

void MainWindow::pauseDownload() {
    isPaused = !isPaused;
    if (isPaused) {
        pauseButton->setText("▶ REPRENDRE");
        statusLabel->setText("⏸ En pause");
    } else {
        pauseButton->setText("⏸ PAUSE");
        statusLabel->setText("🚀 Téléchargement en cours...");
    }
}

void MainWindow::cancelDownload() {
    downloadButton->setEnabled(true);
    pauseButton->setEnabled(false);
    cancelButton->setEnabled(false);
    statusLabel->setText("✖ Téléchargement annulé");

    for (int i = 0; i < 10; i++) {
        threadTable->item(i, 2)->setText("✖ Annulé");
    }
}

void MainWindow::onDownloadProgress(int percentage) {
    globalProgressBar->setValue(percentage);
    statusLabel->setText(QString("📥 Téléchargement: %1%").arg(percentage));
}

void MainWindow::onDownloadFinished(bool success, const QString &message) {
    downloadButton->setEnabled(true);
    pauseButton->setEnabled(false);
    cancelButton->setEnabled(false);

    if (success) {
        statusLabel->setText("✅ Terminé avec succès!");
        statusLabel->setStyleSheet("color: green; font-weight: bold;");

        for (int i = 0; i < 10; i++) {
            threadTable->item(i, 2)->setText("✅ Terminé");
            threadTable->item(i, 2)->setBackground(QColor(200, 255, 200));
        }

        QMessageBox::information(this, "✅ Succès", message);
    } else {
        statusLabel->setText("❌ Erreur");
        statusLabel->setStyleSheet("color: red; font-weight: bold;");
        QMessageBox::critical(this, "❌ Erreur", message);
    }
}

void MainWindow::onLogMessage(const QString &message) {
    logOutput->append(message);
}

void MainWindow::onThreadProgress(int threadId, int percentage) {
    if (threadId >= 0 && threadId < 10) {
        QProgressBar *bar = qobject_cast<QProgressBar*>(threadTable->cellWidget(threadId, 1));
        if (bar) {
            bar->setValue(percentage);
        }

        QString status;
        if (percentage == 0) {
            status = "⏳ Démarrage...";
        } else if (percentage == 100) {
            status = "✅ Terminé";
            threadTable->item(threadId, 2)->setBackground(QColor(200, 255, 200));
        } else {
            status = QString("📥 %1%").arg(percentage);
        }
        threadTable->item(threadId, 2)->setText(status);
    }
}
