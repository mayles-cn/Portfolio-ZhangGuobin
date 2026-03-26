#ifndef ICON_MANAGER_H
#define ICON_MANAGER_H

#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <map>
#include <memory>
#include "FileTypeManager.h"

#pragma comment(lib, "gdiplus.lib")

/**
 * @brief 图标尺寸枚举
 */
enum class IconSize {
    SMALL = 16,     // 树形列表
    MEDIUM = 24,    // 工具栏
    LARGE = 32,     // 详情视图
    EXTRA_LARGE = 48 // 预览
};

/**
 * @brief 图标缓存项
 */
struct CachedIcon {
    Gdiplus::Image* image;
    std::string path;
    IconSize size;
    FILETIME lastModified;
    bool isValid;
    
    CachedIcon() : image(nullptr), size(IconSize::SMALL), isValid(false) {}
    ~CachedIcon() { delete image; }
};

/**
 * @brief 图标管理器 - 负责加载、缓存、缩放图标
 * 职责：管理所有图标资源，提供统一的绘制接口
 */
class IconManager {
public:
    IconManager();
    ~IconManager();
    
    // 禁止拷贝
    IconManager(const IconManager&) = delete;
    IconManager& operator=(const IconManager&) = delete;
    
    // 初始化与配置
    bool initialize(const std::string& basePath = "icons/");
    void setDefaultIconPaths(const std::string& folderIcon, 
                             const std::string& fileIcon);
    
    // 设置自定义扩展名图标（如 "cpp" -> "cprog.png"）
    void setExtensionIcon(const std::string& extension, const std::string& iconFile);
    void clearExtensionIcons();
    
    // 图标获取（自动缓存）
    Gdiplus::Image* getIcon(FileCategory category, IconSize size = IconSize::SMALL);
    Gdiplus::Image* getIconForFile(const std::string& filename, 
                                   bool isDirectory,
                                   IconSize size = IconSize::SMALL);
    Gdiplus::Image* getIconByPath(const std::string& iconPath, 
                                  IconSize size = IconSize::SMALL);
    
    // 公开图标路径解析接口
    std::string resolveIconPathForFile(const std::string& filename, bool isDirectory);
    
    // 绘制接口（带质量控制）
    void drawIcon(HDC hdc, int x, int y, Gdiplus::Image* icon, 
                  IconSize size = IconSize::SMALL);
    void drawIconForFile(HDC hdc, int x, int y, const std::string& filename,
                         bool isDirectory, IconSize size = IconSize::SMALL);
    
    // 缓存管理
    void clearCache();
    void preloadCommonIcons(); // 预加载常用图标
    size_t getCacheSize() const;
    
    // 工具
    static std::wstring toWideString(const std::string& str);
    static std::string toUtf8String(const std::wstring& wstr);
    static bool fileExists(const std::string& path);
    
private:
    std::string basePath_;
    std::string defaultFolderIcon_;
    std::string defaultFileIcon_;
    
    // 自定义扩展名到图标的映射（如 "cpp" -> "cprog.png"）
    std::map<std::string, std::string> extensionIconMap_;
    
    // 多级缓存：路径+尺寸 -> 图标
    std::map<std::pair<std::string, IconSize>, std::unique_ptr<CachedIcon>> cache_;
    
    // 后备图标（内存位图，确保始终可用）
    std::map<IconSize, HBRUSH> fallbackBrushes_; // 按尺寸的颜色块
    
    // 内部方法
    std::string resolveIconPath(FileCategory category);
    Gdiplus::Image* loadAndCache(const std::string& path, IconSize size);
    Gdiplus::Image* createScaledCopy(Gdiplus::Image* source, IconSize size);
    void drawFallback(HDC hdc, int x, int y, IconSize size, bool isDirectory);
    
    // 缓存键生成
    static std::string makeCacheKey(const std::string& path, IconSize size);
};

#endif // ICON_MANAGER_H
