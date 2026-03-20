#include "core/dsa16chdeviceservice.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>

#include <algorithm>

#if defined(Q_CC_MSVC) && defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

QString diagnosticTraceFilePath() {
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (baseDir.trimmed().isEmpty()) {
        baseDir = QCoreApplication::applicationDirPath();
    }

    QDir dir(baseDir);
    dir.mkpath(".");
    return dir.filePath("acquisition_trace.log");
}

bool hotPathTraceEnabled() {
    static const bool enabled = qEnvironmentVariableIsSet("DSA_TRACE_HOTPATH");
    return enabled;
}

void appendDiagnosticTrace(const QString& message) {
    QFile file(diagnosticTraceFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }

    QTextStream stream(&file);
    stream << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz ")
           << "[Device] "
           << message
           << '\n';
}

void appendHotPathTrace(const QString& message) {
    if (!hotPathTraceEnabled()) {
        return;
    }
    appendDiagnosticTrace(message);
}

QString formatExceptionCode(unsigned long code) {
    return QString("0x%1").arg(code, 8, 16, QChar('0')).toUpper();
}

#if defined(Q_CC_MSVC) && defined(Q_OS_WIN)
int callStartWithSeh(Dsa16ChDllLoader::FnDsa16ChStart fn, unsigned long* exceptionCode) {
    *exceptionCode = 0;
    __try {
        return fn();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *exceptionCode = GetExceptionCode();
    }
    return -1;
}

int callQueryBufferWithSeh(Dsa16ChDllLoader::FnDsa16ChPointNumPerChInBufQuery fn,
                           unsigned int* value,
                           unsigned long* exceptionCode) {
    *exceptionCode = 0;
    __try {
        return fn(value);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *exceptionCode = GetExceptionCode();
    }
    return -1;
}

int callReadDataWithSeh(Dsa16ChDllLoader::FnDsa16ChReadData fn,
                        unsigned int pointNum2Read,
                        double* ch0,
                        double* ch1,
                        double* ch2,
                        double* ch3,
                        double* ch4,
                        double* ch5,
                        double* ch6,
                        double* ch7,
                        double* ch8,
                        double* ch9,
                        double* ch10,
                        double* ch11,
                        double* ch12,
                        double* ch13,
                        double* ch14,
                        double* ch15,
                        unsigned long* exceptionCode) {
    *exceptionCode = 0;
    __try {
        return fn(pointNum2Read,
                  ch0,
                  ch1,
                  ch2,
                  ch3,
                  ch4,
                  ch5,
                  ch6,
                  ch7,
                  ch8,
                  ch9,
                  ch10,
                  ch11,
                  ch12,
                  ch13,
                  ch14,
                  ch15);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *exceptionCode = GetExceptionCode();
    }
    return -1;
}

int callQueryOverflowWithSeh(Dsa16ChDllLoader::FnDsa16ChBufOverflowQuery fn,
                             unsigned int* value,
                             unsigned long* exceptionCode) {
    *exceptionCode = 0;
    __try {
        return fn(value);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *exceptionCode = GetExceptionCode();
    }
    return -1;
}
#endif

}  // namespace

Dsa16ChDeviceService::Dsa16ChDeviceService(Dsa16ChDllLoader* loader, QObject* parent)
    : QObject(parent), m_loader(loader) {}

Dsa16ChDeviceService::~Dsa16ChDeviceService() {
    shutdown();
}

bool Dsa16ChDeviceService::initializeSdk(const QString& preferredPath) {
    if (m_loader == nullptr) {
        return setError("SDK loader is null.");
    }

    const bool ok = m_loader->load(preferredPath);
    if (!ok) {
        emit sdkStateChanged(false, m_loader->lastError());
        return setError(m_loader->lastError());
    }

    emit sdkStateChanged(true, QString("SDK loaded: %1").arg(m_loader->loadedPath()));
    return true;
}

bool Dsa16ChDeviceService::sdkReady() const {
    return m_loader != nullptr && m_loader->isLoaded();
}

