#include "core/acquisitiontcpreceiverservice.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>

#include <cstring>

namespace {
constexpr quint32 kSenderMagic = 0x4E415344;   // "DSAN"
constexpr quint32 kSenderVersion = 1;
constexpr quint32 kDiskMagic = 0x31415344;     // "DSA1"
constexpr quint32 kDiskVersion = 1;
constexpr qint64 kFrameHeaderBytes = 44;
}  // namespace

AcquisitionTcpReceiverService::AcquisitionTcpReceiverService(MultiChannelDataStore* dataStore, QObject* parent)
    : QObject(parent), m_dataStore(dataStore), m_server(new QTcpServer(this)) {
    connect(m_server, &QTcpServer::newConnection, this, &AcquisitionTcpReceiverService::onNewConnection);
    setState(false, false, QStringLiteral("未连接"));
}

AcquisitionTcpReceiverService::~AcquisitionTcpReceiverService() {
    stopReceiving();
}

void AcquisitionTcpReceiverService::setReceiverSettings(const dsa::DsaReceiverSettings& settings) {
    m_settings = settings;
}

void AcquisitionTcpReceiverService::setDataDirectory(const QString& dataDirectory) {
    m_dataDirectory = dataDirectory;
}

bool AcquisitionTcpReceiverService::startReceiving() {
    QString validationError;
    if (!m_settings.validate(&validationError)) {
        return setError(validationError);
    }
    if (m_dataStore == nullptr) {
        return setError(QStringLiteral("Receiver data store is null."));
    }

    stopReceiving();
    m_dataStore->clear();
    m_readBuffer.clear();

    QHostAddress listenAddress = QHostAddress::Any;
    if (m_settings.listenHost != QStringLiteral("0.0.0.0")) {
        if (!listenAddress.setAddress(m_settings.listenHost)) {
            return setError(QStringLiteral("无效的监听地址：%1").arg(m_settings.listenHost));
        }
    }

    if (!m_server->listen(listenAddress, static_cast<quint16>(m_settings.listenPort))) {
        return setError(QStringLiteral("启动监听失败：%1").arg(m_server->errorString()));
    }

    if (m_settings.saveToDisk && !ensureOutputFileReady()) {
        m_server->close();
        return false;
    }

    m_running = true;
    emit runningChanged(true);
    setState(true, false, QStringLiteral("监听中"));
    return true;
}

void AcquisitionTcpReceiverService::stopReceiving() {
    if (m_clientSocket != nullptr) {
        disconnect(m_clientSocket, nullptr, this, nullptr);
        m_clientSocket->disconnectFromHost();
        m_clientSocket->deleteLater();
        m_clientSocket = nullptr;
    }
    if (m_server->isListening()) {
        m_server->close();
    }
    if (m_sessionFile != nullptr) {
        if (m_sessionFile->isOpen()) {
            m_sessionFile->flush();
            m_sessionFile->close();
        }
        m_sessionFile.reset();
    }
    m_readBuffer.clear();
    if (m_running) {
        m_running = false;
        emit runningChanged(false);
    }
    setState(false, false, QStringLiteral("未连接"));
}

bool AcquisitionTcpReceiverService::isRunning() const {
    return m_running;
}

bool AcquisitionTcpReceiverService::isClientConnected() const {
    return m_clientSocket != nullptr && m_clientSocket->state() == QAbstractSocket::ConnectedState;
}

QString AcquisitionTcpReceiverService::statusMessage() const {
    return m_statusMessage;
}

void AcquisitionTcpReceiverService::onNewConnection() {
    while (m_server->hasPendingConnections()) {
        QTcpSocket* socket = m_server->nextPendingConnection();
        if (socket == nullptr) {
            continue;
        }

        if (m_clientSocket != nullptr) {
            socket->disconnectFromHost();
            socket->deleteLater();
            continue;
        }

        m_clientSocket = socket;
        connect(m_clientSocket, &QTcpSocket::readyRead, this, &AcquisitionTcpReceiverService::onClientReadyRead);
        connect(m_clientSocket, &QTcpSocket::disconnected, this, &AcquisitionTcpReceiverService::onClientDisconnected);
        connect(m_clientSocket, &QAbstractSocket::errorOccurred, this, &AcquisitionTcpReceiverService::onClientErrorOccurred);
        setState(true, true, QStringLiteral("已连接"));
    }
}

void AcquisitionTcpReceiverService::onClientDisconnected() {
    if (m_clientSocket != nullptr) {
        m_clientSocket->deleteLater();
        m_clientSocket = nullptr;
    }
    m_readBuffer.clear();
    if (m_running) {
        setState(true, false, QStringLiteral("未连接"));
    } else {
        setState(false, false, QStringLiteral("未连接"));
    }
}

void AcquisitionTcpReceiverService::onClientReadyRead() {
    if (m_clientSocket == nullptr) {
        return;
    }
    m_readBuffer.append(m_clientSocket->readAll());
    if (!processPendingFrames()) {
        emit errorOccurred(m_statusMessage);
        stopReceiving();
    }
}

void AcquisitionTcpReceiverService::onClientErrorOccurred() {
    if (m_clientSocket == nullptr) {
        return;
    }
    setState(true, false, QStringLiteral("网络错误：%1").arg(m_clientSocket->errorString()));
}

bool AcquisitionTcpReceiverService::ensureOutputFileReady() {
    if (!m_settings.saveToDisk) {
        return true;
    }
    if (m_dataDirectory.trimmed().isEmpty()) {
        return setError(QStringLiteral("接收端本地保存目录为空。"));
    }

    QDir dir(m_dataDirectory);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        return setError(QStringLiteral("创建接收数据目录失败：%1").arg(m_dataDirectory));
    }
    if (m_sessionFile != nullptr && m_sessionFile->isOpen()) {
        return true;
    }
    return openSessionFile();
}

