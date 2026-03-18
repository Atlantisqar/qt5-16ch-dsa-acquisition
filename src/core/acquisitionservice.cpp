#include "core/acquisitionservice.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QtMath>

#include <cstring>

namespace {
constexpr int kPollIntervalMs = 8;
constexpr int kMockTickMs = 20;
constexpr unsigned int kContinuousReadThreshold = 1024;
constexpr unsigned int kContinuousReadChunk = 4096;
constexpr unsigned int kContinuousReadChunkHigh = 8192;
constexpr unsigned int kContinuousReadChunkCritical = 16384;
constexpr unsigned int kContinuousMinRead = 256;
constexpr unsigned int kBufferHighWaterMark = 24576;      // 75% of 32768
constexpr unsigned int kBufferCriticalWaterMark = 28672;  // 87.5% of 32768
constexpr unsigned int kMockPointPerTick = 512;
constexpr quint32 kBinaryMagic = 0x31415344;  // "DSA1"
constexpr quint32 kBinaryVersion = 1;
constexpr quint32 kChannelCount = 16;
constexpr qint64 kMaxContinuousFileBytes = 50LL * 1024LL * 1024LL;
constexpr bool kWriteLegacyChannelFiles = false;
}  // namespace

AcquisitionService::AcquisitionService(Dsa16ChDeviceService* deviceService,
                                       MultiChannelDataStore* dataStore,
                                       QObject* parent)
    : QObject(parent), m_deviceService(deviceService), m_dataStore(dataStore) {
    connect(&m_pollTimer, &QTimer::timeout, this, &AcquisitionService::onPollTimeout);
}

AcquisitionService::~AcquisitionService() {
    stopAcquisition();
}

void AcquisitionService::setAcquisitionSettings(const dsa::DsaAcquisitionSettings& settings) {
    m_settings = settings;
}

void AcquisitionService::setDataDirectory(const QString& dataDirectory) {
    m_dataDirectory = dataDirectory;
}

void AcquisitionService::setMockModeEnabled(bool enabled) {
    m_mockModeEnabled = enabled;
}

bool AcquisitionService::startAcquisition() {
    if (m_running) {
        return true;
    }
    if (m_dataStore == nullptr) {
        return setError("Acquisition service init failed: data store is null.");
    }
    if (m_deviceService == nullptr) {
        return setError("Acquisition service init failed: device service is null.");
    }

    QString validationError;
    if (!m_settings.validate(&validationError)) {
        return setError(QString("Invalid acquisition settings: %1").arg(validationError));
    }

    m_dataStore->clear();
    m_lastBufferPointCount = 0;
    m_lastOverflow = false;
    m_sessionStartMs = QDateTime::currentMSecsSinceEpoch();
    m_sessionBinaryBaseName.clear();
    m_sessionBinaryFilePath.clear();
    m_sessionBinaryFileIndex = 0;
    m_mockPhase = 0.0;
    closeOutputFiles();

    if (m_deviceService->isDeviceOpen()) {
        return startRealAcquisition();
    }
    if (m_mockModeEnabled) {
        return startMockAcquisition();
    }
    return setError("Start acquisition failed: device not open and mock mode disabled.");
}

void AcquisitionService::stopAcquisition() {
    if (!m_running) {
        m_pollTimer.stop();
        m_mockRunning = false;
        closeOutputFiles();
        return;
    }

    m_pollTimer.stop();

    if (!m_mockRunning && m_deviceService != nullptr && m_deviceService->isDeviceOpen()) {
        if (!m_deviceService->stopAcquisition()) {
            const QString message = m_deviceService->lastError();
            setError(message);
            emit errorOccurred(message);
        }
    }

    closeOutputFiles();
    m_mockRunning = false;
    m_running = false;
    emit runningChanged(false);
}

bool AcquisitionService::isRunning() const {
    return m_running;
}

bool AcquisitionService::isMockModeEnabled() const {
    return m_mockModeEnabled;
}

QString AcquisitionService::lastError() const {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    return m_lastError;
}

bool AcquisitionService::startRealAcquisition() {
    if (!m_deviceService->startAcquisition()) {
        return setError(m_deviceService->lastError());
    }

    if (shouldWriteToDisk() && !ensureOutputFilesReady()) {
        m_deviceService->stopAcquisition();
        return false;
    }

    m_mockRunning = false;
    m_running = true;
    m_pollTimer.start(kPollIntervalMs);
    emit runningChanged(true);
    return true;
}

