#include "core/plotservice.h"

PlotService::PlotService(QObject* parent)
    : QObject(parent) {
    connect(&m_timer, &QTimer::timeout, this, &PlotService::plottingTick);
}

PlotService::~PlotService() {
    stopPlotting();
}

void PlotService::startPlotting(int intervalMs) {
    if (m_running) {
        return;
    }
    m_timer.setInterval(intervalMs);
    m_timer.start();
    m_running = true;
    emit plottingChanged(true);
}

void PlotService::stopPlotting() {
    if (!m_running) {
        return;
    }
    m_timer.stop();
    m_running = false;
    emit plottingChanged(false);
}

bool PlotService::isRunning() const {
    return m_running;
}
