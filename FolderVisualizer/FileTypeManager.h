#ifndef FILE_TYPE_MANAGER_H
#define FILE_TYPE_MANAGER_H

#include <string>
#include <vector>
#include <map>
#include <set>

/**
 * @brief 文件类型枚举 - 支持细粒度分类
 */
enum class FileCategory {
    UNKNOWN = 0,
    DIRECTORY,
    // 媒体文件
    IMAGE,
    AUDIO,
    VIDEO,
    // 文档
    TEXT,
    DOCUMENT,
    SPREADSHEET,
    PRESENTATION,
    PDF,
    // 代码
    SOURCE_CODE,
    HEADER_FILE,
    MARKUP,
    CONFIG,
    // 归档
    ARCHIVE,
    // 可执行
    EXECUTABLE,
    SCRIPT,
    // 数据库
    DATABASE,
    // 字体
    FONT
};

/**
 * @brief 文件类型信息结构
 */
struct FileTypeInfo {
    FileCategory category;
    std::string displayName;        // 显示名称（如"PNG图片"）
    std::string mimeType;           // MIME类型
    std::vector<std::string> extensions; // 关联的扩展名
    bool isBinary;                  // 是否为二进制文件
};

/**
 * @brief 文件类型管理器 - 单例模式
 * 职责：管理所有文件类型定义、扩展名映射、MIME类型推断
 */
class FileTypeManager {
public:
    static FileTypeManager& getInstance();
    
    // 禁止拷贝
    FileTypeManager(const FileTypeManager&) = delete;
    FileTypeManager& operator=(const FileTypeManager&) = delete;
    
    // 核心查询接口
    FileCategory categorize(const std::string& extension) const;
    FileCategory categorizeByFilename(const std::string& filename) const;
    std::string getCategoryName(FileCategory cat) const;
    std::string getCategoryIconBaseName(FileCategory cat) const; // 返回如"image", "code"
    
    // 扩展名处理工具
    std::string extractExtension(const std::string& filename) const;
    std::string normalizeExtension(const std::string& ext) const; // 统一小写，去掉点
    
    // 批量查询
    std::vector<std::string> getExtensionsForCategory(FileCategory cat) const;
    bool isPreviewable(FileCategory cat) const;      // 是否可预览（图片/文本）
    bool isEditable(FileCategory cat) const;         // 是否可编辑
    
    // 统计信息
    size_t getRegisteredTypeCount() const;
    void printRegistry() const; // 调试输出
    
private:
    FileTypeManager();
    ~FileTypeManager() = default;
    
    void initializeRegistry();
    void registerType(FileCategory cat, const std::string& name, 
                      const std::string& mimeBase,
                      const std::vector<std::string>& exts,
                      bool binary = true);
    
    // 数据结构
    std::map<FileCategory, FileTypeInfo> registry_;
    std::map<std::string, FileCategory> extToCategory_; // 扩展名（小写，无点）-> 类型
    
    // 缓存
    mutable std::map<std::string, FileCategory> cache_; // 文件名->类型缓存
};

#endif // FILE_TYPE_MANAGER_H
