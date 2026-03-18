#pragma once

#include <QJsonObject>
#include <QString>

namespace dsa {

constexpr int kChannelCount = 16;
constexpr unsigned int kPointAlign = 16;
constexpr unsigned int kMaxPointPerChannel = 32768;

enum class SampleRateSel : unsigned int {
    Rate204_8K = 0,
    Rate102_4K = 1,
    Rate51_2K = 2,
    Rate25_6K = 3,
    Rate12_8K = 4,
    Rate6_4K = 5
};

enum class SampleMode : unsigned int {
    SingleFixedPoint = 0,
    Continuous = 1,
    ContinuousFixedPoint = 2
};

enum class TriggerSource : unsigned int {
    Software = 0,
    ExternalSignal = 1
};

enum class ExternalTriggerEdge : unsigned int {
    Falling = 0,
    Rising = 1
};

enum class ClockBase : unsigned int {
    Onboard = 0,
    External = 1
};

enum class DioDirection : unsigned int {
    Input = 0,
    Output = 1
};

struct DsaAcquisitionSettings {
    unsigned int sampleRateSel = static_cast<unsigned int>(SampleRateSel::Rate25_6K);
    unsigned int sampleMode = static_cast<unsigned int>(SampleMode::Continuous);
    unsigned int triggerSource = static_cast<unsigned int>(TriggerSource::Software);
    unsigned int externalTriggerEdge = static_cast<unsigned int>(ExternalTriggerEdge::Rising);
    unsigned int clockBase = static_cast<unsigned int>(ClockBase::Onboard);
    unsigned int fixedPointCountPerCh = 4096;

    bool validate(QString* error) const;
    QJsonObject toJson() const;
    static DsaAcquisitionSettings fromJson(const QJsonObject& json);
};

struct DsaDioSettings {
    unsigned int groupIndex = 0;
    unsigned int direction = static_cast<unsigned int>(DioDirection::Input);
    unsigned char doData = 0x00;
    unsigned char diData = 0x00;

    QJsonObject toJson() const;
    static DsaDioSettings fromJson(const QJsonObject& json);
};

QString sampleRateToText(unsigned int value);
QString sampleModeToText(unsigned int value);
QString triggerSourceToText(unsigned int value);
QString externalTriggerEdgeToText(unsigned int value);
QString clockBaseToText(unsigned int value);

}  // namespace dsa