bool AcquisitionService::startMockAcquisition() {
    if (shouldWriteToDisk() && !ensureOutputFilesReady()) {
        return false;
    }

    m_mockRunning = true;
    m_running = true;
    m_pollTimer.start(kMockTickMs);
    emit runningChanged(true);
    return true;
}

void AcquisitionService::onPollTimeout() {
    if (!m_running) {
        return;
    }

    if (m_mockRunning) {
        generateMockAndStore(kMockPointPerTick);
        emit bufferPointCountUpdated(kMockPointPerTick);
        if (m_lastOverflow) {
            m_lastOverflow = false;
            emit overflowDetected(false);
        }
        return;
    }

    unsigned int bufferPointCount = 0;
    if (!m_deviceService->queryBufferPointCount(bufferPointCount)) {
        const QString message = m_deviceService->lastError();
        setError(message);
        emit errorOccurred(message);
        stopAcquisition();
        return;
    }
    m_lastBufferPointCount = bufferPointCount;
    emit bufferPointCountUpdated(bufferPointCount);

    unsigned int pointsToRead = 0;
    if (!computePointsToRead(bufferPointCount, pointsToRead)) {
        setError("Compute read points failed.");
        emit errorOccurred(lastError());
        stopAcquisition();
        return;
    }

    if (pointsToRead > 0) {
        std::array<QVector<double>, dsa::kChannelCount> samples;
        if (!m_deviceService->readData(pointsToRead, samples)) {
            const QString message = m_deviceService->lastError();
            setError(message);
            emit errorOccurred(message);
            stopAcquisition();
            return;
        }
        processReadFrame(pointsToRead, std::move(samples));
    }

    bool overflow = false;
    if (!m_deviceService->queryOverflow(overflow)) {
        const QString message = m_deviceService->lastError();
        setError(message);
        emit errorOccurred(message);
        stopAcquisition();
        return;
    }
    if (overflow != m_lastOverflow) {
        m_lastOverflow = overflow;
        emit overflowDetected(overflow);
    }
}

bool AcquisitionService::computePointsToRead(unsigned int bufferPointCount, unsigned int& pointsToRead) const {
    pointsToRead = 0;

    const bool continuousMode =
        m_settings.sampleMode == static_cast<unsigned int>(dsa::SampleMode::Continuous);
    if (continuousMode) {
        if (bufferPointCount >= kBufferCriticalWaterMark) {
            pointsToRead = qMin(bufferPointCount, kContinuousReadChunkCritical);
        } else if (bufferPointCount >= kBufferHighWaterMark) {
            pointsToRead = qMin(bufferPointCount, kContinuousReadChunkHigh);
        } else if (bufferPointCount >= kContinuousReadThreshold) {
            pointsToRead = qMin(bufferPointCount, kContinuousReadChunk);
        } else if (bufferPointCount >= kContinuousMinRead) {
            pointsToRead = bufferPointCount;
        }
        pointsToRead = (pointsToRead / dsa::kPointAlign) * dsa::kPointAlign;
        return true;
    }

    const unsigned int fixedPointTarget =
        (m_settings.fixedPointCountPerCh / dsa::kPointAlign) * dsa::kPointAlign;
    if (fixedPointTarget > 0 && bufferPointCount >= fixedPointTarget) {
        pointsToRead = fixedPointTarget;
    }
    return true;
}

void AcquisitionService::processReadFrame(unsigned int pointsToRead,
                                          std::array<QVector<double>, dsa::kChannelCount>&& samples) {
    AcquisitionFrame frame;
    frame.timestampMs = QDateTime::currentMSecsSinceEpoch();
    frame.pointCountPerChannel = pointsToRead;
    frame.channels = std::move(samples);

    const quint64 frameIndex = m_dataStore->appendFrame(frame);
    emit frameStored(frameIndex, pointsToRead);

    if (shouldWriteToDisk()) {
        writeFrameToDisk(frame);
    }
}