QString Dsa16ChDeviceService::sdkStatusMessage() const {
    if (!sdkReady()) {
        return m_loader ? m_loader->lastError() : QStringLiteral("SDK loader is null.");
    }
    return QString("SDK loaded: %1").arg(m_loader->loadedPath());
}

bool Dsa16ChDeviceService::openDevice() {
    if (!ensureSdkReady("Open device")) {
        return false;
    }
    if (m_deviceOpen) {
        return true;
    }
    if (m_loader->api().dsa_16ch_open == nullptr) {
        return setError("Open device failed: dsa_16ch_open is unresolved.");
    }

    const int rc = m_loader->api().dsa_16ch_open();
    if (rc != 0) {
        return setError(QString("Open device failed: dsa_16ch_open returned %1.").arg(rc));
    }

    m_deviceOpen = true;
    emit deviceOpenStateChanged(true);
    return true;
}

void Dsa16ChDeviceService::closeDevice() {
    if (!sdkReady() || !m_deviceOpen) {
        return;
    }

    if (m_loader->api().dsa_16ch_close != nullptr) {
        m_loader->api().dsa_16ch_close();
    }

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
    if (!ensureDeviceOpened("Set sample rate")) {
        return false;
    }
    if (m_loader->api().dsa_16ch_sample_rate_sel == nullptr) {
        return setError("Set sample rate failed: dsa_16ch_sample_rate_sel is unresolved.");
    }

    const int rc = m_loader->api().dsa_16ch_sample_rate_sel(sampleRateSel);
    if (rc != 0) {
        return setError(QString("Set sample rate failed: dsa_16ch_sample_rate_sel(%1) returned %2.")
                            .arg(sampleRateSel)
                            .arg(rc));
    }

    m_settings.sampleRateSel = sampleRateSel;
    return true;
}

bool Dsa16ChDeviceService::setSampleMode(unsigned int sampleMode) {
    if (!ensureDeviceOpened("Set sample mode")) {
        return false;
    }
    if (m_loader->api().dsa_16ch_sample_mode_set == nullptr) {
        return setError("Set sample mode failed: dsa_16ch_sample_mode_set is unresolved.");
    }

    const int rc = m_loader->api().dsa_16ch_sample_mode_set(sampleMode);
    if (rc != 0) {
        return setError(QString("Set sample mode failed: dsa_16ch_sample_mode_set(%1) returned %2.")
                            .arg(sampleMode)
                            .arg(rc));
    }

    m_settings.sampleMode = sampleMode;
    return true;
}

bool Dsa16ChDeviceService::setTriggerSource(unsigned int trigSrc) {
    if (!ensureDeviceOpened("Set trigger source")) {
        return false;
    }
    if (m_loader->api().dsa_16ch_trig_src_sel == nullptr) {
        return setError("Set trigger source failed: dsa_16ch_trig_src_sel is unresolved.");
    }

    const int rc = m_loader->api().dsa_16ch_trig_src_sel(trigSrc);
    if (rc != 0) {
        return setError(QString("Set trigger source failed: dsa_16ch_trig_src_sel(%1) returned %2.")
                            .arg(trigSrc)
                            .arg(rc));
    }

    m_settings.triggerSource = trigSrc;
    return true;
}

bool Dsa16ChDeviceService::setExternalTriggerEdge(unsigned int edge) {
    if (!ensureDeviceOpened("Set external trigger edge")) {
        return false;
    }
    if (m_loader->api().dsa_16ch_ext_trig_edge_sel == nullptr) {
        return setError("Set external trigger edge failed: dsa_16ch_ext_trig_edge_sel is unresolved.");
    }

    const int rc = m_loader->api().dsa_16ch_ext_trig_edge_sel(edge);
    if (rc != 0) {
        return setError(QString("Set external trigger edge failed: dsa_16ch_ext_trig_edge_sel(%1) returned %2.")
                            .arg(edge)
                            .arg(rc));
    }

    m_settings.externalTriggerEdge = edge;
    return true;
}

