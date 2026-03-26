#include "FileTypeManager.h"
#include <algorithm>
#include <cctype>
#include <iostream>

FileTypeManager& FileTypeManager::getInstance() {
    static FileTypeManager instance;
    return instance;
}

FileTypeManager::FileTypeManager() {
    initializeRegistry();
}

void FileTypeManager::registerType(FileCategory cat, const std::string& name,
                                   const std::string& mimeBase,
                                   const std::vector<std::string>& exts,
                                   bool binary) {
    FileTypeInfo info;
    info.category = cat;
    info.displayName = name;
    info.mimeType = mimeBase;
    info.extensions = exts;
    info.isBinary = binary;

    registry_[cat] = info;

    for (const auto& ext : exts) {
        std::string normalized = normalizeExtension(ext);
        extToCategory_[normalized] = cat;
    }
}

void FileTypeManager::initializeRegistry() {
    registerType(FileCategory::IMAGE, "图片", "image",
        {"jpg", "jpeg", "png", "gif", "bmp", "webp", "tiff", "tif", "svg", "ico", "raw", "psd"}, true);

    registerType(FileCategory::AUDIO, "音频", "audio",
        {"mp3", "wav", "flac", "aac", "ogg", "wma", "m4a", "opus"}, true);

    registerType(FileCategory::VIDEO, "视频", "video",
        {"mp4", "avi", "mkv", "mov", "wmv", "flv", "webm", "m4v", "mpg", "mpeg"}, true);

    registerType(FileCategory::TEXT, "文本", "text",
        {"txt", "md", "log", "rtf"}, false);

    registerType(FileCategory::DOCUMENT, "文档", "application",
        {"doc", "docx", "odt"}, true);

    registerType(FileCategory::SPREADSHEET, "表格", "application",
        {"xls", "xlsx", "ods", "csv"}, true);

    registerType(FileCategory::PRESENTATION, "演示文稿", "application",
        {"ppt", "pptx", "odp"}, true);

    registerType(FileCategory::PDF, "PDF文档", "application/pdf",
        {"pdf"}, true);

    registerType(FileCategory::SOURCE_CODE, "源代码", "text",
        {"c", "cpp", "cc", "cxx",
         "java", "class", "jar",
         "py", "pyc", "pyo",
         "js", "ts", "jsx", "tsx",
         "go", "rs", "swift", "kt",
         "cs", "vb", "fs",
         "rb", "php", "pl", "sh", "bash",
         "sql"}, false);

    registerType(FileCategory::HEADER_FILE, "头文件", "text",
        {"h", "hpp"}, false);

    registerType(FileCategory::MARKUP, "标记语言", "text",
        {"html", "htm", "xhtml", "xml", "css", "scss", "sass", "less"}, false);

    registerType(FileCategory::CONFIG, "配置文件", "text",
        {"json", "yaml", "yml", "toml", "ini", "conf", "cfg", "properties"}, false);

    registerType(FileCategory::ARCHIVE, "压缩包", "application",
        {"zip", "rar", "7z", "tar", "gz", "bz2", "xz", "lz", "cab"}, true);

    registerType(FileCategory::EXECUTABLE, "可执行文件", "application",
        {"exe", "msi", "dll", "sys", "com"}, true);

    registerType(FileCategory::SCRIPT, "脚本", "text",
        {"bat", "cmd", "ps1", "vbs", "wsf", "reg"}, false);

    registerType(FileCategory::DATABASE, "数据库", "application",
        {"db", "sqlite", "sqlite3", "mdb", "accdb", "dbf"}, true);

    registerType(FileCategory::FONT, "字体", "font",
        {"ttf", "otf", "woff", "woff2", "eot"}, true);
}

