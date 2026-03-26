#ifndef TREE_NODE_H
#define TREE_NODE_H

#include <vector>
#include <string>
#include <windows.h>
#include "FileTypeManager.h"

/**
 * @brief 树节点数据结构 - 纯数据，无逻辑
 */
struct TreeNode {
    // 基本信息
    std::string name;           // 文件名
    std::string fullPath;       // 完整路径（UTF-8）
    bool isDirectory;
    int depth;
    bool isLast;
    
    // 文件属性
    FileCategory category;
    std::string extension;      // 小写，无点
    ULONGLONG fileSize;
    FILETIME lastModified;
    DWORD attributes;
    
    // 可视化数据
    std::string displayIcon;    // 实际使用的图标路径（运行时填充）
    std::string description;    // 额外描述（如info.txt内容）
    
    // 树结构
    std::vector<TreeNode> children;
    
    TreeNode() 
        : isDirectory(false), depth(0), isLast(false),
          category(FileCategory::UNKNOWN), fileSize(0),
          attributes(0) {
        lastModified.dwHighDateTime = 0;
        lastModified.dwLowDateTime = 0;
    }
    
    // 工具方法
    bool hasChildren() const { return !children.empty(); }
    std::string getFormattedSize() const;
    std::string getCategoryName() const;
};

#endif // TREE_NODE_H