bool Dsa16ChDeviceService::setClockBase(unsigned int clockBase) {
    if (!ensureDeviceOpened("Set clock base")) {
        return false;
    }
    if (m_loader->api().dsa_16ch_clock_base_sel == nullptr) {
        return setError("Set clock base failed: dsa_16ch_clock_base_sel is unresolved.");
    }

    const int rc = m_loader->api().dsa_16ch_clock_base_sel(clockBase);
    if (rc != 0) {
        return setError(QString("Set clock base failed: dsa_16ch_clock_base_sel(%1) returned %2.")
                            .arg(clockBase)
                            .arg(rc));
    }

    m_settings.clockBase = clockBase;
    return true;
}

bool Dsa16ChDeviceService::setFixedPointCount(unsigned int pointCountPerCh) {
    if (!ensureDeviceOpened("Set fixed point count")) {
        return false;
    }
    if (pointCountPerCh == 0 || pointCountPerCh > dsa::kMaxPointPerChannel ||
        pointCountPerCh % dsa::kPointAlign != 0) {
        return setError("Invalid fixed point count: it must be a non-zero multiple of 16 and <= 32768.");
    }
    if (m_loader->api().dsa_16ch_fix_point_mode_point_num_per_ch_set == nullptr) {
        return setError("Set fixed point count failed: dsa_16ch_fix_point_mode_point_num_per_ch_set is unresolved.");
    }

    const int rc = m_loader->api().dsa_16ch_fix_point_mode_point_num_per_ch_set(pointCountPerCh);
    if (rc != 0) {
        return setError(QString("Set fixed point count failed: "
                                "dsa_16ch_fix_point_mode_point_num_per_ch_set(%1) returned %2.")
                            .arg(pointCountPerCh)
                            .arg(rc));
    }

    m_settings.fixedPointCountPerCh = pointCountPerCh;
    return true;
}

bool Dsa16ChDeviceService::startAcquisition() {
    if (!ensureDeviceOpened("Start acquisition")) {
        return false;
    }
    if (m_loader->api().dsa_16ch_start == nullptr) {
        return setError("Start acquisition failed: dsa_16ch_start is unresolved.");
    }

    appendDiagnosticTrace("startAcquisition begin");

    int rc = -1;
#if defined(Q_CC_MSVC) && defined(Q_OS_WIN)
    unsigned long code = 0;
    rc = callStartWithSeh(m_loader->api().dsa_16ch_start, &code);
    if (code != 0) {
        appendDiagnosticTrace(QString("startAcquisition seh=%1").arg(formatExceptionCode(code)));
        return setError(QString("Start acquisition hit an SEH exception: %1.").arg(formatExceptionCode(code)));
    }
#else
    rc = m_loader->api().dsa_16ch_start();
#endif

    appendDiagnosticTrace(QString("startAcquisition rc=%1").arg(rc));
    if (rc != 0) {
        return setError(QString("Start acquisition failed: dsa_16ch_start returned %1.").arg(rc));
    }

    return true;
}

bool Dsa16ChDeviceService::stopAcquisition() {
    if (!ensureSdkReady("Stop acquisition")) {
        return false;
    }
    if (!m_deviceOpen) {
        return true;
    }
    if (m_loader->api().dsa_16ch_stop == nullptr) {
        return setError("Stop acquisition failed: dsa_16ch_stop is unresolved.");
    }

    const int rc = m_loader->api().dsa_16ch_stop();
    if (rc != 0) {
        return setError(QString("Stop acquisition failed: dsa_16ch_stop returned %1.").arg(rc));
    }

    return true;
}

