#include "core/acquisitiontcpstreamservice.h"

#include <QAbstractSocket>
#include <QDateTime>
#include <QTcpSocket>

namespace {
constexpr quint32 kNetworkMagic = 0x4E415344;  // "DSAN"
constexpr quint32 kNetworkVersion = 1;
constexpr qint64 kReconnectIntervalMs = 1000;
constexpr qint64 kMaxPendingBytes = 8LL * 1024LL * 1024LL;
}  // namespace

AcquisitionTcpStreamService::AcquisitionTcpStreamService(QObject* parent)
    : QObject(parent), m_socket(new QTcpSocket(this)) {
    connect(m_socket, &QTcpSocket::connected, this, &AcquisitionTcpStreamService::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &AcquisitionTcpStreamService::onSocketDisconnected);
    connect(m_socket, &QAbstractSocket::errorOccurred, this, &AcquisitionTcpStreamService::onSocketErrorOccurred);
    setState(false, false, QStringLiteral("网络传输未启用"));
}

void AcquisitionTcpStreamService::setNetworkSettings(const dsa::DsaNetworkSettings& settings) {
    const bool endpointChanged =
        m_settings.remoteHost != settings.remoteHost || m_settings.remotePort != settings.remotePort;
    const bool enabledChanged = m_settings.enabled != settings.enabled;

    m_settings = settings;

    if (!m_settings.enabled) {
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->abort();
        }
        setState(false, false, QStringLiteral("网络传输未启用"));
        return;
    }

    if (enabledChanged || endpointChanged) {
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->abort();
        }
        setState(true, false, QStringLiteral("已更新"));
    }
}

dsa::DsaNetworkSettings AcquisitionTcpStreamService::networkSettings() const {
    return m_settings;
}

bool AcquisitionTcpStreamService::startStreaming(bool force) {
    if (!m_settings.enabled) {
        setState(false, false, QStringLiteral("网络传输未启用"));
        return false;
    }
    return ensureConnectionStarted(force);
}

void AcquisitionTcpStreamService::stopStreaming() {
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
    setState(m_settings.enabled, false, m_settings.enabled ? QStringLiteral("网络已断开")
                                                           : QStringLiteral("网络传输未启用"));
}

bool AcquisitionTcpStreamService::sendFrame(const AcquisitionFrame& frame,
                                            const dsa::DsaAcquisitionSettings& acquisitionSettings) {
    if (!m_settings.enabled) {
        return true;
    }

    if (!isConnected()) {
        ensureConnectionStarted();
        return false;
    }

    if (m_socket->bytesToWrite() >= kMaxPendingBytes) {
        m_socket->abort();
        setState(true, false, QStringLiteral("网络发送积压过大，连接已重置"));
        return false;
    }

    const QByteArray payload = serializeFrame(frame, acquisitionSettings);
    const qint64 written = m_socket->write(payload);
    if (written != payload.size()) {
        const QString message = QStringLiteral("网络发送失败：%1").arg(m_socket->errorString());
        m_socket->abort();
        setState(true, false, message);
        return false;
    }

    if (!m_statusMessage.startsWith(QStringLiteral("已连接"))) {
        setState(true,
                 true,
                 QStringLiteral("已连接到 %1:%2").arg(m_settings.remoteHost).arg(m_settings.remotePort));
    }
    return true;
}

bool AcquisitionTcpStreamService::isEnabled() const {
    return m_settings.enabled;
}

bool AcquisitionTcpStreamService::isConnected() const {
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

QString AcquisitionTcpStreamService::statusMessage() const {
    return m_statusMessage;
}

void AcquisitionTcpStreamService::onSocketConnected() {
    setState(true,
             true,
             QStringLiteral("已连接到 %1:%2").arg(m_settings.remoteHost).arg(m_settings.remotePort));
}

void AcquisitionTcpStreamService::onSocketDisconnected() {
    if (!m_settings.enabled) {
        setState(false, false, QStringLiteral("网络传输未启用"));
        return;
    }
    setState(true, false, QStringLiteral("网络已断开，等待重连"));
}

void AcquisitionTcpStreamService::onSocketErrorOccurred() {
    if (!m_settings.enabled) {
        return;
    }
    setState(true, false, QStringLiteral("网络错误：%1").arg(m_socket->errorString()));
}

bool AcquisitionTcpStreamService::ensureConnectionStarted(bool force) {
    QString validationError;
    if (!m_settings.validate(&validationError)) {
        setState(true, false, validationError);
        return false;
    }

    if (isConnected()) {
        return true;
    }

    if (!force &&
        (m_socket->state() == QAbstractSocket::ConnectingState ||
         m_socket->state() == QAbstractSocket::HostLookupState)) {
        return true;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (!force && m_lastConnectAttemptMs > 0 && (nowMs - m_lastConnectAttemptMs) < kReconnectIntervalMs) {
        return false;
    }
    m_lastConnectAttemptMs = nowMs;

    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }

    setState(true,
             false,
             QStringLiteral("正在连接 %1:%2 ...").arg(m_settings.remoteHost).arg(m_settings.remotePort));
    m_socket->connectToHost(m_settings.remoteHost, static_cast<quint16>(m_settings.remotePort));
    return true;
}

QByteArray AcquisitionTcpStreamService::serializeFrame(
    const AcquisitionFrame& frame,
    const dsa::DsaAcquisitionSettings& acquisitionSettings) const {
    QByteArray payload;

    auto appendPod = [&payload](const auto& value) {
        payload.append(reinterpret_cast<const char*>(&value), static_cast<int>(sizeof(value)));
    };

    const quint32 magic = kNetworkMagic;
    const quint32 version = kNetworkVersion;
    const quint32 channelCount = dsa::kChannelCount;
    const quint32 pointCount = frame.pointCountPerChannel;
    const quint64 timestampMs = static_cast<quint64>(frame.timestampMs);
    const quint32 sampleRateSel = acquisitionSettings.sampleRateSel;
    const quint32 sampleMode = acquisitionSettings.sampleMode;
    const quint32 triggerSource = acquisitionSettings.triggerSource;
    const quint32 externalTriggerEdge = acquisitionSettings.externalTriggerEdge;
    const quint32 clockBase = acquisitionSettings.clockBase;

    payload.reserve(static_cast<int>(sizeof(magic) * 8 + sizeof(timestampMs) +
                                     static_cast<qint64>(pointCount) * dsa::kChannelCount *
                                         static_cast<qint64>(sizeof(double))));

    appendPod(magic);
    appendPod(version);
    appendPod(channelCount);
    appendPod(pointCount);
    appendPod(timestampMs);
    appendPod(sampleRateSel);
    appendPod(sampleMode);
    appendPod(triggerSource);
    appendPod(externalTriggerEdge);
    appendPod(clockBase);

    for (int channel = 0; channel < dsa::kChannelCount; ++channel) {
        const QVector<double>& channelData = frame.channels[channel];
        if (channelData.isEmpty()) {
            continue;
        }
        const char* raw = reinterpret_cast<const char*>(channelData.constData());
        const int bytes = static_cast<int>(channelData.size() * static_cast<int>(sizeof(double)));
        payload.append(raw, bytes);
    }

    return payload;
}

void AcquisitionTcpStreamService::setState(bool enabled, bool connected, const QString& message) {
    if (m_statusMessage == message && enabled == m_settings.enabled && connected == isConnected()) {
        return;
    }
    m_statusMessage = message;
    emit stateChanged(enabled, connected, message);
}
