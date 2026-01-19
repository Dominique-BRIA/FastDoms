#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QTextEdit>
#include <QTableWidget>
#include <QGroupBox>
#include "downloadmanager.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void startDownload();
    void pauseDownload();
    void cancelDownload();
    void onDownloadProgress(int percentage);
    void onDownloadFinished(bool success, const QString &message);
    void onLogMessage(const QString &message);
    void onThreadProgress(int threadId, int percentage);

private:
    void setupUI();
    void setupStyles();

    QLineEdit *urlInput;
    QLineEdit *savePathInput;
    QPushButton *downloadButton;
    QPushButton *pauseButton;
    QPushButton *cancelButton;
    QPushButton *browseButton;
    QProgressBar *globalProgressBar;
    QLabel *statusLabel;
    QLabel *speedLabel;
    QLabel *fileSizeLabel;
    QLabel *elapsedTimeLabel;
    QTextEdit *logOutput;
    QTableWidget *threadTable;

    DownloadManager *downloadManager;
    bool isPaused;
};

#endif
