#include "IconManager.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <utility>

IconManager::IconManager() 
    : basePath_("icons/"),
      defaultFolderIcon_("folder.png"),
      defaultFileIcon_("file.png") {
}

IconManager::~IconManager() {
    clearCache();
    for (auto& pair : fallbackBrushes_) {
        DeleteObject(pair.second);
    }
}

bool IconManager::initialize(const std::string& basePath) {
    basePath_ = basePath;
    if (!basePath_.empty() && basePath_.back() != '/' && basePath_.back() != '\\') {
        basePath_ += '/';
    }
    
    // 确保目录存在
    std::wstring wBase = toWideString(basePath_);
    DWORD attrs = GetFileAttributesW(wBase.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        // 尝试创建目录
        CreateDirectoryW(wBase.c_str(), nullptr);
    }
    
    preloadCommonIcons();
    return true;
}

void IconManager::setDefaultIconPaths(const std::string& folderIcon, 
                                      const std::string& fileIcon) {
    defaultFolderIcon_ = folderIcon;
    defaultFileIcon_ = fileIcon;
}

void IconManager::setExtensionIcon(const std::string& extension, const std::string& iconFile) {
    std::string ext = extension;
    // 统一格式：小写，去掉点
    if (!ext.empty() && ext[0] == '.') {
        ext = ext.substr(1);
    }
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    extensionIconMap_[ext] = iconFile;
}

void IconManager::clearExtensionIcons() {
    extensionIconMap_.clear();
}

void IconManager::clearCache() {
    cache_.clear();
}

void IconManager::preloadCommonIcons() {
    // 预加载最常用的图标避免首次卡顿
    FileTypeManager& ftm = FileTypeManager::getInstance();
    
    IconSize sizes[] = {IconSize::SMALL, IconSize::MEDIUM};
    FileCategory cats[] = {
        FileCategory::DIRECTORY,
        FileCategory::IMAGE,
        FileCategory::TEXT,
        FileCategory::SOURCE_CODE
    };
    
    for (auto size : sizes) {
        for (auto cat : cats) {
            getIcon(cat, size); // 触发缓存
        }
    }
}

size_t IconManager::getCacheSize() const {
    return cache_.size();
}

std::string IconManager::resolveIconPath(FileCategory category) {
    FileTypeManager& ftm = FileTypeManager::getInstance();
    std::string baseName = ftm.getCategoryIconBaseName(category);
    return basePath_ + baseName + ".png";
}

std::string IconManager::resolveIconPathForFile(const std::string& filename, 
                                                bool isDirectory) {
    if (isDirectory) {
        return basePath_ + defaultFolderIcon_;
    }
    
    FileTypeManager& ftm = FileTypeManager::getInstance();
    FileCategory cat = ftm.categorizeByFilename(filename);
    
    // 获取扩展名
    std::string ext = ftm.extractExtension(filename);
    
    // 调试：输出解析过程
    // std::wstring wFilename = toWideString(filename);
    // std::wstring wExt = toWideString(ext);
    // std::wstring debug = L"文件: " + wFilename + L" 扩展名: " + wExt + L"\n";
    // debug += L"自定义映射数量: " + std::to_wstring(extensionIconMap_.size()) + L"\n";
    // for (const auto& pair : extensionIconMap_) {
    //     debug += L"  [" + toWideString(pair.first) + L"] -> " + toWideString(pair.second) + L"\n";
    // }
    // MessageBoxW(nullptr, debug.c_str(), L"图标解析", MB_OK);
    
    // 优先检查自定义扩展名图标映射
    if (!ext.empty()) {
        auto it = extensionIconMap_.find(ext);
        if (it != extensionIconMap_.end()) {
            std::string customPath = basePath_ + it->second;
            if (fileExists(customPath)) {
                return customPath;
            }
        }
    }
    
    // 尝试精确匹配扩展名图标（如 cpp.png）
    if (!ext.empty()) {
        std::string specificPath = basePath_ + ext + ".png";
        if (fileExists(specificPath)) {
            return specificPath;
        }
    }
    
    // 回退到类型图标
    return resolveIconPath(cat);
}

Gdiplus::Image* IconManager::getIcon(FileCategory category, IconSize size) {
    std::string path = resolveIconPath(category);
    return getIconByPath(path, size);
}

Gdiplus::Image* IconManager::getIconForFile(const std::string& filename, 
                                            bool isDirectory,
                                            IconSize size) {
    std::string path = resolveIconPathForFile(filename, isDirectory);
    return getIconByPath(path, size);
}

Gdiplus::Image* IconManager::getIconByPath(const std::string& iconPath, IconSize size) {
    std::string cacheKey = makeCacheKey(iconPath, size);
    auto key = std::make_pair(iconPath, size);
    
    auto it = cache_.find(key);
    if (it != cache_.end() && it->second->isValid) {
        return it->second->image;
    }
    
    // 加载新图标
    Gdiplus::Image* img = loadAndCache(iconPath, size);
    return img;
}