bool Dsa16ChDeviceService::queryBufferPointCount(unsigned int& pointCountPerCh) {
    if (!ensureDeviceOpened("Query buffer point count")) {
        return false;
    }
    if (m_loader->api().dsa_16ch_point_num_per_ch_in_buf_query == nullptr) {
        return setError("Query buffer point count failed: dsa_16ch_point_num_per_ch_in_buf_query is unresolved.");
    }

    unsigned int value = 0;
    appendHotPathTrace("queryBufferPointCount begin");

    int rc = -1;
#if defined(Q_CC_MSVC) && defined(Q_OS_WIN)
    unsigned long code = 0;
    rc = callQueryBufferWithSeh(m_loader->api().dsa_16ch_point_num_per_ch_in_buf_query, &value, &code);
    if (code != 0) {
        appendHotPathTrace(QString("queryBufferPointCount seh=%1").arg(formatExceptionCode(code)));
        return setError(QString("Query buffer point count hit an SEH exception: %1.").arg(formatExceptionCode(code)));
    }
#else
    rc = m_loader->api().dsa_16ch_point_num_per_ch_in_buf_query(&value);
#endif

    appendHotPathTrace(QString("queryBufferPointCount rc=%1 value=%2").arg(rc).arg(value));
    if (rc != 0) {
        return setError(QString("Query buffer point count failed: "
                                "dsa_16ch_point_num_per_ch_in_buf_query returned %1.")
                            .arg(rc));
    }

    pointCountPerCh = std::min(value, dsa::kMaxPointPerChannel);
    return true;
}