std::string FileTypeManager::normalizeExtension(const std::string& ext) const {
    std::string result;
    result.reserve(ext.length());

    for (char c : ext) {
        if (c == '.') continue;
        result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return result;
}

std::string FileTypeManager::extractExtension(const std::string& filename) const {
    size_t pos = filename.find_last_of('.');
    if (pos == std::string::npos || pos == 0 || pos == filename.length() - 1) {
        return "";
    }
    return normalizeExtension(filename.substr(pos));
}

FileCategory FileTypeManager::categorize(const std::string& extension) const {
    std::string norm = normalizeExtension(extension);
    if (norm.empty()) return FileCategory::UNKNOWN;

    auto it = extToCategory_.find(norm);
    return (it != extToCategory_.end()) ? it->second : FileCategory::UNKNOWN;
}

FileCategory FileTypeManager::categorizeByFilename(const std::string& filename) const {
    auto cacheIt = cache_.find(filename);
    if (cacheIt != cache_.end()) {
        return cacheIt->second;
    }

    FileCategory result = categorize(extractExtension(filename));
    cache_[filename] = result;
    return result;
}

std::string FileTypeManager::getCategoryName(FileCategory cat) const {
    switch (cat) {
        case FileCategory::DIRECTORY: return "目录";
        case FileCategory::IMAGE: return "图片";
        case FileCategory::AUDIO: return "音频";
        case FileCategory::VIDEO: return "视频";
        case FileCategory::TEXT: return "文本";
        case FileCategory::DOCUMENT: return "文档";
        case FileCategory::SPREADSHEET: return "表格";
        case FileCategory::PRESENTATION: return "演示文稿";
        case FileCategory::PDF: return "PDF";
        case FileCategory::SOURCE_CODE: return "源代码";
        case FileCategory::HEADER_FILE: return "头文件";
        case FileCategory::MARKUP: return "标记语言";
        case FileCategory::CONFIG: return "配置";
        case FileCategory::ARCHIVE: return "压缩包";
        case FileCategory::EXECUTABLE: return "可执行文件";
        case FileCategory::SCRIPT: return "脚本";
        case FileCategory::DATABASE: return "数据库";
        case FileCategory::FONT: return "字体";
        default: return "未知";
    }
}

std::string FileTypeManager::getCategoryIconBaseName(FileCategory cat) const {
    switch (cat) {
        case FileCategory::DIRECTORY: return "folder";
        case FileCategory::IMAGE: return "image";
        case FileCategory::AUDIO: return "audio";
        case FileCategory::VIDEO: return "video";
        case FileCategory::TEXT: return "text";
        case FileCategory::DOCUMENT: return "word";
        case FileCategory::SPREADSHEET: return "excel";
        case FileCategory::PRESENTATION: return "powerpoint";
        case FileCategory::PDF: return "pdf";
        case FileCategory::SOURCE_CODE: return "code";
        case FileCategory::HEADER_FILE: return "header";
        case FileCategory::MARKUP: return "code";
        case FileCategory::CONFIG: return "config";
        case FileCategory::ARCHIVE: return "archive";
        case FileCategory::EXECUTABLE: return "executable";
        case FileCategory::SCRIPT: return "script";
        case FileCategory::DATABASE: return "database";
        case FileCategory::FONT: return "font";
        default: return "file";
    }
}

std::vector<std::string> FileTypeManager::getExtensionsForCategory(FileCategory cat) const {
    auto it = registry_.find(cat);
    if (it != registry_.end()) {
        return it->second.extensions;
    }
    return {};
}

bool FileTypeManager::isPreviewable(FileCategory cat) const {
    return cat == FileCategory::IMAGE ||
           cat == FileCategory::TEXT ||
           cat == FileCategory::MARKUP ||
           cat == FileCategory::CONFIG ||
           cat == FileCategory::SOURCE_CODE ||
           cat == FileCategory::SCRIPT;
}

bool FileTypeManager::isEditable(FileCategory cat) const {
    return cat == FileCategory::TEXT ||
           cat == FileCategory::CONFIG ||
           cat == FileCategory::SOURCE_CODE ||
           cat == FileCategory::SCRIPT ||
           cat == FileCategory::MARKUP;
}

size_t FileTypeManager::getRegisteredTypeCount() const {
    return registry_.size();
}

void FileTypeManager::printRegistry() const {
    std::cout << "Registered " << registry_.size() << " file types:\n";
    for (const auto& pair : registry_) {
        const auto& info = pair.second;
        std::cout << "  " << getCategoryName(info.category)
                  << " (" << info.extensions.size() << " extensions)\n";
    }
}
