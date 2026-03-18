#include "core/dsa16chdeviceservice.h"

#include <algorithm>

Dsa16ChDeviceService::Dsa16ChDeviceService(Dsa16ChDllLoader* loader, QObject* parent)
    : QObject(parent), m_loader(loader) {}

Dsa16ChDeviceService::~Dsa16ChDeviceService() {
    shutdown();
}

bool Dsa16ChDeviceService::initializeSdk(const QString& preferredPath) {
    if (m_loader == nullptr) {
        return setError("SDK Loader 未初始化。");
    }
    const bool ok = m_loader->load(preferredPath);
    if (!ok) {
        emit sdkStateChanged(false, m_loader->lastError());
        return setError(m_loader->lastError());
    }
    emit sdkStateChanged(true, QString("SDK 已就绪：%1").arg(m_loader->loadedPath()));
    return true;
}

bool Dsa16ChDeviceService::sdkReady() const {
    return m_loader != nullptr && m_loader->isLoaded();
}

QString Dsa16ChDeviceService::sdkStatusMessage() const {
    if (!sdkReady()) {
        return m_loader ? m_loader->lastError() : "SDK Loader 未创建。";
    }
    return QString("SDK 已加载：%1").arg(m_loader->loadedPath());
}

bool Dsa16ChDeviceService::openDevice() {
    if (!ensureSdkReady("打开设备")) {
        return false;
    }
    if (m_deviceOpen) {
        return true;
    }
    const int rc = m_loader->api().dsa_16ch_open();
    if (rc != 0) {
        return setError(QString("设备打开失败：dsa_16ch_open 返回 %1（成功应为 0）").arg(rc));
    }
    m_deviceOpen = true;
    emit deviceOpenStateChanged(true);
    return true;
}

void Dsa16ChDeviceService::closeDevice() {
    if (!sdkReady() || !m_deviceOpen) {
        return;
    }
    m_loader->api().dsa_16ch_close();
    m_deviceOpen = false;
    emit deviceOpenStateChanged(false);
}

bool Dsa16ChDeviceService::isDeviceOpen() const {
    return m_deviceOpen;
}

bool Dsa16ChDeviceService::setAcquisitionSettings(const dsa::DsaAcquisitionSettings& settings) {
    QString error;
    if (!settings.validate(&error)) {
        return setError(error);
    }

    if (!setSampleRate(settings.sampleRateSel) || !setSampleMode(settings.sampleMode) ||
        !setTriggerSource(settings.triggerSource) || !setExternalTriggerEdge(settings.externalTriggerEdge) ||
        !setClockBase(settings.clockBase)) {
        return false;
    }

    const bool fixedPointMode = settings.sampleMode == static_cast<unsigned int>(dsa::SampleMode::SingleFixedPoint) ||
                                settings.sampleMode == static_cast<unsigned int>(dsa::SampleMode::ContinuousFixedPoint);
    if (fixedPointMode && !setFixedPointCount(settings.fixedPointCountPerCh)) {
        return false;
    }

    m_settings = settings;
    return true;
}

dsa::DsaAcquisitionSettings Dsa16ChDeviceService::currentSettings() const {
    return m_settings;
}

dsa::DsaDioSettings Dsa16ChDeviceService::currentDioSettings() const {
    return m_dioSettings;
}

bool Dsa16ChDeviceService::setSampleRate(unsigned int sampleRateSel) {
    if (!ensureDeviceOpened("设置采样率")) {
        return false;
    }
    const int rc = m_loader->api().dsa_16ch_sample_rate_sel(sampleRateSel);
    if (rc != 0) {
        return setError(QString("设置采样率失败：dsa_16ch_sample_rate_sel(%1) 返回 %2").arg(sampleRateSel).arg(rc));
    }
    m_settings.sampleRateSel = sampleRateSel;
    return true;
}

bool Dsa16ChDeviceService::setSampleMode(unsigned int sampleMode) {
    if (!ensureDeviceOpened("设置采集模式")) {
        return false;
    }
    const int rc = m_loader->api().dsa_16ch_sample_mode_set(sampleMode);
    if (rc != 0) {
        return setError(QString("设置采集模式失败：dsa_16ch_sample_mode_set(%1) 返回 %2").arg(sampleMode).arg(rc));
    }
    m_settings.sampleMode = sampleMode;
    return true;
}

