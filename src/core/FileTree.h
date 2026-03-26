#ifndef FILE_TREE_H
#define FILE_TREE_H

#include "core/TreeNode.h"
#include "services/IconManager.h"
#include <functional>

/**
 * @brief 扫描选项
 */
struct ScanOptions {
    bool includeHidden = false;         // 包含隐藏文件
    bool includeSystem = false;         // 包含系统文件
    bool readDirectoryThumbnails = true; // 读取cover.jpg等
    bool readDescriptions = true;        // 读取info.txt
    size_t maxDepth = 100;              // 最大递归深度
    size_t maxFilesPerDir = 10000;      // 每目录最大文件数（防卡顿）
    
    bool skipDirectory(const std::string& name) const;
};

/**
 * @brief 扫描进度回调
 */
using ScanProgressCallback = std::function<void(const std::string& currentPath, 
                                                size_t processedCount)>;

/**
 * @brief 文件树扫描器 - 职责：目录遍历与节点构建
 */
class FileTree {
public:
    FileTree();
    ~FileTree();
    
    // 禁止拷贝
    FileTree(const FileTree&) = delete;
    FileTree& operator=(const FileTree&) = delete;
    
    // 配置
    void setRootPath(const std::string& path);
    void setIconManager(IconManager* iconMgr); // 可选，用于填充图标路径
    void setOptions(const ScanOptions& options);
    
    // 扫描
    bool scan();
    bool scan(const std::string& path); // 便捷方法
    
    // 带进度回调的扫描
    bool scanWithProgress(ScanProgressCallback callback);
    
    // 数据访问
    const TreeNode& getRoot() const { return root_; }
    TreeNode& getRoot() { return root_; }
    size_t getTotalNodeCount() const { return nodeCount_; }
    size_t getFileCount() const { return fileCount_; }
    size_t getDirectoryCount() const { return dirCount_; }
    
    // 导出
    std::string exportToText(bool includeIcons = true, 
                            bool includeSize = true,
                            bool includeCategory = true) const;
    bool saveToTextFile(const std::string& outputPath,
                       bool includeIcons = true,
                       bool includeSize = true,
                       bool includeCategory = true) const;
    
    // 统计
    std::map<FileCategory, size_t> getCategoryStatistics() const;
    
    // 工具
    static std::string formatFileTime(const FILETIME& ft);
    static bool isValidPath(const std::string& path);

private:
    TreeNode root_;
    std::string rootPath_;
    size_t nodeCount_;
    size_t fileCount_;
    size_t dirCount_;
    ScanOptions options_;
    IconManager* iconMgr_;
    ScanProgressCallback progressCallback_;
    
    // 内部扫描
    void scanRecursive(const std::string& path, TreeNode& parent, int depth);
    void processNode(TreeNode& node, const WIN32_FIND_DATAW& findData);
    void sortChildren(TreeNode& node);
    void fillIconPath(TreeNode& node);
    
    // 特殊文件处理
    void readDirectoryInfo(TreeNode& node);
    void findCoverImage(TreeNode& node);
    
    // 文本生成
    void generateTextRecursive(const TreeNode& node, 
                               std::ostringstream& oss,
                               const std::string& prefix,
                               bool includeIcons,
                               bool includeSize,
                               bool includeCategory) const;
};

#endif // FILE_TREE_H
