#pragma once

#include <QObject>
#include <QLibrary>
#include <QStringList>

class Dsa16ChDllLoader : public QObject {
    Q_OBJECT

public:
    using FnDsa16ChOpen = int (*)();
    using FnDsa16ChClose = void (*)();
    using FnDsa16ChSampleRateSel = int (*)(unsigned int);
    using FnDsa16ChSampleModeSet = int (*)(unsigned int);
    using FnDsa16ChTrigSrcSel = int (*)(unsigned int);
    using FnDsa16ChExtTrigEdgeSel = int (*)(unsigned int);
    using FnDsa16ChClockBaseSel = int (*)(unsigned int);
    using FnDsa16ChFixPointModePointNumPerChSet = int (*)(unsigned int);
    using FnDsa16ChStart = int (*)();
    using FnDsa16ChStop = int (*)();
    using FnDsa16ChPointNumPerChInBufQuery = int (*)(unsigned int*);
    using FnDsa16ChReadData = int (*)(unsigned int,
                                      double*,
                                      double*,
                                      double*,
                                      double*,
                                      double*,
                                      double*,
                                      double*,
                                      double*,
                                      double*,
                                      double*,
                                      double*,
                                      double*,
                                      double*,
                                      double*,
                                      double*,
                                      double*);
    using FnDsa16ChBufOverflowQuery = int (*)(unsigned int*);
    using FnDsa16ChDioDirSet = int (*)(unsigned int, unsigned int);
    using FnDsa16ChWrDoData = int (*)(unsigned int, unsigned char);
    using FnDsa16ChRdDiData = int (*)(unsigned int, unsigned char*);

    struct Api {
        FnDsa16ChOpen dsa_16ch_open = nullptr;
        FnDsa16ChClose dsa_16ch_close = nullptr;
        FnDsa16ChSampleRateSel dsa_16ch_sample_rate_sel = nullptr;
        FnDsa16ChSampleModeSet dsa_16ch_sample_mode_set = nullptr;
        FnDsa16ChTrigSrcSel dsa_16ch_trig_src_sel = nullptr;
        FnDsa16ChExtTrigEdgeSel dsa_16ch_ext_trig_edge_sel = nullptr;
        FnDsa16ChClockBaseSel dsa_16ch_clock_base_sel = nullptr;
        FnDsa16ChFixPointModePointNumPerChSet dsa_16ch_fix_point_mode_point_num_per_ch_set = nullptr;
        FnDsa16ChStart dsa_16ch_start = nullptr;
        FnDsa16ChStop dsa_16ch_stop = nullptr;
        FnDsa16ChPointNumPerChInBufQuery dsa_16ch_point_num_per_ch_in_buf_query = nullptr;
        FnDsa16ChReadData dsa_16ch_read_data = nullptr;
        FnDsa16ChBufOverflowQuery dsa_16ch_buf_overflow_query = nullptr;
        FnDsa16ChDioDirSet dsa_16ch_dio_dir_set = nullptr;
        FnDsa16ChWrDoData dsa_16ch_wr_do_data = nullptr;
        FnDsa16ChRdDiData dsa_16ch_rd_di_data = nullptr;
    };

    explicit Dsa16ChDllLoader(QObject* parent = nullptr);
    ~Dsa16ChDllLoader() override;

    bool load(const QString& preferredPath = QString());
    void unload();

    bool isLoaded() const;
    QString lastError() const;
    QString loadedPath() const;
    QStringList candidatePaths() const;
    const Api& api() const;

signals:
    void sdkLoadStateChanged(bool loaded, const QString& message);

private:
    bool resolveAll();
    bool resolveSymbol(void** fn, const char* symbolName);
    void clearApi();

private:
    QLibrary m_library;
    Api m_api;
    QString m_lastError;
    QString m_loadedPath;
};