bool Dsa16ChDeviceService::setTriggerSource(unsigned int trigSrc) {
    if (!ensureDeviceOpened("设置触发源")) {
        return false;
    }
    const int rc = m_loader->api().dsa_16ch_trig_src_sel(trigSrc);
    if (rc != 0) {
        return setError(QString("设置触发源失败：dsa_16ch_trig_src_sel(%1) 返回 %2").arg(trigSrc).arg(rc));
    }
    m_settings.triggerSource = trigSrc;
    return true;
}

bool Dsa16ChDeviceService::setExternalTriggerEdge(unsigned int edge) {
    if (!ensureDeviceOpened("设置外部触发沿")) {
        return false;
    }
    const int rc = m_loader->api().dsa_16ch_ext_trig_edge_sel(edge);
    if (rc != 0) {
        return setError(QString("设置外部触发沿失败：dsa_16ch_ext_trig_edge_sel(%1) 返回 %2").arg(edge).arg(rc));
    }
    m_settings.externalTriggerEdge = edge;
    return true;
}

bool Dsa16ChDeviceService::setClockBase(unsigned int clockBase) {
    if (!ensureDeviceOpened("设置时钟基准")) {
        return false;
    }
    const int rc = m_loader->api().dsa_16ch_clock_base_sel(clockBase);
    if (rc != 0) {
        return setError(QString("设置时钟基准失败：dsa_16ch_clock_base_sel(%1) 返回 %2").arg(clockBase).arg(rc));
    }
    m_settings.clockBase = clockBase;
    return true;
}

bool Dsa16ChDeviceService::setFixedPointCount(unsigned int pointCountPerCh) {
    if (!ensureDeviceOpened("设置定点采样点数")) {
        return false;
    }
    if (pointCountPerCh == 0 || pointCountPerCh > dsa::kMaxPointPerChannel ||
        pointCountPerCh % dsa::kPointAlign != 0) {
        return setError("定点采样点数不合法：必须是 16 的整数倍且不超过 32768。");
    }
    const int rc = m_loader->api().dsa_16ch_fix_point_mode_point_num_per_ch_set(pointCountPerCh);
    if (rc != 0) {
        return setError(QString("设置定点采样点数失败：dsa_16ch_fix_point_mode_point_num_per_ch_set(%1) 返回 %2")
                            .arg(pointCountPerCh)
                            .arg(rc));
    }
    m_settings.fixedPointCountPerCh = pointCountPerCh;
    return true;
}

bool Dsa16ChDeviceService::startAcquisition() {
    if (!ensureDeviceOpened("开始采集")) {
        return false;
    }
    const int rc = m_loader->api().dsa_16ch_start();
    if (rc != 0) {
        return setError(QString("开始采集失败：dsa_16ch_start 返回 %1").arg(rc));
    }
    return true;
}

bool Dsa16ChDeviceService::stopAcquisition() {
    if (!ensureSdkReady("停止采集")) {
        return false;
    }
    if (!m_deviceOpen) {
        return true;
    }
    const int rc = m_loader->api().dsa_16ch_stop();
    if (rc != 0) {
        return setError(QString("停止采集失败：dsa_16ch_stop 返回 %1").arg(rc));
    }
    return true;
}

bool Dsa16ChDeviceService::queryBufferPointCount(unsigned int& pointCountPerCh) {
    if (!ensureDeviceOpened("查询缓冲点数")) {
        return false;
    }
    unsigned int value = 0;
    const int rc = m_loader->api().dsa_16ch_point_num_per_ch_in_buf_query(&value);
    if (rc != 0) {
        return setError(QString("查询缓冲点数失败：dsa_16ch_point_num_per_ch_in_buf_query 返回 %1").arg(rc));
    }
    pointCountPerCh = std::min(value, dsa::kMaxPointPerChannel);
    return true;
}

bool Dsa16ChDeviceService::readData(unsigned int pointNum2Read,
                                    std::array<QVector<double>, dsa::kChannelCount>& outSamples) {
    if (!ensureDeviceOpened("读取采样数据")) {
        return false;
    }
    if (pointNum2Read == 0 || pointNum2Read % dsa::kPointAlign != 0) {
        return setError("读取点数不合法：point_num2read 必须是 16 的整数倍。");
    }

    for (QVector<double>& channelBuffer : outSamples) {
        channelBuffer.resize(static_cast<int>(pointNum2Read));
    }

    const int rc = m_loader->api().dsa_16ch_read_data(pointNum2Read,
                                                       outSamples[0].data(),
                                                       outSamples[1].data(),
                                                       outSamples[2].data(),
                                                       outSamples[3].data(),
                                                       outSamples[4].data(),
                                                       outSamples[5].data(),
                                                       outSamples[6].data(),
                                                       outSamples[7].data(),
                                                       outSamples[8].data(),
                                                       outSamples[9].data(),
                                                       outSamples[10].data(),
                                                       outSamples[11].data(),
                                                       outSamples[12].data(),
                                                       outSamples[13].data(),
                                                       outSamples[14].data(),
                                                       outSamples[15].data());
    if (rc != 0) {
        return setError(QString("读取采样数据失败：dsa_16ch_read_data(%1) 返回 %2").arg(pointNum2Read).arg(rc));
    }
    return true;
}

