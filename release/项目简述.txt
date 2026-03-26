Folder Visualizer 项目简述

一、项目定位
Folder Visualizer 是一个用于目录结构可视化展示与导出的 Windows 原生工具，适合项目交付、结构说明和教学演示。

二、主要功能
1. 扫描文件夹并生成树形结构
2. 文件类型识别与图标映射展示
3. 导出 TXT 文本结构
4. 导出 PNG 高清图片

三、技术方案
- 语言：C++17
- UI：Win32 API
- 图形：GDI+
- 构建：CMake + MinGW-w64
- 资源：Windows Resource（图标与版本信息）

四、构建与发布
- 已切换为纯 CMake 配置
- 标准构建目录：build_mingw/
- 可执行文件输出目录：release/
- 运行资源（icons/settings/tree.ico）由 CMake 自动复制到 release/

五、当前构建命令
1) cmake --preset mingw-release
2) cmake --build --preset mingw-release

六、产物
- release/FolderVisualizer.exe
