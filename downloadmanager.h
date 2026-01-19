#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QVector>
#include <QMutex>
#include <QTime>
#include <QTimer>

class DownloadThread : public QObject {
    Q_OBJECT

public:
    DownloadThread(int id, const QString &url, qint64 start, qint64 end, QObject *parent = nullptr);
    void start();

signals:
    void chunkDownloaded(int id, const QByteArray &data);
    void chunkProgress(int id, qint64 received, qint64 total);
    void chunkError(int id, const QString &error);

private slots:
    void onReadyRead();
    void onFinished();
    void onDownloadProgress(qint64 received, qint64 total);

private:
    int threadId;
    QString downloadUrl;
    qint64 startByte;
    qint64 endByte;
    QNetworkAccessManager *networkManager;
    QNetworkReply *reply;
    QByteArray downloadedData;
};

class DownloadManager : public QObject {
    Q_OBJECT

public:
    explicit DownloadManager(QObject *parent = nullptr);
    ~DownloadManager();
    void startDownload(const QString &url, const QString &savePath);

signals:
    void progressUpdated(int percentage);
    void downloadFinished(bool success, const QString &message);
    void logMessage(const QString &message);
    void threadProgressUpdated(int threadId, int percentage);
    void speedUpdated(const QString &speed);
    void fileSizeReceived(const QString &size);

private slots:
    void onChunkDownloaded(int id, const QByteArray &data);
    void onChunkProgress(int id, qint64 received, qint64 total);
    void onChunkError(int id, const QString &error);
    void onHeadFinished();
    void updateSpeed();

private:
    void assembleFile();
    QString formatSize(qint64 bytes);

    QString fileUrl;
    QString fileSavePath;
    qint64 fileSize;
    int numThreads;

    QNetworkAccessManager *headManager;
    QVector<DownloadThread*> threads;
    QVector<QByteArray> chunks;
    QVector<qint64> chunkProgress;
    QVector<qint64> chunkTotal;

    int completedChunks;
    QMutex mutex;

    QTime startTime;
    QTimer *speedTimer;
    qint64 lastBytesReceived;
};

#endif