bool Dsa16ChDeviceService::readData(unsigned int pointNum2Read,
                                    std::array<QVector<double>, dsa::kChannelCount>& outSamples) {
    if (!ensureDeviceOpened("Read acquisition data")) {
        return false;
    }
    if (pointNum2Read == 0 || pointNum2Read % dsa::kPointAlign != 0) {
        return setError("Invalid read size: point_num2read must be a non-zero multiple of 16.");
    }
    if (m_loader->api().dsa_16ch_read_data == nullptr) {
        return setError("Read acquisition data failed: dsa_16ch_read_data is unresolved.");
    }

    for (QVector<double>& channelBuffer : outSamples) {
        channelBuffer.resize(static_cast<int>(pointNum2Read));
    }

    appendHotPathTrace(QString("readData begin points=%1").arg(pointNum2Read));

    int rc = -1;
#if defined(Q_CC_MSVC) && defined(Q_OS_WIN)
    unsigned long code = 0;
    rc = callReadDataWithSeh(m_loader->api().dsa_16ch_read_data,
                             pointNum2Read,
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
                             outSamples[15].data(),
                             &code);
    if (code != 0) {
        appendHotPathTrace(QString("readData seh=%1 points=%2").arg(formatExceptionCode(code)).arg(pointNum2Read));
        return setError(QString("Read acquisition data hit an SEH exception: %1, point_num2read=%2.")
                            .arg(formatExceptionCode(code))
                            .arg(pointNum2Read));
    }
#else
    rc = m_loader->api().dsa_16ch_read_data(pointNum2Read,
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
#endif

    appendHotPathTrace(QString("readData rc=%1 points=%2").arg(rc).arg(pointNum2Read));
    if (rc != 0) {
        return setError(QString("Read acquisition data failed: dsa_16ch_read_data(%1) returned %2.")
                            .arg(pointNum2Read)
                            .arg(rc));
    }

    return true;
}

bool Dsa16ChDeviceService::queryOverflow(bool& overflow) {
    if (!ensureDeviceOpened("Query overflow state")) {
        return false;
    }
    if (m_loader->api().dsa_16ch_buf_overflow_query == nullptr) {
        return setError("Query overflow state failed: dsa_16ch_buf_overflow_query is unresolved.");
    }

    unsigned int value = 0;
    appendHotPathTrace("queryOverflow begin");

    int rc = -1;
#if defined(Q_CC_MSVC) && defined(Q_OS_WIN)
    unsigned long code = 0;
    rc = callQueryOverflowWithSeh(m_loader->api().dsa_16ch_buf_overflow_query, &value, &code);
    if (code != 0) {
        appendHotPathTrace(QString("queryOverflow seh=%1").arg(formatExceptionCode(code)));
        return setError(QString("Query overflow state hit an SEH exception: %1.").arg(formatExceptionCode(code)));
    }
#else
    rc = m_loader->api().dsa_16ch_buf_overflow_query(&value);
#endif

    appendHotPathTrace(QString("queryOverflow rc=%1 value=%2").arg(rc).arg(value));
    if (rc != 0) {
        return setError(QString("Query overflow state failed: dsa_16ch_buf_overflow_query returned %1.").arg(rc));
    }

    overflow = (value != 0);
    return true;
}

bool Dsa16ChDeviceService::setDioDirection(unsigned int groupIndex, unsigned int direction) {
    if (!ensureDeviceOpened("Set DIO direction")) {
        return false;
    }
    if (groupIndex > 1 || direction > 1) {
        return setError("Invalid DIO parameters: group_index must be 0/1 and direction must be 0/1.");
    }
    if (m_loader->api().dsa_16ch_dio_dir_set == nullptr) {
        return setError("Set DIO direction failed: dsa_16ch_dio_dir_set is unresolved.");
    }

    const int rc = m_loader->api().dsa_16ch_dio_dir_set(groupIndex, direction);
    if (rc != 0) {
        return setError(QString("Set DIO direction failed: dsa_16ch_dio_dir_set(%1, %2) returned %3.")
                            .arg(groupIndex)
                            .arg(direction)
                            .arg(rc));
    }

    m_dioSettings.groupIndex = groupIndex;
    m_dioSettings.direction = direction;
    return true;
}

bool Dsa16ChDeviceService::writeDo(unsigned int groupIndex, unsigned char doData) {
    if (!ensureDeviceOpened("Write DO data")) {
        return false;
    }
    if (groupIndex > 1) {
        return setError("Invalid DIO group index: group_index must be 0/1.");
    }
    if (m_loader->api().dsa_16ch_wr_do_data == nullptr) {
        return setError("Write DO data failed: dsa_16ch_wr_do_data is unresolved.");
    }

    const int rc = m_loader->api().dsa_16ch_wr_do_data(groupIndex, doData);
    if (rc != 0) {
        return setError(QString("Write DO data failed: dsa_16ch_wr_do_data(%1, 0x%2) returned %3.")
                            .arg(groupIndex)
                            .arg(QString::number(doData, 16).rightJustified(2, '0'))
                            .arg(rc));
    }

    m_dioSettings.groupIndex = groupIndex;
    m_dioSettings.doData = doData;
    return true;
}

bool Dsa16ChDeviceService::readDi(unsigned int groupIndex, unsigned char& diData) {
    if (!ensureDeviceOpened("Read DI data")) {
        return false;
    }
    if (groupIndex > 1) {
        return setError("Invalid DIO group index: group_index must be 0/1.");
    }
    if (m_loader->api().dsa_16ch_rd_di_data == nullptr) {
        return setError("Read DI data failed: dsa_16ch_rd_di_data is unresolved.");
    }

    unsigned char value = 0;
    const int rc = m_loader->api().dsa_16ch_rd_di_data(groupIndex, &value);
    if (rc != 0) {
        return setError(QString("Read DI data failed: dsa_16ch_rd_di_data(%1) returned %2.")
                            .arg(groupIndex)
                            .arg(rc));
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
    if (m_loader != nullptr) {
        m_loader->unload();
    }
}

bool Dsa16ChDeviceService::ensureSdkReady(const QString& operation) {
    if (sdkReady()) {
        return true;
    }
    return setError(QString("%1 failed: SDK is not ready. %2").arg(operation, m_loader ? m_loader->lastError() : QString()));
}

bool Dsa16ChDeviceService::ensureDeviceOpened(const QString& operation) {
    if (!ensureSdkReady(operation)) {
        return false;
    }
    if (m_deviceOpen) {
        return true;
    }
    return setError(QString("%1 failed: device is not open.").arg(operation));
}

bool Dsa16ChDeviceService::setError(const QString& message) {
    m_lastError = message;
    appendDiagnosticTrace(QString("error=%1").arg(message));
    return false;
}
