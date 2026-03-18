#include "core/dsa16chtypes.h"

namespace dsa {

bool DsaAcquisitionSettings::validate(QString* error) const {
    if (sampleRateSel > static_cast<unsigned int>(SampleRateSel::Rate6_4K)) {
        if (error) {
            *error = "采样率枚举值无效，应为 0~5。";
        }
        return false;
    }
    if (sampleMode > static_cast<unsigned int>(SampleMode::ContinuousFixedPoint)) {
        if (error) {
            *error = "采集模式枚举值无效，应为 0~2。";
        }
        return false;
    }
    if (triggerSource > static_cast<unsigned int>(TriggerSource::ExternalSignal)) {
        if (error) {
            *error = "触发源枚举值无效，应为 0~1。";
        }
        return false;
    }
    if (externalTriggerEdge > static_cast<unsigned int>(ExternalTriggerEdge::Rising)) {
        if (error) {
            *error = "外部触发沿枚举值无效，应为 0~1。";
        }
        return false;
    }
    if (clockBase > static_cast<unsigned int>(ClockBase::External)) {
        if (error) {
            *error = "时钟基准枚举值无效，应为 0~1。";
        }
        return false;
    }

    const bool fixedPointMode =
        sampleMode == static_cast<unsigned int>(SampleMode::SingleFixedPoint) ||
        sampleMode == static_cast<unsigned int>(SampleMode::ContinuousFixedPoint);
    if (fixedPointMode) {
        if (fixedPointCountPerCh == 0 || fixedPointCountPerCh > kMaxPointPerChannel ||
            fixedPointCountPerCh % kPointAlign != 0) {
            if (error) {
                *error = "定点模式每通道点数必须是 16 的整数倍，范围 16~32768。";
            }
            return false;
        }
    }

    return true;
}

QJsonObject DsaAcquisitionSettings::toJson() const {
    QJsonObject json;
    json["sampleRateSel"] = static_cast<int>(sampleRateSel);
    json["sampleMode"] = static_cast<int>(sampleMode);
    json["triggerSource"] = static_cast<int>(triggerSource);
    json["externalTriggerEdge"] = static_cast<int>(externalTriggerEdge);
    json["clockBase"] = static_cast<int>(clockBase);
    json["fixedPointCountPerCh"] = static_cast<int>(fixedPointCountPerCh);
    return json;
}

DsaAcquisitionSettings DsaAcquisitionSettings::fromJson(const QJsonObject& json) {
    DsaAcquisitionSettings settings;
    settings.sampleRateSel =
        static_cast<unsigned int>(json.value("sampleRateSel").toInt(static_cast<int>(SampleRateSel::Rate25_6K)));
    settings.sampleMode =
        static_cast<unsigned int>(json.value("sampleMode").toInt(static_cast<int>(SampleMode::Continuous)));
    settings.triggerSource =
        static_cast<unsigned int>(json.value("triggerSource").toInt(static_cast<int>(TriggerSource::Software)));
    settings.externalTriggerEdge = static_cast<unsigned int>(
        json.value("externalTriggerEdge").toInt(static_cast<int>(ExternalTriggerEdge::Rising)));
    settings.clockBase =
        static_cast<unsigned int>(json.value("clockBase").toInt(static_cast<int>(ClockBase::Onboard)));
    settings.fixedPointCountPerCh = static_cast<unsigned int>(json.value("fixedPointCountPerCh").toInt(4096));
    return settings;
}

QJsonObject DsaDioSettings::toJson() const {
    QJsonObject json;
    json["groupIndex"] = static_cast<int>(groupIndex);
    json["direction"] = static_cast<int>(direction);
    json["doData"] = static_cast<int>(doData);
    json["diData"] = static_cast<int>(diData);
    return json;
}

DsaDioSettings DsaDioSettings::fromJson(const QJsonObject& json) {
    DsaDioSettings settings;
    settings.groupIndex = static_cast<unsigned int>(json.value("groupIndex").toInt(0));
    settings.direction = static_cast<unsigned int>(json.value("direction").toInt(0));
    settings.doData = static_cast<unsigned char>(json.value("doData").toInt(0));
    settings.diData = static_cast<unsigned char>(json.value("diData").toInt(0));
    return settings;
}

QString sampleRateToText(unsigned int value) {
    switch (static_cast<SampleRateSel>(value)) {
        case SampleRateSel::Rate204_8K:
            return "0 - 204.8KSps";
        case SampleRateSel::Rate102_4K:
            return "1 - 102.4KSps";
        case SampleRateSel::Rate51_2K:
            return "2 - 51.2KSps";
        case SampleRateSel::Rate25_6K:
            return "3 - 25.6KSps (默认)";
        case SampleRateSel::Rate12_8K:
            return "4 - 12.8KSps";
        case SampleRateSel::Rate6_4K:
            return "5 - 6.4KSps";
    }
    return "未知采样率";
}

QString sampleModeToText(unsigned int value) {
    switch (static_cast<SampleMode>(value)) {
        case SampleMode::SingleFixedPoint:
            return "0 - 单次定点采集模式";
        case SampleMode::Continuous:
            return "1 - 连续采集模式";
        case SampleMode::ContinuousFixedPoint:
            return "2 - 连续定点采集模式";
    }
    return "未知采集模式";
}

QString triggerSourceToText(unsigned int value) {
    switch (static_cast<TriggerSource>(value)) {
        case TriggerSource::Software:
            return "0 - 软件触发";
        case TriggerSource::ExternalSignal:
            return "1 - 外部信号触发";
    }
    return "未知触发源";
}

QString externalTriggerEdgeToText(unsigned int value) {
    switch (static_cast<ExternalTriggerEdge>(value)) {
        case ExternalTriggerEdge::Falling:
            return "0 - 下降沿";
        case ExternalTriggerEdge::Rising:
            return "1 - 上升沿";
    }
    return "未知触发沿";
}

QString clockBaseToText(unsigned int value) {
    switch (static_cast<ClockBase>(value)) {
        case ClockBase::Onboard:
            return "0 - 板载时钟";
        case ClockBase::External:
            return "1 - 外部时钟";
    }
    return "未知时钟基准";
}

}  // namespace dsa
