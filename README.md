# 16 通道动态信号采集与波形绘图桌面软件（Qt Widgets）

本项目基于 **Qt 5.15.2 + C++17 + Qt Widgets + CMake**，主路径采用真实板卡 SDK（`atom_16ch_dsa_api.h + atom_16ch_dsa_api.dll`）的**运行时动态加载**方案，支持在没有 `.lib` 导入库时完成编译与运行。

## 1. 目录结构

建议按以下结构放置工程与 SDK：

```text
project_root/
  CMakeLists.txt
  README.md
  src/
  resources/
  third_party/
    device_sdk/
      atom_16ch_dsa_api.h
      atom_16ch_dsa_api.dll
      16通道动态信号采集板接口函数说明.pdf
```

程序会优先在以下路径搜索 DLL（按顺序尝试）：

1. 可执行文件目录：`<exe_dir>/atom_16ch_dsa_api.dll`
2. `<exe_dir>/third_party/device_sdk/atom_16ch_dsa_api.dll`
3. `<exe_dir>/../third_party/device_sdk/atom_16ch_dsa_api.dll`
4. `<exe_dir>/../../third_party/device_sdk/atom_16ch_dsa_api.dll`
5. 当前工作目录及其 `third_party/device_sdk`
6. 系统 PATH（仅文件名 `atom_16ch_dsa_api.dll`）

## 2. 构建方式

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

> CMake 最低版本 3.16，要求 Qt 5.15.2，并启用 `Widgets` 与 `Svg` 组件。

## 3. 为什么没有 `.lib` 也能编译

工程不依赖 `target_link_libraries(... atom_16ch_dsa_api.lib)`，而是使用 `QLibrary` 在运行时：

1. 加载 `atom_16ch_dsa_api.dll`
2. 逐个解析函数地址（`resolve`）
3. 通过函数指针调用接口

这样即使只有 `.dll + .h`，也可正常编译并在运行时工作。

## 4. 如果未来补充 `.lib`，如何改成隐式链接（可选）

可以新增一个可选配置（不影响当前主实现）：

1. 在 CMake 中添加导入库路径并链接 `.lib`
2. 将 `Dsa16ChDllLoader` 替换为直接头文件声明调用
3. 保留 `QLibrary` 路径作为 fallback（推荐）

当前工程默认仍建议保留运行时加载，便于部署与版本切换。

## 5. 程序启动与项目文件参数

程序支持命令行直接传入 `.acqproj`：

```bash
Dsa16ChAcquisition.exe D:\Projects\Demo\Demo.acqproj
```

如果参数中检测到 `.acqproj`，程序会在启动后自动打开该项目。

## 6. 项目文件关联（后续）

可在 Windows 注册表中将 `.acqproj` 关联到本程序，双击项目文件即启动并自动打开。  
本工程已具备命令行接收项目路径能力，关联时只需把 `%1` 传给可执行文件。

## 7. 常见故障排查

### 7.1 DLL 未找到 / SDK 未就绪

检查：

1. `atom_16ch_dsa_api.dll` 是否在搜索路径中
2. 是否存在位数不匹配（x64 程序加载 x86 DLL）
3. 是否缺少 DLL 依赖项（可用 Dependency Walker/Dependencies 检查）

### 7.2 函数解析失败

程序会明确提示失败函数名（例如 `dsa_16ch_read_data`）。  
通常是 DLL 版本与头文件版本不匹配，请使用同一 SDK 版本。

### 7.3 设备打开失败

`dsa_16ch_open()` 成功应返回 `0`，失败 `-1`。  
检查设备驱动、硬件连接、权限、以及是否已被其它进程占用。

### 7.4 采集溢出

连续采集模式需及时读取缓冲区；本工程在轮询中按阈值读取并检查：

- `dsa_16ch_point_num_per_ch_in_buf_query`
- `dsa_16ch_buf_overflow_query`

高采样率下建议保持采集页运行并开启绘图或实时读数。

## 8. 可选 mock 模式（仅降级）

默认使用真实 DLL backend。  
若硬件暂不可用，可设置环境变量后启动：

```bash
set DSA_USE_MOCK=1
Dsa16ChAcquisition.exe
```

此时在未打开设备时可使用 mock 数据进行界面联调；主路径仍为真实 DLL。

## 9. 功能闭环清单

1. 新建项目（创建目录 + `.acqproj` JSON）
2. 打开项目
3. 打开设备（真实 DLL API）
4. 参数设置（采样/触发/时钟/定点点数）
5. 采集开关（`dsa_16ch_start/stop`）
6. 绘图开关（独立于采集服务）
7. 16 通道动态布局波形显示
8. 缓冲点数与溢出状态展示
9. DIO 两组 8 位方向设置、DO 写入、DI 读取
10. 软件信息对话框