void AcquisitionService::generateMockAndStore(unsigned int pointCount) {
    AcquisitionFrame frame;
    frame.timestampMs = QDateTime::currentMSecsSinceEpoch();
    frame.pointCountPerChannel = pointCount;

    for (int channel = 0; channel < dsa::kChannelCount; ++channel) {
        QVector<double> values;
        values.reserve(static_cast<int>(pointCount));
        const double channelScale = 0.8 + channel * 0.03;
        const double freq = 0.02 + channel * 0.003;
        for (unsigned int i = 0; i < pointCount; ++i) {
            const double t = m_mockPhase + i;
            const double y = channelScale * qSin(t * freq) + 0.15 * qSin(t * (freq * 3.0));
            values.push_back(y);
        }
        frame.channels[channel] = std::move(values);
    }
    m_mockPhase += pointCount;

    const quint64 frameIndex = m_dataStore->appendFrame(frame);
    emit frameStored(frameIndex, pointCount);

    if (shouldWriteToDisk()) {
        writeFrameToDisk(frame);
    }
}

bool AcquisitionService::shouldWriteToDisk() const {
    return !m_dataDirectory.trimmed().isEmpty();
}

bool AcquisitionService::ensureOutputFilesReady() {
    if (!shouldWriteToDisk()) {
        return true;
    }

    QDir dir(m_dataDirectory);
    if (!dir.exists() && !dir.mkpath(".")) {
        return setError(QString("Create data directory failed: %1").arg(m_dataDirectory));
    }

    if (!ensureSessionBinaryFileReady(false)) {
        return false;
    }

    if (kWriteLegacyChannelFiles) {
        for (int i = 0; i < dsa::kChannelCount; ++i) {
            if (m_channelFiles[i] != nullptr && m_channelFiles[i]->isOpen()) {
                continue;
            }
            const QString filePath = dir.filePath(QString("ch%1.bin").arg(i + 1));
            m_channelFiles[i].reset(new QFile(filePath));
            if (!m_channelFiles[i]->open(QIODevice::Append)) {
                return setError(QString("Open legacy channel file failed: %1").arg(filePath));
            }
        }
    }
    return true;
}

bool AcquisitionService::ensureSessionBinaryFileReady(bool forceCreateNew) {
    if (!shouldWriteToDisk()) {
        return true;
    }
    if (!forceCreateNew && m_sessionBinaryFile != nullptr && m_sessionBinaryFile->isOpen()) {
        return true;
    }
    return rotateSessionBinaryFile();
}

bool AcquisitionService::rotateSessionBinaryFile() {
    if (!shouldWriteToDisk()) {
        return true;
    }

    QDir dir(m_dataDirectory);
    if (!dir.exists() && !dir.mkpath(".")) {
        return setError(QString("Create data directory failed: %1").arg(m_dataDirectory));
    }

    if (m_sessionBinaryBaseName.isEmpty()) {
        const QDateTime sessionTime = QDateTime::fromMSecsSinceEpoch(m_sessionStartMs);
        m_sessionBinaryBaseName = QString("session_%1").arg(sessionTime.toString("yyyyMMdd_HHmmss_zzz"));
    }

    quint32 candidateIndex = m_sessionBinaryFileIndex + 1;
    QString candidatePath;
    do {
        candidatePath = dir.filePath(
            QString("%1_part%2.bin").arg(m_sessionBinaryBaseName).arg(candidateIndex, 3, 10, QChar('0')));
        if (!QFileInfo::exists(candidatePath)) {
            break;
        }
        ++candidateIndex;
    } while (true);

    std::unique_ptr<QFile> newFile(new QFile(candidatePath));
    if (!newFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return setError(QString("Open binary session file failed: %1").arg(candidatePath));
    }

    const quint32 magic = kBinaryMagic;
    const quint32 version = kBinaryVersion;
    const quint32 channelCount = kChannelCount;
    if (newFile->write(reinterpret_cast<const char*>(&magic), sizeof(magic)) != sizeof(magic) ||
        newFile->write(reinterpret_cast<const char*>(&version), sizeof(version)) != sizeof(version) ||
        newFile->write(reinterpret_cast<const char*>(&channelCount), sizeof(channelCount)) != sizeof(channelCount)) {
        newFile->close();
        return setError(QString("Write binary session header failed: %1").arg(candidatePath));
    }

    if (m_sessionBinaryFile != nullptr && m_sessionBinaryFile->isOpen()) {
        m_sessionBinaryFile->flush();
        m_sessionBinaryFile->close();
    }
    m_sessionBinaryFile = std::move(newFile);
    m_sessionBinaryFilePath = candidatePath;
    m_sessionBinaryFileIndex = candidateIndex;
    return true;
}

