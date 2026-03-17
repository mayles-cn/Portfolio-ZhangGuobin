#include "FileTree.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <functional>
#include <sstream>

FileTree::FileTree() 
    : nodeCount_(0), fileCount_(0), dirCount_(0), iconMgr_(nullptr) {
}

FileTree::~FileTree() = default;

bool ScanOptions::skipDirectory(const std::string& name) const {
    return name == "." || name == ".." || name.empty();
}

void FileTree::setRootPath(const std::string& path) {
    rootPath_ = path;
    // 标准化路径
    if (!rootPath_.empty()) {
        std::replace(rootPath_.begin(), rootPath_.end(), '/', '\\');
        while (rootPath_.length() > 1 && rootPath_.back() == '\\') {
            rootPath_.pop_back();
        }
    }
    
    // 设置根节点
    size_t pos = rootPath_.find_last_of('\\');
    root_.name = (pos != std::string::npos) ? rootPath_.substr(pos + 1) : rootPath_;
    root_.fullPath = rootPath_;
    root_.isDirectory = true;
    root_.depth = 0;
    root_.isLast = true;
    root_.category = FileCategory::DIRECTORY;
}

void FileTree::setIconManager(IconManager* iconMgr) {
    iconMgr_ = iconMgr;
}

void FileTree::setOptions(const ScanOptions& options) {
    options_ = options;
}

bool FileTree::scan() {
    if (rootPath_.empty()) return false;
    
    nodeCount_ = 0;
    fileCount_ = 0;
    dirCount_ = 0;
    root_.children.clear();
    
    // 验证根路径
    std::wstring wRoot = IconManager::toWideString(rootPath_);
    DWORD attrs = GetFileAttributesW(wRoot.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        return false;
    }
    
    scanRecursive(rootPath_, root_, 0);
    
    // 填充根节点图标
    if (iconMgr_) {
        fillIconPath(root_);
    }
    
    return true;
}

bool FileTree::scan(const std::string& path) {
    setRootPath(path);
    return scan();
}

bool FileTree::scanWithProgress(ScanProgressCallback callback) {
    progressCallback_ = callback;
    bool result = scan();
    progressCallback_ = nullptr;
    return result;
}