Gdiplus::Image* IconManager::loadAndCache(const std::string& path, IconSize size) {
    std::wstring wPath = toWideString(path);
    
    // 检查文件存在
    if (!fileExists(path)) {
        auto key = std::make_pair(path, size);
        auto cached = std::make_unique<CachedIcon>();
        cached->path = path;
        cached->size = size;
        cached->isValid = false;
        cache_[key] = std::move(cached);
        return nullptr;
    }
    
    // 加载原始图像
    Gdiplus::Image* original = new Gdiplus::Image(wPath.c_str());
    if (original->GetLastStatus() != Gdiplus::Ok) {
        delete original;
        auto key = std::make_pair(path, size);
        auto cached = std::make_unique<CachedIcon>();
        cached->path = path;
        cached->size = size;
        cached->isValid = false;
        cache_[key] = std::move(cached);
        return nullptr;
    }
    
    // 缩放到目标尺寸
    Gdiplus::Image* scaled = createScaledCopy(original, size);
    delete original;
    
    // 缓存
    auto key = std::make_pair(path, size);
    auto cached = std::make_unique<CachedIcon>();
    cached->image = scaled;
    cached->path = path;
    cached->size = size;
    cached->isValid = (scaled != nullptr);
    
    // 获取文件时间
    WIN32_FILE_ATTRIBUTE_DATA attrData;
    if (GetFileAttributesExW(wPath.c_str(), GetFileExInfoStandard, &attrData)) {
        cached->lastModified = attrData.ftLastWriteTime;
    }
    
    Gdiplus::Image* result = cached->image;
    cache_[key] = std::move(cached);
    return result;
}

Gdiplus::Image* IconManager::createScaledCopy(Gdiplus::Image* source, IconSize size) {
    if (!source) return nullptr;
    
    int targetSize = static_cast<int>(size);
    Gdiplus::Bitmap* bmp = new Gdiplus::Bitmap(targetSize, targetSize, PixelFormat32bppARGB);
    Gdiplus::Graphics graphics(bmp);
    
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    
    // 保持宽高比
    int srcW = source->GetWidth();
    int srcH = source->GetHeight();
    float ratio = std::min(static_cast<float>(targetSize) / srcW,
                          static_cast<float>(targetSize) / srcH);
    int drawW = static_cast<int>(srcW * ratio);
    int drawH = static_cast<int>(srcH * ratio);
    int offsetX = (targetSize - drawW) / 2;
    int offsetY = (targetSize - drawH) / 2;
    
    graphics.DrawImage(source, offsetX, offsetY, drawW, drawH);
    return bmp;
}

void IconManager::drawIcon(HDC hdc, int x, int y, Gdiplus::Image* icon, IconSize size) {
    if (!icon) {
        drawFallback(hdc, x, y, size, false);
        return;
    }
    
    Gdiplus::Graphics graphics(hdc);
    graphics.DrawImage(icon, x, y, static_cast<int>(size), static_cast<int>(size));
}

void IconManager::drawIconForFile(HDC hdc, int x, int y, const std::string& filename,
                                  bool isDirectory, IconSize size) {
    Gdiplus::Image* icon = getIconForFile(filename, isDirectory, size);
    if (!icon) {
        drawFallback(hdc, x, y, size, isDirectory);
        return;
    }
    drawIcon(hdc, x, y, icon, size);
}

void IconManager::drawFallback(HDC hdc, int x, int y, IconSize size, bool isDirectory) {
    int sz = static_cast<int>(size);
    RECT rect = {x, y, x + sz, y + sz};
    
    // 创建或获取画刷
    auto it = fallbackBrushes_.find(size);
    if (it == fallbackBrushes_.end()) {
        COLORREF color = isDirectory ? RGB(64, 128, 128) : RGB(128, 128, 128);
        HBRUSH brush = CreateSolidBrush(color);
        fallbackBrushes_[size] = brush;
        it = fallbackBrushes_.find(size);
    }
    
    FillRect(hdc, &rect, it->second);
    
    // 绘制简单标识
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));
    HFONT hFont = CreateFontW(sz/2, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, 0, 0, 0, 0, L"Arial");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    
    std::wstring text = isDirectory ? L"F" : L"f";
    DrawTextW(hdc, text.c_str(), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
}

std::string IconManager::makeCacheKey(const std::string& path, IconSize size) {
    std::ostringstream oss;
    oss << path << "@" << static_cast<int>(size);
    return oss.str();
}

// 静态工具方法
std::wstring IconManager::toWideString(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (size <= 0) return std::wstring();
    std::wstring result(size - 1, 0);  // -1 排除终止符
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
    return result;
}

std::string IconManager::toUtf8String(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return std::string();
    std::string result(size - 1, 0);  // -1 排除终止符
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size, nullptr, nullptr);
    return result;
}

bool IconManager::fileExists(const std::string& path) {
    std::wstring wPath = toWideString(path);
    DWORD attrs = GetFileAttributesW(wPath.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY));
}
