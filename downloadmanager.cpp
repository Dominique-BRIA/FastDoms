#include "downloadmanager.h"
#include <QNetworkRequest>
#include <QThread>

DownloadThread::DownloadThread(int id, const QString &url, qint64 start, qint64 end, QObject *parent)
    : QObject(parent), threadId(id), downloadUrl(url), startByte(start), endByte(end), reply(nullptr) {
    networkManager = new QNetworkAccessManager(this);
}

void DownloadThread::start() {
    QNetworkRequest request(downloadUrl);
    QString range = QString("bytes=%1-%2").arg(startByte).arg(endByte);
    request.setRawHeader("Range", range.toUtf8());

    reply = networkManager->get(request);
    connect(reply, &QNetworkReply::readyRead, this, &DownloadThread::onReadyRead);
    connect(reply, &QNetworkReply::finished, this, &DownloadThread::onFinished);
    connect(reply, &QNetworkReply::downloadProgress, this, &DownloadThread::onDownloadProgress);
}

void DownloadThread::onReadyRead() {
    if (reply) {
        downloadedData.append(reply->readAll());
    }
}

void DownloadThread::onFinished() {
    if (reply->error() == QNetworkReply::NoError) {
        emit chunkDownloaded(threadId, downloadedData);
    } else {
        emit chunkError(threadId, reply->errorString());
    }
    reply->deleteLater();
}

void DownloadThread::onDownloadProgress(qint64 received, qint64 total) {
    emit chunkProgress(threadId, received, total);
}

DownloadManager::DownloadManager(QObject *parent)
    : QObject(parent), fileSize(0), numThreads(10), completedChunks(0), lastBytesReceived(0) {
    headManager = new QNetworkAccessManager(this);
    speedTimer = new QTimer(this);
    connect(speedTimer, &QTimer::timeout, this, &DownloadManager::updateSpeed);
}

DownloadManager::~DownloadManager() {
    qDeleteAll(threads);
}

void DownloadManager::startDownload(const QString &url, const QString &savePath) {
    fileUrl = url;
    fileSavePath = savePath;
    completedChunks = 0;
    lastBytesReceived = 0;

    threads.clear();
    chunks.clear();
    chunkProgress.clear();
    chunkTotal.clear();

    emit logMessage("🔍 Récupération des informations du fichier...");

    QNetworkRequest headRequest(url);
    QNetworkReply *headReply = headManager->head(headRequest);
    connect(headReply, &QNetworkReply::finished, this, &DownloadManager::onHeadFinished);
}

void DownloadManager::onHeadFinished() {
    QNetworkReply *headReply = qobject_cast<QNetworkReply*>(sender());

    if (headReply->error() != QNetworkReply::NoError) {
        emit logMessage("❌ Erreur: " + headReply->errorString());
        emit downloadFinished(false, "Impossible de récupérer les informations du fichier: " + headReply->errorString());
        headReply->deleteLater();
        return;
    }

    fileSize = headReply->header(QNetworkRequest::ContentLengthHeader).toLongLong();

    if (fileSize <= 0) {
        emit logMessage("❌ Erreur: Taille du fichier invalide");
        emit downloadFinished(false, "Le serveur ne supporte pas les téléchargements fragmentés.");
        headReply->deleteLater();
        return;
    }

    emit fileSizeReceived(formatSize(fileSize));
    emit logMessage(QString("📊 Taille du fichier: %1").arg(formatSize(fileSize)));
    emit logMessage(QString("🚀 Démarrage avec %1 threads parallèles...").arg(numThreads));

    chunks.resize(numThreads);
    chunkProgress.resize(numThreads);
    chunkProgress.fill(0);
    chunkTotal.resize(numThreads);

    qint64 chunkSize = fileSize / numThreads;

    startTime = QTime::currentTime();
    speedTimer->start(1000);

    for (int i = 0; i < numThreads; ++i) {
        qint64 start = i * chunkSize;
        qint64 end = (i == numThreads - 1) ? fileSize - 1 : (start + chunkSize - 1);

        chunkTotal[i] = end - start + 1;

        DownloadThread *thread = new DownloadThread(i, fileUrl, start, end, this);
        threads.append(thread);

        connect(thread, &DownloadThread::chunkDownloaded, this, &DownloadManager::onChunkDownloaded);
        connect(thread, &DownloadThread::chunkProgress, this, &DownloadManager::onChunkProgress);
        connect(thread, &DownloadThread::chunkError, this, &DownloadManager::onChunkError);

        QThread *workerThread = new QThread(this);
        thread->moveToThread(workerThread);
        connect(workerThread, &QThread::started, thread, &DownloadThread::start);
        workerThread->start();

        emit logMessage(QString("✅ Thread %1 démarré: %2 - %3 (%4)").arg(i).arg(start).arg(end).arg(formatSize(end - start + 1)));
    }

    headReply->deleteLater();
}