bool Dsa16ChDeviceService::queryOverflow(bool& overflow) {
    if (!ensureDeviceOpened("查询溢出状态")) {
        return false;
    }
    unsigned int value = 0;
    const int rc = m_loader->api().dsa_16ch_buf_overflow_query(&value);
    if (rc != 0) {
        return setError(QString("查询溢出状态失败：dsa_16ch_buf_overflow_query 返回 %1").arg(rc));
    }
    overflow = (value != 0);
    return true;
}

bool Dsa16ChDeviceService::setDioDirection(unsigned int groupIndex, unsigned int direction) {
    if (!ensureDeviceOpened("设置 DIO 方向")) {
        return false;
    }
    if (groupIndex > 1 || direction > 1) {
        return setError("DIO 参数无效：group_index 仅支持 0/1，direction 仅支持 0(输入)/1(输出)。");
    }
    const int rc = m_loader->api().dsa_16ch_dio_dir_set(groupIndex, direction);
    if (rc != 0) {
        return setError(QString("设置 DIO 方向失败：dsa_16ch_dio_dir_set(%1, %2) 返回 %3")
                            .arg(groupIndex)
                            .arg(direction)
                            .arg(rc));
    }
    m_dioSettings.groupIndex = groupIndex;
    m_dioSettings.direction = direction;
    return true;
}

bool Dsa16ChDeviceService::writeDo(unsigned int groupIndex, unsigned char doData) {
    if (!ensureDeviceOpened("写 DO 数据")) {
        return false;
    }
    if (groupIndex > 1) {
        return setError("DIO 组号无效：group_index 仅支持 0/1。");
    }
    const int rc = m_loader->api().dsa_16ch_wr_do_data(groupIndex, doData);
    if (rc != 0) {
        return setError(QString("写 DO 数据失败：dsa_16ch_wr_do_data(%1, 0x%2) 返回 %3")
                            .arg(groupIndex)
                            .arg(QString::number(doData, 16).rightJustified(2, '0'))
                            .arg(rc));
    }
    m_dioSettings.groupIndex = groupIndex;
    m_dioSettings.doData = doData;
    return true;
}

bool Dsa16ChDeviceService::readDi(unsigned int groupIndex, unsigned char& diData) {
    if (!ensureDeviceOpened("读 DI 数据")) {
        return false;
    }
    if (groupIndex > 1) {
        return setError("DIO 组号无效：group_index 仅支持 0/1。");
    }
    unsigned char value = 0;
    const int rc = m_loader->api().dsa_16ch_rd_di_data(groupIndex, &value);
    if (rc != 0) {
        return setError(QString("读 DI 数据失败：dsa_16ch_rd_di_data(%1) 返回 %2").arg(groupIndex).arg(rc));
    }
    diData = value;
    m_dioSettings.groupIndex = groupIndex;
    m_dioSettings.diData = value;
    return true;
}

QString Dsa16ChDeviceService::lastError() const {
    return m_lastError;
}

void Dsa16ChDeviceService::resetCachedState() {
    m_lastError.clear();
    m_settings = dsa::DsaAcquisitionSettings{};
    m_dioSettings = dsa::DsaDioSettings{};
}

void Dsa16ChDeviceService::shutdown() {
    closeDevice();
    if (m_loader) {
        m_loader->unload();
    }
}

bool Dsa16ChDeviceService::ensureSdkReady(const QString& operation) {
    if (sdkReady()) {
        return true;
    }
    return setError(QString("%1失败：SDK 未就绪。%2").arg(operation, m_loader ? m_loader->lastError() : QString()));
}

bool Dsa16ChDeviceService::ensureDeviceOpened(const QString& operation) {
    if (!ensureSdkReady(operation)) {
        return false;
    }
    if (m_deviceOpen) {
        return true;
    }
    return setError(QString("%1失败：设备未打开，请先执行 dsa_16ch_open。").arg(operation));
}

bool Dsa16ChDeviceService::setError(const QString& message) {
    m_lastError = message;
    return false;
}
