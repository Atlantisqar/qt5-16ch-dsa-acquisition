#include "core/dsa16chdllloader.h"
#include "atom_16ch_dsa_api.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStringList>

namespace {
const char* kSdkDllName = "atom_16ch_dsa_api.dll";
}

Dsa16ChDllLoader::Dsa16ChDllLoader(QObject* parent)
    : QObject(parent) {}

Dsa16ChDllLoader::~Dsa16ChDllLoader() {
    unload();
}

bool Dsa16ChDllLoader::load(const QString& preferredPath) {
    unload();

    QStringList attempts;
    QStringList paths = candidatePaths();
    if (!preferredPath.trimmed().isEmpty()) {
        paths.removeAll(preferredPath);
        paths.prepend(preferredPath);
    }

    for (const QString& path : paths) {
        m_library.setFileName(path);
        if (!m_library.load()) {
            attempts << QString("%1 -> %2").arg(path, m_library.errorString());
            continue;
        }

        if (!resolveAll()) {
            const QString resolveError = m_lastError;
            m_library.unload();
            clearApi();
            attempts << QString("%1 -> %2").arg(path, resolveError);
            continue;
        }

        m_loadedPath = QFileInfo(path).isAbsolute() ? path : m_library.fileName();
        m_lastError.clear();
        emit sdkLoadStateChanged(true, QString("SDK 已加载：%1").arg(m_loadedPath));
        return true;
    }

    m_lastError = QString("SDK 加载失败，尝试路径如下：\n%1").arg(attempts.join("\n"));
    emit sdkLoadStateChanged(false, m_lastError);
    return false;
}

void Dsa16ChDllLoader::unload() {
    if (m_library.isLoaded()) {
        m_library.unload();
    }
    clearApi();
    m_loadedPath.clear();
}

bool Dsa16ChDllLoader::isLoaded() const {
    return m_library.isLoaded();
}

QString Dsa16ChDllLoader::lastError() const {
    return m_lastError;
}

QString Dsa16ChDllLoader::loadedPath() const {
    return m_loadedPath;
}

QStringList Dsa16ChDllLoader::candidatePaths() const {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString cwd = QDir::currentPath();
    QStringList paths;
    paths << QFileInfo(appDir + "/" + kSdkDllName).absoluteFilePath()
          << QFileInfo(appDir + "/third_party/device_sdk/" + kSdkDllName).absoluteFilePath()
          << QFileInfo(appDir + "/../third_party/device_sdk/" + kSdkDllName).absoluteFilePath()
          << QFileInfo(appDir + "/../../third_party/device_sdk/" + kSdkDllName).absoluteFilePath()
          << QFileInfo(cwd + "/" + kSdkDllName).absoluteFilePath()
          << QFileInfo(cwd + "/third_party/device_sdk/" + kSdkDllName).absoluteFilePath()
          << QFileInfo(cwd + "/../third_party/device_sdk/" + kSdkDllName).absoluteFilePath()
          << kSdkDllName;
    paths.removeDuplicates();
    return paths;
}

const Dsa16ChDllLoader::Api& Dsa16ChDllLoader::api() const {
    return m_api;
}

bool Dsa16ChDllLoader::resolveAll() {
    return resolveSymbol(reinterpret_cast<void**>(&m_api.dsa_16ch_open), "dsa_16ch_open") &&
           resolveSymbol(reinterpret_cast<void**>(&m_api.dsa_16ch_close), "dsa_16ch_close") &&
           resolveSymbol(reinterpret_cast<void**>(&m_api.dsa_16ch_sample_rate_sel), "dsa_16ch_sample_rate_sel") &&
           resolveSymbol(reinterpret_cast<void**>(&m_api.dsa_16ch_sample_mode_set), "dsa_16ch_sample_mode_set") &&
           resolveSymbol(reinterpret_cast<void**>(&m_api.dsa_16ch_trig_src_sel), "dsa_16ch_trig_src_sel") &&
           resolveSymbol(reinterpret_cast<void**>(&m_api.dsa_16ch_ext_trig_edge_sel), "dsa_16ch_ext_trig_edge_sel") &&
           resolveSymbol(reinterpret_cast<void**>(&m_api.dsa_16ch_clock_base_sel), "dsa_16ch_clock_base_sel") &&
           resolveSymbol(reinterpret_cast<void**>(&m_api.dsa_16ch_fix_point_mode_point_num_per_ch_set),
                         "dsa_16ch_fix_point_mode_point_num_per_ch_set") &&
           resolveSymbol(reinterpret_cast<void**>(&m_api.dsa_16ch_start), "dsa_16ch_start") &&
           resolveSymbol(reinterpret_cast<void**>(&m_api.dsa_16ch_stop), "dsa_16ch_stop") &&
           resolveSymbol(reinterpret_cast<void**>(&m_api.dsa_16ch_point_num_per_ch_in_buf_query),
                         "dsa_16ch_point_num_per_ch_in_buf_query") &&
           resolveSymbol(reinterpret_cast<void**>(&m_api.dsa_16ch_read_data), "dsa_16ch_read_data") &&
           resolveSymbol(reinterpret_cast<void**>(&m_api.dsa_16ch_buf_overflow_query), "dsa_16ch_buf_overflow_query") &&
           resolveSymbol(reinterpret_cast<void**>(&m_api.dsa_16ch_dio_dir_set), "dsa_16ch_dio_dir_set") &&
           resolveSymbol(reinterpret_cast<void**>(&m_api.dsa_16ch_wr_do_data), "dsa_16ch_wr_do_data") &&
           resolveSymbol(reinterpret_cast<void**>(&m_api.dsa_16ch_rd_di_data), "dsa_16ch_rd_di_data");
}

bool Dsa16ChDllLoader::resolveSymbol(void** fn, const char* symbolName) {
    *fn = m_library.resolve(symbolName);
    if (*fn != nullptr) {
        return true;
    }
    m_lastError = QString("函数解析失败：%1，错误：%2").arg(symbolName, m_library.errorString());
    return false;
}

void Dsa16ChDllLoader::clearApi() {
    m_api = Api{};
}