void DownloadManager::onChunkDownloaded(int id, const QByteArray &data) {
    QMutexLocker locker(&mutex);

    chunks[id] = data;
    completedChunks++;

    emit logMessage(QString("✅ Thread %1 terminé (%2/%3)").arg(id).arg(completedChunks).arg(numThreads));
    emit threadProgressUpdated(id, 100);

    if (completedChunks == numThreads) {
        speedTimer->stop();
        assembleFile();
    }
}

void DownloadManager::onChunkProgress(int id, qint64 received, qint64 total) {
    QMutexLocker locker(&mutex);

    chunkProgress[id] = received;

    qint64 totalReceived = 0;
    for (qint64 progress : chunkProgress) {
        totalReceived += progress;
    }

    int percentage = (fileSize > 0) ? (totalReceived * 100 / fileSize) : 0;
    emit progressUpdated(percentage);

    int threadPercentage = (total > 0) ? (received * 100 / total) : 0;
    emit threadProgressUpdated(id, threadPercentage);
}

void DownloadManager::onChunkError(int id, const QString &error) {
    emit logMessage(QString("❌ Erreur thread %1: %2").arg(id).arg(error));
    emit downloadFinished(false, QString("Erreur lors du téléchargement: %1").arg(error));
}

void DownloadManager::updateSpeed() {
    QMutexLocker locker(&mutex);

    qint64 totalReceived = 0;
    for (qint64 progress : chunkProgress) {
        totalReceived += progress;
    }

    qint64 bytesPerSecond = totalReceived - lastBytesReceived;
    lastBytesReceived = totalReceived;

    emit speedUpdated(formatSize(bytesPerSecond) + "/s");
}

void DownloadManager::assembleFile() {
    emit logMessage("🔧 Assemblage des fragments...");

    QFile file(fileSavePath);
    if (!file.open(QIODevice::WriteOnly)) {
        emit logMessage("❌ Erreur: Impossible de créer le fichier");
        emit downloadFinished(false, "Impossible de créer le fichier: " + file.errorString());
        return;
    }

    for (int i = 0; i < numThreads; ++i) {
        file.write(chunks[i]);
    }

    file.close();

    int elapsed = startTime.secsTo(QTime::currentTime());
    int minutes = elapsed / 60;
    int seconds = elapsed % 60;

    emit logMessage(QString("✅ Fichier assemblé avec succès: %1").arg(fileSavePath));
    emit logMessage(QString("⏱ Temps total: %1:%2").arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0')));
    emit downloadFinished(true, QString("Téléchargement terminé!\n\nFichier: %1\nTaille: %2\nTemps: %3:%4")
                                    .arg(fileSavePath)
                                    .arg(formatSize(fileSize))
                                    .arg(minutes, 2, 10, QChar('0'))
                                    .arg(seconds, 2, 10, QChar('0')));
}

QString DownloadManager::formatSize(qint64 bytes) {
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 2);
    if (bytes < 1024 * 1024 * 1024) return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);
    return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}
