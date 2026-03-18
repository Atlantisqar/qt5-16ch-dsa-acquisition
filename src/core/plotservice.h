#pragma once

#include <QObject>
#include <QTimer>

class PlotService : public QObject {
    Q_OBJECT

public:
    explicit PlotService(QObject* parent = nullptr);
    ~PlotService() override;

    void startPlotting(int intervalMs = 33);
    void stopPlotting();
    bool isRunning() const;

signals:
    void plottingChanged(bool running);
    void plottingTick();

private:
    QTimer m_timer;
    bool m_running = false;
};