bool AcquisitionTcpReceiverService::openSessionFile() {
    QDir dir(m_dataDirectory);
    const QString fileName = QStringLiteral("session_%1.bin")
                                 .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz")));
    const QString filePath = dir.filePath(fileName);

    std::unique_ptr<QFile> file(new QFile(filePath));
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return setError(QStringLiteral("打开接收数据文件失败：%1").arg(filePath));
    }

    const quint32 magic = kDiskMagic;
    const quint32 version = kDiskVersion;
    const quint32 channelCount = static_cast<quint32>(dsa::kChannelCount);
    if (file->write(reinterpret_cast<const char*>(&magic), sizeof(magic)) != sizeof(magic) ||
        file->write(reinterpret_cast<const char*>(&version), sizeof(version)) != sizeof(version) ||
        file->write(reinterpret_cast<const char*>(&channelCount), sizeof(channelCount)) != sizeof(channelCount)) {
        file->close();
        return setError(QStringLiteral("写入接收数据文件头失败：%1").arg(filePath));
    }

    m_sessionFile = std::move(file);
    return true;
}

bool AcquisitionTcpReceiverService::writeFrameToDisk(const AcquisitionFrame& frame) {
    if (!m_settings.saveToDisk) {
        return true;
    }
    if (!ensureOutputFileReady()) {
        return false;
    }
    if (m_sessionFile == nullptr || !m_sessionFile->isOpen()) {
        return setError(QStringLiteral("接收数据文件未打开。"));
    }

    const quint64 timestampMs = static_cast<quint64>(frame.timestampMs);
    const quint32 pointCount = frame.pointCountPerChannel;
    const quint32 channelCount = static_cast<quint32>(dsa::kChannelCount);
    if (m_sessionFile->write(reinterpret_cast<const char*>(&timestampMs), sizeof(timestampMs)) != sizeof(timestampMs) ||
        m_sessionFile->write(reinterpret_cast<const char*>(&pointCount), sizeof(pointCount)) != sizeof(pointCount) ||
        m_sessionFile->write(reinterpret_cast<const char*>(&channelCount), sizeof(channelCount)) != sizeof(channelCount)) {
        return setError(QStringLiteral("写入接收帧头失败。"));
    }

    for (int i = 0; i < dsa::kChannelCount; ++i) {
        const QVector<double>& channelData = frame.channels[i];
        if (channelData.isEmpty()) {
            continue;
        }
        const char* raw = reinterpret_cast<const char*>(channelData.constData());
        const qint64 bytes = static_cast<qint64>(channelData.size()) * static_cast<qint64>(sizeof(double));
        if (m_sessionFile->write(raw, bytes) != bytes) {
            return setError(QStringLiteral("写入接收通道数据失败。"));
        }
    }
    return true;
}

bool AcquisitionTcpReceiverService::processPendingFrames() {
    auto readU32 = [](const char* data) -> quint32 {
        quint32 value = 0;
        memcpy(&value, data, sizeof(value));
        return value;
    };
    auto readU64 = [](const char* data) -> quint64 {
        quint64 value = 0;
        memcpy(&value, data, sizeof(value));
        return value;
    };

    while (m_readBuffer.size() >= kFrameHeaderBytes) {
        const char* raw = m_readBuffer.constData();
        const quint32 magic = readU32(raw + 0);
        const quint32 version = readU32(raw + 4);
        const quint32 channelCount = readU32(raw + 8);
        const quint32 pointCount = readU32(raw + 12);
        const quint64 timestampMs = readU64(raw + 16);

        if (magic != kSenderMagic || version != kSenderVersion) {
            return setError(QStringLiteral("接收到未知协议数据帧。"));
        }
        if (channelCount != static_cast<quint32>(dsa::kChannelCount) || pointCount == 0 ||
            pointCount > dsa::kMaxPointPerChannel) {
            return setError(QStringLiteral("接收到无效数据帧参数。"));
        }

        const qint64 payloadBytes =
            static_cast<qint64>(channelCount) * static_cast<qint64>(pointCount) * static_cast<qint64>(sizeof(double));
        const qint64 frameBytes = kFrameHeaderBytes + payloadBytes;
        if (m_readBuffer.size() < frameBytes) {
            return true;
        }

        AcquisitionFrame frame;
        frame.timestampMs = static_cast<qint64>(timestampMs);
        frame.pointCountPerChannel = pointCount;

        const char* payload = raw + kFrameHeaderBytes;
        for (int channel = 0; channel < dsa::kChannelCount; ++channel) {
            QVector<double> values(static_cast<int>(pointCount));
            const qint64 channelBytes = static_cast<qint64>(pointCount) * static_cast<qint64>(sizeof(double));
            memcpy(values.data(), payload + channel * channelBytes, static_cast<size_t>(channelBytes));
            frame.channels[channel] = std::move(values);
        }

        m_dataStore->appendFrame(frame);
        if (!writeFrameToDisk(frame)) {
            return false;
        }
        emit bufferPointCountUpdated(static_cast<unsigned int>(m_dataStore->bufferedPointCount(0)));
        m_readBuffer.remove(0, static_cast<int>(frameBytes));
    }
    return true;
}

bool AcquisitionTcpReceiverService::setError(const QString& message) {
    m_statusMessage = message;
    emit errorOccurred(message);
    return false;
}

void AcquisitionTcpReceiverService::setState(bool enabled, bool connected, const QString& message) {
    m_statusMessage = message;
    emit networkStateChanged(enabled, connected, message);
}