bool AcquisitionService::isContinuousModeForFileSplit() const {
    const unsigned int mode = m_settings.sampleMode;
    return mode == static_cast<unsigned int>(dsa::SampleMode::Continuous) ||
           mode == static_cast<unsigned int>(dsa::SampleMode::ContinuousFixedPoint);
}

qint64 AcquisitionService::estimateFrameBytes(const AcquisitionFrame& frame) const {
    qint64 bytes = static_cast<qint64>(sizeof(quint64) + sizeof(quint32) + sizeof(quint32));
    for (const QVector<double>& channelData : frame.channels) {
        bytes += static_cast<qint64>(channelData.size()) * static_cast<qint64>(sizeof(double));
    }
    return bytes;
}

void AcquisitionService::writeFrameToDisk(const AcquisitionFrame& frame) {
    if (!shouldWriteToDisk()) {
        return;
    }
    if (!ensureOutputFilesReady()) {
        emit errorOccurred(lastError());
        return;
    }

    if (isContinuousModeForFileSplit() && m_sessionBinaryFile != nullptr && m_sessionBinaryFile->isOpen()) {
        const qint64 frameBytes = estimateFrameBytes(frame);
        const qint64 projectedSize = m_sessionBinaryFile->size() + frameBytes;
        if (projectedSize > kMaxContinuousFileBytes) {
            if (!rotateSessionBinaryFile()) {
                emit errorOccurred(QString("Rotate binary file failed: %1").arg(lastError()));
            }
        }
    }

    if (m_sessionBinaryFile != nullptr && m_sessionBinaryFile->isOpen()) {
        const quint64 ts = static_cast<quint64>(frame.timestampMs);
        const quint32 pointCount = frame.pointCountPerChannel;
        const quint32 channelCount = kChannelCount;
        if (m_sessionBinaryFile->write(reinterpret_cast<const char*>(&ts), sizeof(ts)) != sizeof(ts) ||
            m_sessionBinaryFile->write(reinterpret_cast<const char*>(&pointCount), sizeof(pointCount)) !=
                sizeof(pointCount) ||
            m_sessionBinaryFile->write(reinterpret_cast<const char*>(&channelCount), sizeof(channelCount)) !=
                sizeof(channelCount)) {
            emit errorOccurred(QString("Write session frame header failed: %1").arg(m_sessionBinaryFilePath));
        } else {
            for (int i = 0; i < dsa::kChannelCount; ++i) {
                const QVector<double>& channelData = frame.channels[i];
                if (channelData.isEmpty()) {
                    continue;
                }
                const char* raw = reinterpret_cast<const char*>(channelData.constData());
                const qint64 bytes = static_cast<qint64>(channelData.size() * static_cast<int>(sizeof(double)));
                if (m_sessionBinaryFile->write(raw, bytes) != bytes) {
                    emit errorOccurred(QString("Write session channel data failed: ch%1").arg(i + 1));
                    break;
                }
            }
        }
    }

    if (kWriteLegacyChannelFiles) {
        writeLegacyChannelFiles(frame);
    }
}

void AcquisitionService::writeLegacyChannelFiles(const AcquisitionFrame& frame) {
    for (int i = 0; i < dsa::kChannelCount; ++i) {
        const QVector<double>& channelData = frame.channels[i];
        if (channelData.isEmpty() || m_channelFiles[i] == nullptr || !m_channelFiles[i]->isOpen()) {
            continue;
        }
        const char* raw = reinterpret_cast<const char*>(channelData.constData());
        const qint64 bytes = static_cast<qint64>(channelData.size() * static_cast<int>(sizeof(double)));
        const qint64 written = m_channelFiles[i]->write(raw, bytes);
        if (written != bytes) {
            emit errorOccurred(QString("Write legacy channel data failed: ch%1.bin").arg(i + 1));
        }
    }
}

void AcquisitionService::closeOutputFiles() {
    if (m_sessionBinaryFile != nullptr) {
        if (m_sessionBinaryFile->isOpen()) {
            m_sessionBinaryFile->flush();
            m_sessionBinaryFile->close();
        }
        m_sessionBinaryFile.reset();
    }
    m_sessionBinaryFilePath.clear();

    for (std::unique_ptr<QFile>& file : m_channelFiles) {
        if (file != nullptr) {
            if (file->isOpen()) {
                file->flush();
                file->close();
            }
            file.reset();
        }
    }
}

bool AcquisitionService::setError(const QString& message) {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    m_lastError = message;
    return false;
}
