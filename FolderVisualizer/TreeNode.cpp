#include "TreeNode.h"
#include <sstream>
#include <iomanip>

std::string TreeNode::getFormattedSize() const {
    if (isDirectory || fileSize == 0) return "";
    
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = static_cast<double>(fileSize);
    
    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        unit++;
    }
    
    std::ostringstream oss;
    if (unit == 0) {
        oss << static_cast<ULONGLONG>(size) << " " << units[unit];
    } else {
        oss << std::fixed << std::setprecision(2) << size << " " << units[unit];
    }
    return oss.str();
}

std::string TreeNode::getCategoryName() const {
    FileTypeManager& ftm = FileTypeManager::getInstance();
    return ftm.getCategoryName(category);
}