void FileTree::scanRecursive(const std::string& path, TreeNode& parent, int depth) {
    if (depth >= static_cast<int>(options_.maxDepth)) return;
    
    std::string searchPath = path + "\\*";
    std::wstring wSearch = IconManager::toWideString(searchPath);
    
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(wSearch.c_str(), &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) return;
    
    std::vector<TreeNode> files;
    std::vector<TreeNode> dirs;
    size_t count = 0;
    
    do {
        std::wstring wName(findData.cFileName);
        std::string name = IconManager::toUtf8String(wName);
        
        if (options_.skipDirectory(name)) continue;
        
        // 检查属性
        bool isHidden = (findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
        bool isSystem = (findData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) != 0;
        
        if (isHidden && !options_.includeHidden) continue;
        if (isSystem && !options_.includeSystem) continue;
        
        TreeNode node;
        node.name = name;
        node.fullPath = path + "\\" + name;
        node.isDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        node.depth = depth + 1;
        node.attributes = findData.dwFileAttributes;
        node.lastModified = findData.ftLastWriteTime;
        
        processNode(node, findData);
        
        if (node.isDirectory) {
            dirs.push_back(node);
        } else {
            files.push_back(node);
        }
        
        nodeCount_++;
        if (node.isDirectory) dirCount_++; else fileCount_++;
        
        // 进度回调
        if (progressCallback_ && nodeCount_ % 100 == 0) {
            progressCallback_(node.fullPath, nodeCount_);
        }
        
        // 限制检查
        if (++count >= options_.maxFilesPerDir) break;
        
    } while (FindNextFileW(hFind, &findData));
    
    FindClose(hFind);
    
    // 排序
    auto cmp = [](const TreeNode& a, const TreeNode& b) { return a.name < b.name; };
    std::sort(dirs.begin(), dirs.end(), cmp);
    std::sort(files.begin(), files.end(), cmp);
    
    // 合并并标记isLast
    std::vector<TreeNode> all;
    all.reserve(dirs.size() + files.size());
    all.insert(all.end(), dirs.begin(), dirs.end());
    all.insert(all.end(), files.begin(), files.end());
    
    for (size_t i = 0; i < all.size(); i++) {
        all[i].isLast = (i == all.size() - 1);
        
        if (all[i].isDirectory) {
            // 递归前读取目录信息
            if (options_.readDescriptions || options_.readDirectoryThumbnails) {
                readDirectoryInfo(all[i]);
                findCoverImage(all[i]);
            }
            
            scanRecursive(all[i].fullPath, all[i], depth + 1);
        }
        
        // 填充图标
        if (iconMgr_) {
            fillIconPath(all[i]);
        }
        
        parent.children.push_back(std::move(all[i]));
    }
}

void FileTree::processNode(TreeNode& node, const WIN32_FIND_DATAW& findData) {
    FileTypeManager& ftm = FileTypeManager::getInstance();
    
    if (node.isDirectory) {
        node.category = FileCategory::DIRECTORY;
    } else {
        node.fileSize = ((ULONGLONG)findData.nFileSizeHigh << 32) | findData.nFileSizeLow;
        node.extension = ftm.extractExtension(node.name);
        node.category = ftm.categorize(node.extension);
    }
}

void FileTree::fillIconPath(TreeNode& node) {
    if (!iconMgr_) return;
    
    // 这里只记录图标路径，实际加载由IconManager负责
    // 为了避免重复查询，我们存储解析后的路径
    node.displayIcon = iconMgr_->resolveIconPathForFile(node.name, node.isDirectory);
}

void FileTree::readDirectoryInfo(TreeNode& node) {
    if (!options_.readDescriptions) return;
    
    std::string infoPath = node.fullPath + "\\info.txt";
    std::wstring wPath = IconManager::toWideString(infoPath);
    
    HANDLE hFile = CreateFileW(wPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;
    
    char buffer[256] = {0};
    DWORD read = 0;
    if (ReadFile(hFile, buffer, sizeof(buffer) - 1, &read, nullptr) && read > 0) {
        // 清理换行符
        for (int i = 0; i < 255 && buffer[i]; i++) {
            if (buffer[i] == '\r' || buffer[i] == '\n') {
                buffer[i] = '\0';
                break;
            }
        }
        node.description = std::string(buffer);
    }
    CloseHandle(hFile);
}

void FileTree::findCoverImage(TreeNode& node) {
    if (!options_.readDirectoryThumbnails) return;
    
    static const char* coverNames[] = {"cover.jpg", "cover.png", "surface.jpg", "surface.png"};
    
    for (const char* cover : coverNames) {
        std::string coverPath = node.fullPath + "\\" + cover;
        if (IconManager::fileExists(coverPath)) {
            node.description = "[封面: " + std::string(cover) + "] " + node.description;
            break;
        }
    }
}

std::string FileTree::exportToText(bool includeIcons, bool includeSize, 
                                   bool includeCategory) const {
    std::ostringstream oss;
    
    // 头部 - 只显示根目录名，不显示类别
    oss << root_.name << "/\n";
    
    // 递归生成 - includeCategory=false 来隐藏类别信息
    for (const auto& child : root_.children) {
        generateTextRecursive(child, oss, "", false, includeSize, false);  // includeCategory=false
    }
    
    return oss.str();
}

void FileTree::generateTextRecursive(const TreeNode& node, std::ostringstream& oss,
                                     const std::string& prefix,
                                     bool includeIcons, bool includeSize,
                                     bool includeCategory) const {
    std::string connector = node.isLast ? "└── " : "├── ";
    oss << prefix << connector << node.name;
    
    if (node.isDirectory) {
        oss << "/";  // 目录添加斜杠标识
    }
    
    // 注意：不导出类别、扩展名和大小信息（includeCategory=false）
    // 即使 includeCategory=true，也不输出这些信息
    
    // 描述（如果有）
    if (!node.description.empty()) {
        oss << " <- " << node.description;
    }
    
    oss << "\n";
    
    // 子节点
    std::string childPrefix = prefix + (node.isLast ? "    " : "│   ");
    for (const auto& child : node.children) {
        generateTextRecursive(child, oss, childPrefix, includeIcons, includeSize, includeCategory);
    }
}

bool FileTree::saveToTextFile(const std::string& outputPath,
                              bool includeIcons, bool includeSize,
                              bool includeCategory) const {
    std::ofstream file(outputPath, std::ios::binary);
    if (!file.is_open()) return false;
    
    // UTF-8 BOM
    file.write("\xEF\xBB\xBF", 3);
    
    file << "========================================\n";
    file << "        文件夹结构可视化\n";
    file << "========================================\n";
    file << "根目录: " << rootPath_ << "\n";
    file << "总节点: " << nodeCount_ << " (目录: " << dirCount_ 
         << ", 文件: " << fileCount_ << ")\n";
    file << "========================================\n\n";
    
    file << exportToText(includeIcons, includeSize, includeCategory);
    
    return file.good();
}

std::map<FileCategory, size_t> FileTree::getCategoryStatistics() const {
    std::map<FileCategory, size_t> stats;
    
    std::function<void(const TreeNode&)> count = [&](const TreeNode& n) {
        if (!n.isDirectory) {
            stats[n.category]++;
        }
        for (const auto& c : n.children) count(c);
    };
    
    count(root_);
    return stats;
}

std::string FileTree::formatFileTime(const FILETIME& ft) {
    SYSTEMTIME st;
    FileTimeToSystemTime(&ft, &st);
    
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return std::string(buffer);
}

bool FileTree::isValidPath(const std::string& path) {
    std::wstring wPath = IconManager::toWideString(path);
    return GetFileAttributesW(wPath.c_str()) != INVALID_FILE_ATTRIBUTES;
}
