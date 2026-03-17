#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shlobj.h>
#include <string>
#include <memory>
#include <gdiplus.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <map>

#include "resource.h"  // 资源 ID 定义
#include "FileTree.h"
#include "IconManager.h"
#include "FileTypeManager.h"

// 配置结构体
struct AppConfig {
    std::string iconPath = "icons/";
    std::string defaultFolderIcon = "folder.png";
    std::string defaultFileIcon = "file.png";
    std::string windowIcon = "tree.ico";
    std::map<std::string, std::string> extensionIcons;  // 扩展名 -> 图标文件名
    
    void loadFromJson(const std::string& jsonPath);
};

AppConfig g_config;

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

// 控件ID
#define IDC_EDIT_PATH       1001
#define IDC_BTN_BROWSE      1002
#define IDC_BTN_SCAN        1003
#define IDC_BTN_SAVE_TXT    1004
#define IDC_BTN_SAVE_IMG    1005
#define IDC_TREE_VIEW       1006
#define IDC_STATUS_BAR      1007

// 配色
#define COLOR_BG            RGB(245, 246, 250)
#define COLOR_PANEL         RGB(255, 255, 255)
#define COLOR_PRIMARY       RGB(64, 128, 128)
#define COLOR_TEXT          RGB(51, 51, 51)
#define COLOR_TEXT_LIGHT    RGB(128, 128, 128)
#define COLOR_TREE_LINE     RGB(200, 200, 200)

// 全局实例
HINSTANCE g_hInst;
HWND g_hWndMain;
HWND g_hEditPath;
HWND g_hTreeView;
HWND g_hStatusBar;
HFONT g_hFontNormal;
HFONT g_hFontBold;

// 核心对象
std::unique_ptr<FileTree> g_fileTree;
std::unique_ptr<IconManager> g_iconManager;
bool g_hasScanned = false;
int g_scrollY = 0;
int g_totalTreeHeight = 0;

// 函数声明
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK TreeViewProc(HWND, UINT, WPARAM, LPARAM);
void CreateModernUI(HWND hWnd);
void BrowseFolder();
void DoScan();
void DoSaveText();
void DoSaveImage();
void DrawTreeView(HDC hdc);
void UpdateStatus(const std::wstring& msg);
WNDPROC g_oldTreeProc;

// 辅助函数：从 JSON 提取字符串值
std::string extractJsonString(const std::string& json, const std::string& key) {
    size_t keyPos = json.find("\"" + key + "\"");
    if (keyPos == std::string::npos) return "";
    
    size_t colonPos = json.find(":", keyPos);
    if (colonPos == std::string::npos) return "";
    
    size_t quotePos = json.find("\"", colonPos);
    if (quotePos == std::string::npos) return "";
    
    size_t endQuotePos = json.find("\"", quotePos + 1);
    if (endQuotePos == std::string::npos) return "";
    
    return json.substr(quotePos + 1, endQuotePos - quotePos - 1);
}

void AppConfig::loadFromJson(const std::string& jsonPath) {
    std::ifstream file(jsonPath);
    if (!file.is_open()) return;
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();
    
    std::string val;
    if (!(val = extractJsonString(json, "folderVisualizer.iconPath")).empty()) iconPath = val;
    if (!(val = extractJsonString(json, "folderVisualizer.defaultFolderIcon")).empty()) defaultFolderIcon = val;
    if (!(val = extractJsonString(json, "folderVisualizer.defaultFileIcon")).empty()) defaultFileIcon = val;
    if (!(val = extractJsonString(json, "folderVisualizer.windowIcon")).empty()) windowIcon = val;
    
    // 读取自定义扩展名图标（如 defaultcppIcon, defaulthtmlIcon 等）
    // 格式: folderVisualizer.default{扩展名}Icon -> 图标文件名
    // 动态解析：查找所有 folderVisualizer.defaultXXXIcon 格式的键
    size_t searchPos = 0;
    while ((searchPos = json.find("\"folderVisualizer.default", searchPos)) != std::string::npos) {
        // 找到键名的起始和结束位置
        size_t keyStart = searchPos + 1;  // 跳过开头的引号
        size_t keyEnd = json.find("\"", keyStart);
        
        if (keyEnd == std::string::npos) break;
        
        std::string fullKey = json.substr(keyStart, keyEnd - keyStart);
        
        // 检查是否是扩展名图标配置（格式: folderVisualizer.default{扩展名}Icon）
        // 排除 defaultFolderIcon, defaultFileIcon, defaultWindowIcon 等
        const std::string prefix = "folderVisualizer.default";
        const std::string suffix = "Icon";
        
        if (fullKey.length() > prefix.length() + suffix.length() && 
            fullKey.substr(0, prefix.length()) == prefix &&
            fullKey.substr(fullKey.length() - suffix.length()) == suffix) {
            
            // 提取扩展名
            std::string extPart = fullKey.substr(prefix.length(), 
                                                  fullKey.length() - prefix.length() - suffix.length());
            
            // 排除 Folder 和 File（这些是默认图标，不是扩展名）
            if (extPart != "Folder" && extPart != "File" && extPart != "Window") {
                // 提取值
                size_t colonPos = json.find(":", keyEnd);
                if (colonPos != std::string::npos) {
                    size_t valQuoteStart = json.find("\"", colonPos);
                    if (valQuoteStart != std::string::npos) {
                        size_t valQuoteEnd = json.find("\"", valQuoteStart + 1);
                        if (valQuoteEnd != std::string::npos) {
                            std::string iconFile = json.substr(valQuoteStart + 1, 
                                                                valQuoteEnd - valQuoteStart - 1);
                            // 扩展名转为小写
                            std::transform(extPart.begin(), extPart.end(), extPart.begin(), ::tolower);
                            extensionIcons[extPart] = iconFile;
                        }
                    }
                }
            }
        }
        
        searchPos = keyEnd + 1;
    }
}

// 获取程序所在目录
std::string GetExecutableDirectory() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring wPath(path);
    size_t pos = wPath.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        wPath = wPath.substr(0, pos);
    }
    return IconManager::toUtf8String(wPath);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // 加载配置 - 优先从程序目录读取，如果不存在则使用默认值
    std::string exeDir = GetExecutableDirectory();
    std::string configPath = exeDir + "/settings.json";
    
    // 检查程序目录是否有配置文件
    if (!IconManager::fileExists(configPath)) {
        // 回退到开发目录的 .vscode/settings.json
        configPath = ".vscode/settings.json";
    }
    
    g_config.loadFromJson(configPath);
    
    // 初始化GDI+
    Gdiplus::GdiplusStartupInput input;
    ULONG_PTR token;
    Gdiplus::GdiplusStartup(&token, &input, nullptr);
    
    // 初始化通用控件
    INITCOMMONCONTROLSEX iccex = {sizeof(iccex), ICC_WIN95_CLASSES};
    InitCommonControlsEx(&iccex);
    
    // 初始化核心组件
    FileTypeManager::getInstance(); // 确保单例初始化
    g_iconManager = std::make_unique<IconManager>();
    g_iconManager->initialize(g_config.iconPath);
    g_iconManager->setDefaultIconPaths(g_config.defaultFolderIcon, g_config.defaultFileIcon);
    
    // 应用自定义扩展名图标配置
    for (const auto& pair : g_config.extensionIcons) {
        g_iconManager->setExtensionIcon(pair.first, pair.second);
    }
    
    g_hInst = hInstance;
    
    // 加载嵌入的程序图标（从资源文件中）
    HICON hWindowIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
    if (!hWindowIcon) {
        // 如果加载失败，尝试从配置文件指定的文件加载
        std::wstring wIconPath = IconManager::toWideString(g_config.windowIcon);
        if (!g_config.windowIcon.empty() && IconManager::fileExists(g_config.windowIcon)) {
            hWindowIcon = (HICON)LoadImageW(nullptr, wIconPath.c_str(), IMAGE_ICON, 
                                            0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
        }
        if (!hWindowIcon) {
            hWindowIcon = LoadIcon(nullptr, IDI_APPLICATION);
        }
    }
    
    // 注册窗口类
    WNDCLASSEXW wcex = {sizeof(wcex)};
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = hWindowIcon;  // 使用嵌入的图标
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = CreateSolidBrush(COLOR_BG);
    wcex.lpszClassName = L"FolderVisualizerV3";
    
    RegisterClassExW(&wcex);
    
    // 创建主窗口
    g_hWndMain = CreateWindowExW(WS_EX_CLIENTEDGE, L"FolderVisualizerV3",
                                 L"Folder Visualizer",
                                 WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
                                 CW_USEDEFAULT, 0, 1220, 800,
                                 nullptr, nullptr, hInstance, nullptr);
    
    if (!g_hWndMain) return 1;
    
    // 字体
    g_hFontNormal = CreateFontW(14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0,0,0,0, L"Microsoft YaHei");
    g_hFontBold = CreateFontW(16, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0,0,0,0, L"Microsoft YaHei");
    
    CreateModernUI(g_hWndMain);
    
    ShowWindow(g_hWndMain, nCmdShow);
    UpdateWindow(g_hWndMain);
    
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // 清理
    g_fileTree.reset();
    g_iconManager.reset();
    Gdiplus::GdiplusShutdown(token);
    return (int)msg.wParam;
}

void CreateModernUI(HWND hWnd) {
    // 标题
    HWND hTitle = CreateWindowW(L"STATIC", L"📁 Folder Visualizer Professional",
                                WS_VISIBLE | WS_CHILD | SS_CENTER,
                                0, 15, 1200, 30, hWnd, nullptr, g_hInst, nullptr);
    SendMessage(hTitle, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);
    
    // 路径输入区
    CreateWindowW(L"STATIC", L"路径:", WS_VISIBLE | WS_CHILD,
                  20, 60, 40, 25, hWnd, nullptr, g_hInst, nullptr);
    
    g_hEditPath = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                  WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL,
                                  70, 57, 750, 26, hWnd, (HMENU)IDC_EDIT_PATH, g_hInst, nullptr);
    SendMessage(g_hEditPath, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    
    // 按钮
    CreateWindowW(L"BUTTON", L"浏览...", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                  830, 57, 80, 26, hWnd, (HMENU)IDC_BTN_BROWSE, g_hInst, nullptr);
    CreateWindowW(L"BUTTON", L"开始扫描", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                  920, 57, 100, 26, hWnd, (HMENU)IDC_BTN_SCAN, g_hInst, nullptr);
    CreateWindowW(L"BUTTON", L"保存TXT", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                  1030, 57, 80, 26, hWnd, (HMENU)IDC_BTN_SAVE_TXT, g_hInst, nullptr);
    CreateWindowW(L"BUTTON", L"保存图片", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                  1120, 57, 80, 26, hWnd, (HMENU)IDC_BTN_SAVE_IMG, g_hInst, nullptr);
    
    // 树形视图区域（自绘制）
    g_hTreeView = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"",
                                  WS_VISIBLE | WS_CHILD | SS_OWNERDRAW,
                                  20, 100, 1140, 620, hWnd, (HMENU)IDC_TREE_VIEW, g_hInst, nullptr);
    g_oldTreeProc = (WNDPROC)SetWindowLongPtr(g_hTreeView, GWLP_WNDPROC, (LONG_PTR)TreeViewProc);
    
    // 状态栏
    g_hStatusBar = CreateWindowW(L"STATIC", L"就绪 - 请选择文件夹",
                                 WS_VISIBLE | WS_CHILD | SS_LEFT,
                                 20, 730, 1140, 22, hWnd, (HMENU)IDC_STATUS_BAR, g_hInst, nullptr);
    SendMessage(g_hStatusBar, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_BTN_BROWSE: BrowseFolder(); break;
        case IDC_BTN_SCAN:   DoScan(); break;
        case IDC_BTN_SAVE_TXT: DoSaveText(); break;
        case IDC_BTN_SAVE_IMG: DoSaveImage(); break;
        }
        return 0;
        
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK TreeViewProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        DrawTreeView(hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        g_scrollY -= delta / 30;
        if (g_scrollY < 0) g_scrollY = 0;
        if (g_totalTreeHeight > 0) {
            int maxScroll = std::max(0, g_totalTreeHeight - 600);
            if (g_scrollY > maxScroll) g_scrollY = maxScroll;
        }
        InvalidateRect(hWnd, nullptr, TRUE);
        return 0;
    }
    }
    return CallWindowProc(g_oldTreeProc, hWnd, msg, wParam, lParam);
}

void BrowseFolder() {
    BROWSEINFOW bi = {0};
    bi.lpszTitle = L"选择要可视化的文件夹";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) {
            SetWindowTextW(g_hEditPath, path);
        }
        CoTaskMemFree(pidl);
    }
}

void DoScan() {
    wchar_t wPath[MAX_PATH];
    GetWindowTextW(g_hEditPath, wPath, MAX_PATH);
    
    if (wcslen(wPath) == 0) {
        MessageBoxW(g_hWndMain, L"请先选择文件夹", L"提示", MB_OK);
        return;
    }
    
    std::string path = IconManager::toUtf8String(wPath);
    
    UpdateStatus(L"正在扫描...");
    
    g_fileTree = std::make_unique<FileTree>();
    g_fileTree->setIconManager(g_iconManager.get());
    g_fileTree->setRootPath(path);  // 设置扫描路径
    
    ScanOptions opts;
    opts.includeHidden = false;
    opts.readDescriptions = true;
    opts.readDirectoryThumbnails = true;
    g_fileTree->setOptions(opts);
    
    // 带进度回调的扫描
    bool success = g_fileTree->scanWithProgress([](const std::string& cur, size_t count) {
        std::wstring msg = L"已扫描: " + std::to_wstring(count) + L" 项";
        UpdateStatus(msg);
    });
    
    if (!success) {
        MessageBoxW(g_hWndMain, L"扫描失败，请检查路径", L"错误", MB_OK);
        g_fileTree.reset();
        return;
    }
    
    g_hasScanned = true;
    g_scrollY = 0;
    
    // 计算总高度用于滚动
    g_totalTreeHeight = 0;
    std::function<void(const TreeNode&)> calcHeight = [&](const TreeNode& n) {
        g_totalTreeHeight += 24;
        for (const auto& c : n.children) calcHeight(c);
    };
    calcHeight(g_fileTree->getRoot());
    
    InvalidateRect(g_hTreeView, nullptr, TRUE);
    
    // 统计信息
    auto stats = g_fileTree->getCategoryStatistics();
    std::wstring status = L"扫描完成: " + std::to_wstring(g_fileTree->getTotalNodeCount()) + 
                         L" 项 (目录: " + std::to_wstring(g_fileTree->getDirectoryCount()) +
                         L", 文件: " + std::to_wstring(g_fileTree->getFileCount()) + L")";
    UpdateStatus(status);
}

// 辅助函数：检查是否可以使用折叠显示（连续相同扩展名的文件）
struct FoldInfo {
    bool shouldFold;
    int foldCount;      // 折叠的文件数量
    std::string firstName;
    std::string lastName;
};

FoldInfo checkFold(const std::vector<TreeNode>& children, size_t startIdx) {
    FoldInfo info = {false, 1, "", ""};
    if (startIdx >= children.size()) return info;
    
    const TreeNode& first = children[startIdx];
    if (first.isDirectory || first.extension.empty()) return info;
    
    std::string ext = first.extension;
    size_t count = 1;
    size_t lastIdx = startIdx;
    
    // 向后查找连续相同扩展名的文件
    for (size_t i = startIdx + 1; i < children.size(); ++i) {
        if (!children[i].isDirectory && children[i].extension == ext) {
            count++;
            lastIdx = i;
        } else {
            break;
        }
    }
    
    // 如果超过3个相同扩展名，启用折叠
    if (count >= 3) {
        info.shouldFold = true;
        info.foldCount = static_cast<int>(count);
        info.firstName = first.name;
        info.lastName = children[lastIdx].name;
    }
    
    return info;
}

void DrawTreeView(HDC hdc) {
    if (!g_hasScanned || !g_fileTree) return;
    
    RECT rect;
    GetClientRect(g_hTreeView, &rect);
    FillRect(hdc, &rect, CreateSolidBrush(COLOR_PANEL));
    
    SetBkMode(hdc, TRANSPARENT);
    
    int y = 10 - g_scrollY;
    int x = 10;
    
    std::function<void(const TreeNode&, int, int&)> draw = 
        [&](const TreeNode& node, int depth, int& curY) {
        
        const int LINE_H = 24;
        const int INDENT = 24;
        const int ICON_SZ = 16;
        
        int curX = x + depth * INDENT;
        
        // 绘制连接线
        if (depth > 0) {
            HPEN pen = CreatePen(PS_SOLID, 1, COLOR_TREE_LINE);
            HPEN oldPen = (HPEN)SelectObject(hdc, pen);
            
            int lineX = curX - 12;
            MoveToEx(hdc, lineX, curY + 8, nullptr);
            LineTo(hdc, lineX + 8, curY + 8); // 横线
            
            if (!node.isLast) {
                MoveToEx(hdc, lineX, curY + 8, nullptr);
                LineTo(hdc, lineX, curY + LINE_H); // 竖线
            }
            
            SelectObject(hdc, oldPen);
            DeleteObject(pen);
        }
        
        // 绘制图标
        if (g_iconManager) {
            g_iconManager->drawIconForFile(hdc, curX + 2, curY + 2, 
                                           node.name, node.isDirectory, 
                                           IconSize::SMALL);
        }
        
        // 绘制文字
        if (node.isDirectory) {
            SetTextColor(hdc, COLOR_PRIMARY);
            SelectObject(hdc, g_hFontBold);
        } else {
            SetTextColor(hdc, COLOR_TEXT);
            SelectObject(hdc, g_hFontNormal);
        }
        
        std::wstring name = IconManager::toWideString(node.name);
        if (node.isDirectory) name += L"/";
        
        int textX = curX + 22;
        TextOutW(hdc, textX, curY + 2, name.c_str(), (int)name.length());
        
        // 元数据（类型、大小）
        if (!node.isDirectory) {
            std::string meta = " [" + node.getCategoryName();
            if (!node.extension.empty()) {
                meta += "." + node.extension;
            }
            if (node.fileSize > 0) {
                meta += ", " + node.getFormattedSize();
            }
            meta += "]";
            
            SetTextColor(hdc, COLOR_TEXT_LIGHT);
            SelectObject(hdc, g_hFontNormal);
            
            SIZE nameSize;
            GetTextExtentPoint32W(hdc, name.c_str(), (int)name.length(), &nameSize);
            
            std::wstring wMeta = IconManager::toWideString(meta);
            TextOutW(hdc, textX + nameSize.cx + 5, curY + 2, 
                    wMeta.c_str(), (int)wMeta.length());
        }
        
        // 描述
        if (!node.description.empty()) {
            SIZE totalSize;
            std::wstring fullText = name + (node.isDirectory ? L"" : 
                IconManager::toWideString(" [" + node.getCategoryName() + "]"));
            GetTextExtentPoint32W(hdc, fullText.c_str(), (int)fullText.length(), &totalSize);
            
            SetTextColor(hdc, RGB(100, 100, 100));
            std::wstring desc = L"  <- " + IconManager::toWideString(node.description);
            TextOutW(hdc, textX + totalSize.cx + 10, curY + 2, 
                    desc.c_str(), (int)desc.length());
        }
        
        curY += LINE_H;
        
        // 子节点（带折叠逻辑）
        for (size_t i = 0; i < node.children.size(); ) {
            FoldInfo fold = checkFold(node.children, i);
            
            if (fold.shouldFold) {
                // 列式折叠：显示第一个文件、省略号行、最后一个文件
                const TreeNode& first = node.children[i];
                const TreeNode& last = node.children[i + fold.foldCount - 1];
                int childCurX = curX + INDENT;
                
                // 1. 绘制第一个文件
                {
                    // 连接线
                    HPEN pen = CreatePen(PS_SOLID, 1, COLOR_TREE_LINE);
                    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
                    int lineX = childCurX - 12;
                    MoveToEx(hdc, lineX, curY + 8, nullptr);
                    LineTo(hdc, lineX + 8, curY + 8);
                    MoveToEx(hdc, lineX, curY + 8, nullptr);
                    LineTo(hdc, lineX, curY + LINE_H);
                    SelectObject(hdc, oldPen);
                    DeleteObject(pen);
                    
                    // 图标
                    if (g_iconManager) {
                        g_iconManager->drawIconForFile(hdc, childCurX + 2, curY + 2,
                                                       first.name, false, IconSize::SMALL);
                    }
                    
                    // 文件名
                    SetTextColor(hdc, COLOR_TEXT);
                    SelectObject(hdc, g_hFontNormal);
                    std::wstring name = IconManager::toWideString(first.name);
                    TextOutW(hdc, childCurX + 22, curY + 2, name.c_str(), (int)name.length());
                    
                    // 元数据
                    std::string meta = " [" + first.getCategoryName();
                    if (!first.extension.empty()) meta += "." + first.extension;
                    meta += "]";
                    SetTextColor(hdc, COLOR_TEXT_LIGHT);
                    SIZE nameSize;
                    GetTextExtentPoint32W(hdc, name.c_str(), (int)name.length(), &nameSize);
                    std::wstring wMeta = IconManager::toWideString(meta);
                    TextOutW(hdc, childCurX + 22 + nameSize.cx + 5, curY + 2,
                            wMeta.c_str(), (int)wMeta.length());
                    
                    curY += LINE_H;
                }
                
                // 2. 绘制省略号行
                {
                    // 连接线（竖线继续）
                    HPEN pen = CreatePen(PS_SOLID, 1, COLOR_TREE_LINE);
                    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
                    int lineX = childCurX - 12;
                    MoveToEx(hdc, lineX, curY, nullptr);
                    LineTo(hdc, lineX, curY + LINE_H);
                    SelectObject(hdc, oldPen);
                    DeleteObject(pen);
                    
                    // 省略号文字
                    SetTextColor(hdc, RGB(150, 150, 150));
                    SelectObject(hdc, g_hFontNormal);
                    std::wstring dots = L"│  ... (" + std::to_wstring(fold.foldCount - 2) + 
                                       L" more ." + IconManager::toWideString(first.extension) + L" files)";
                    TextOutW(hdc, childCurX + 22, curY + 2, dots.c_str(), (int)dots.length());
                    
                    curY += LINE_H;
                }
                
                // 3. 绘制最后一个文件
                {
                    // 连接线（最后一根横线）
                    HPEN pen = CreatePen(PS_SOLID, 1, COLOR_TREE_LINE);
                    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
                    int lineX = childCurX - 12;
                    MoveToEx(hdc, lineX, curY + 8, nullptr);
                    LineTo(hdc, lineX + 8, curY + 8);
                    if (!last.isLast) {
                        MoveToEx(hdc, lineX, curY + 8, nullptr);
                        LineTo(hdc, lineX, curY + LINE_H);
                    }
                    SelectObject(hdc, oldPen);
                    DeleteObject(pen);
                    
                    // 图标
                    if (g_iconManager) {
                        g_iconManager->drawIconForFile(hdc, childCurX + 2, curY + 2,
                                                       last.name, false, IconSize::SMALL);
                    }
                    
                    // 文件名
                    SetTextColor(hdc, COLOR_TEXT);
                    SelectObject(hdc, g_hFontNormal);
                    std::wstring name = IconManager::toWideString(last.name);
                    TextOutW(hdc, childCurX + 22, curY + 2, name.c_str(), (int)name.length());
                    
                    // 元数据
                    std::string meta = " [" + last.getCategoryName();
                    if (!last.extension.empty()) meta += "." + last.extension;
                    meta += "]";
                    SetTextColor(hdc, COLOR_TEXT_LIGHT);
                    SIZE nameSize;
                    GetTextExtentPoint32W(hdc, name.c_str(), (int)name.length(), &nameSize);
                    std::wstring wMeta = IconManager::toWideString(meta);
                    TextOutW(hdc, childCurX + 22 + nameSize.cx + 5, curY + 2,
                            wMeta.c_str(), (int)wMeta.length());
                    
                    curY += LINE_H;
                }
                
                i += fold.foldCount;
            } else {
                draw(node.children[i], depth + 1, curY);
                i++;
            }
        }
    };
    
    int dummyY = y;
    draw(g_fileTree->getRoot(), 0, dummyY);
}

void DoSaveText() {
    if (!g_hasScanned || !g_fileTree) {
        MessageBoxW(g_hWndMain, L"请先扫描文件夹", L"提示", MB_OK);
        return;
    }
    
    wchar_t filename[MAX_PATH] = {0};
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hWndMain;
    ofn.lpstrFilter = L"文本文件 (*.txt)\0*.txt\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"txt";
    ofn.Flags = OFN_OVERWRITEPROMPT;
    
    if (GetSaveFileNameW(&ofn)) {
        std::string path = IconManager::toUtf8String(filename);
        bool ok = g_fileTree->saveToTextFile(path, true, true, true);
        
        if (ok) {
            UpdateStatus(std::wstring(L"已保存: ") + filename);
        } else {
            MessageBoxW(g_hWndMain, L"保存失败", L"错误", MB_OK);
        }
    }
}

void DoSaveImage() {
    if (!g_hasScanned || !g_fileTree) {
        MessageBoxW(g_hWndMain, L"请先扫描文件夹", L"提示", MB_OK);
        return;
    }
    
    // 高分辨率设置：2 倍缩放
    const int SCALE = 2;
    
    // 计算所需尺寸（2 倍分辨率）
    int width = 1600 * SCALE;  // 增加宽度
    int height = std::max(800, g_totalTreeHeight + 150) * SCALE;
    
    // 创建内存 DC
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    
    // 创建高分辨率 DIB 位图（支持 ARGB）
    BITMAPINFOHEADER bi = {sizeof(BITMAPINFOHEADER), width, -height, 1, 32, BI_RGB, 0, 0, 0, 0, 0};
    void* bits;
    HBITMAP hBmp = CreateDIBSection(hdcScreen, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    SelectObject(hdcMem, hBmp);
    
    // 设置高质量绘图模式
    SetGraphicsMode(hdcMem, GM_ADVANCED);
    SetStretchBltMode(hdcMem, HALFTONE);
    
    // 绘制背景
    RECT bgRect = {0, 0, width, height};
    FillRect(hdcMem, &bgRect, CreateSolidBrush(COLOR_BG));
    
    // 标题（放大字体）
    HFONT titleFont = CreateFontW(20 * SCALE, 0, 0, 0, FW_BOLD, 0,0,0, DEFAULT_CHARSET, 0,0,0,0, L"Microsoft YaHei");
    SelectObject(hdcMem, titleFont);
    SetTextColor(hdcMem, COLOR_PRIMARY);
    std::wstring title = L"📁 " + IconManager::toWideString(g_fileTree->getRoot().name);
    TextOutW(hdcMem, 20 * SCALE, 20 * SCALE, title.c_str(), (int)title.length());
    DeleteObject(titleFont);
    
    // 绘制树（完整，无滚动）
    int y = 60 * SCALE;
    std::function<void(const TreeNode&, int, int&)> draw = 
        [&](const TreeNode& node, int depth, int& curY) {
        
        const int LINE_H = 24 * SCALE;
        const int INDENT = 24 * SCALE;
        int curX = 20 * SCALE + depth * INDENT;
        
        // 连接线（加粗）
        if (depth > 0) {
            HPEN pen = CreatePen(PS_SOLID, 2 * SCALE, COLOR_TREE_LINE);
            HPEN oldPen = (HPEN)SelectObject(hdcMem, pen);
            int lineX = curX - 12 * SCALE;
            MoveToEx(hdcMem, lineX, curY + 8 * SCALE, nullptr);
            LineTo(hdcMem, lineX + 8 * SCALE, curY + 8 * SCALE);
            if (!node.isLast) {
                MoveToEx(hdcMem, lineX, curY + 8 * SCALE, nullptr);
                LineTo(hdcMem, lineX, curY + LINE_H);
            }
            SelectObject(hdcMem, oldPen);
            DeleteObject(pen);
        }
        
        // 图标（使用中等尺寸）
        if (g_iconManager) {
            g_iconManager->drawIconForFile(hdcMem, curX + 2 * SCALE, curY + 2 * SCALE,
                                           node.name, node.isDirectory, IconSize::MEDIUM);
        }
        
        // 文字（使用更大字体）
        SetBkMode(hdcMem, TRANSPARENT);
        HFONT scaledFont = CreateFontW(14 * SCALE, 0, 0, 0, FW_NORMAL, 0,0,0, DEFAULT_CHARSET, 0,0,0,0, L"Microsoft YaHei");
        HFONT scaledBoldFont = CreateFontW(16 * SCALE, 0, 0, 0, FW_BOLD, 0,0,0, DEFAULT_CHARSET, 0,0,0,0, L"Microsoft YaHei");
        
        if (node.isDirectory) {
            SetTextColor(hdcMem, COLOR_PRIMARY);
            SelectObject(hdcMem, scaledBoldFont);
        } else {
            SetTextColor(hdcMem, COLOR_TEXT);
            SelectObject(hdcMem, scaledFont);
        }
        
        std::wstring name = IconManager::toWideString(node.name);
        if (node.isDirectory) name += L"/";
        TextOutW(hdcMem, curX + 22 * SCALE, curY + 2 * SCALE, name.c_str(), (int)name.length());
        
        // 元数据
        if (!node.isDirectory) {
            std::string meta = " [" + node.getCategoryName();
            if (!node.extension.empty()) {
                meta += "." + node.extension;
            }
            if (node.fileSize > 0) {
                meta += ", " + node.getFormattedSize();
            }
            meta += "]";
            
            SIZE nameSize;
            GetTextExtentPoint32W(hdcMem, name.c_str(), (int)name.length(), &nameSize);
            SetTextColor(hdcMem, COLOR_TEXT_LIGHT);
            std::wstring wMeta = IconManager::toWideString(meta);
            TextOutW(hdcMem, curX + 22 * SCALE + nameSize.cx + 5 * SCALE, curY + 2 * SCALE, 
                    wMeta.c_str(), (int)wMeta.length());
        }
        
        // 描述
        if (!node.description.empty()) {
            SIZE totalSize;
            std::wstring fullText = name + (node.isDirectory ? L"" : 
                IconManager::toWideString(" [" + node.getCategoryName() + "]"));
            GetTextExtentPoint32W(hdcMem, fullText.c_str(), (int)fullText.length(), &totalSize);
            
            SetTextColor(hdcMem, RGB(100, 100, 100));
            std::wstring desc = L"  <- " + IconManager::toWideString(node.description);
            TextOutW(hdcMem, curX + 22 * SCALE + totalSize.cx + 10 * SCALE, curY + 2 * SCALE, 
                    desc.c_str(), (int)desc.length());
        }
        
        DeleteObject(scaledFont);
        DeleteObject(scaledBoldFont);
        
        curY += LINE_H;
        
        // 子节点（带折叠逻辑）
        for (size_t i = 0; i < node.children.size(); ) {
            FoldInfo fold = checkFold(node.children, i);
            
            if (fold.shouldFold) {
                // 列式折叠（高分辨率版本）
                const TreeNode& first = node.children[i];
                const TreeNode& last = node.children[i + fold.foldCount - 1];
                int childCurX = curX + INDENT;
                
                // 1. 绘制第一个文件
                {
                    HPEN pen = CreatePen(PS_SOLID, 2 * SCALE, COLOR_TREE_LINE);
                    HPEN oldPen = (HPEN)SelectObject(hdcMem, pen);
                    int lineX = childCurX - 12 * SCALE;
                    MoveToEx(hdcMem, lineX, curY + 8 * SCALE, nullptr);
                    LineTo(hdcMem, lineX + 8 * SCALE, curY + 8 * SCALE);
                    MoveToEx(hdcMem, lineX, curY + 8 * SCALE, nullptr);
                    LineTo(hdcMem, lineX, curY + LINE_H);
                    SelectObject(hdcMem, oldPen);
                    DeleteObject(pen);
                    
                    if (g_iconManager) {
                        g_iconManager->drawIconForFile(hdcMem, childCurX + 2 * SCALE, curY + 2 * SCALE,
                                                       first.name, false, IconSize::MEDIUM);
                    }
                    
                    SetBkMode(hdcMem, TRANSPARENT);
                    HFONT scaledFont = CreateFontW(14 * SCALE, 0, 0, 0, FW_NORMAL, 0,0,0, DEFAULT_CHARSET, 0,0,0,0, L"Microsoft YaHei");
                    SelectObject(hdcMem, scaledFont);
                    SetTextColor(hdcMem, COLOR_TEXT);
                    
                    std::wstring name = IconManager::toWideString(first.name);
                    TextOutW(hdcMem, childCurX + 22 * SCALE, curY + 2 * SCALE, 
                            name.c_str(), (int)name.length());
                    
                    std::string meta = " [" + first.getCategoryName();
                    if (!first.extension.empty()) meta += "." + first.extension;
                    meta += "]";
                    SetTextColor(hdcMem, COLOR_TEXT_LIGHT);
                    SIZE nameSize;
                    GetTextExtentPoint32W(hdcMem, name.c_str(), (int)name.length(), &nameSize);
                    std::wstring wMeta = IconManager::toWideString(meta);
                    TextOutW(hdcMem, childCurX + 22 * SCALE + nameSize.cx + 5 * SCALE, curY + 2 * SCALE,
                            wMeta.c_str(), (int)wMeta.length());
                    
                    DeleteObject(scaledFont);
                    curY += LINE_H;
                }
                
                // 2. 绘制省略号行
                {
                    HPEN pen = CreatePen(PS_SOLID, 2 * SCALE, COLOR_TREE_LINE);
                    HPEN oldPen = (HPEN)SelectObject(hdcMem, pen);
                    int lineX = childCurX - 12 * SCALE;
                    MoveToEx(hdcMem, lineX, curY, nullptr);
                    LineTo(hdcMem, lineX, curY + LINE_H);
                    SelectObject(hdcMem, oldPen);
                    DeleteObject(pen);
                    
                    HFONT scaledFont = CreateFontW(14 * SCALE, 0, 0, 0, FW_NORMAL, 0,0,0, DEFAULT_CHARSET, 0,0,0,0, L"Microsoft YaHei");
                    SelectObject(hdcMem, scaledFont);
                    SetTextColor(hdcMem, RGB(150, 150, 150));
                    
                    std::wstring dots = L"│  ... (" + std::to_wstring(fold.foldCount - 2) + 
                                       L" more ." + IconManager::toWideString(first.extension) + L" files)";
                    TextOutW(hdcMem, childCurX + 22 * SCALE, curY + 2 * SCALE, 
                            dots.c_str(), (int)dots.length());
                    
                    DeleteObject(scaledFont);
                    curY += LINE_H;
                }
                
                // 3. 绘制最后一个文件
                {
                    HPEN pen = CreatePen(PS_SOLID, 2 * SCALE, COLOR_TREE_LINE);
                    HPEN oldPen = (HPEN)SelectObject(hdcMem, pen);
                    int lineX = childCurX - 12 * SCALE;
                    MoveToEx(hdcMem, lineX, curY + 8 * SCALE, nullptr);
                    LineTo(hdcMem, lineX + 8 * SCALE, curY + 8 * SCALE);
                    if (!last.isLast) {
                        MoveToEx(hdcMem, lineX, curY + 8 * SCALE, nullptr);
                        LineTo(hdcMem, lineX, curY + LINE_H);
                    }
                    SelectObject(hdcMem, oldPen);
                    DeleteObject(pen);
                    
                    if (g_iconManager) {
                        g_iconManager->drawIconForFile(hdcMem, childCurX + 2 * SCALE, curY + 2 * SCALE,
                                                       last.name, false, IconSize::MEDIUM);
                    }
                    
                    HFONT scaledFont = CreateFontW(14 * SCALE, 0, 0, 0, FW_NORMAL, 0,0,0, DEFAULT_CHARSET, 0,0,0,0, L"Microsoft YaHei");
                    SelectObject(hdcMem, scaledFont);
                    SetTextColor(hdcMem, COLOR_TEXT);
                    
                    std::wstring name = IconManager::toWideString(last.name);
                    TextOutW(hdcMem, childCurX + 22 * SCALE, curY + 2 * SCALE, 
                            name.c_str(), (int)name.length());
                    
                    std::string meta = " [" + last.getCategoryName();
                    if (!last.extension.empty()) meta += "." + last.extension;
                    meta += "]";
                    SetTextColor(hdcMem, COLOR_TEXT_LIGHT);
                    SIZE nameSize;
                    GetTextExtentPoint32W(hdcMem, name.c_str(), (int)name.length(), &nameSize);
                    std::wstring wMeta = IconManager::toWideString(meta);
                    TextOutW(hdcMem, childCurX + 22 * SCALE + nameSize.cx + 5 * SCALE, curY + 2 * SCALE,
                            wMeta.c_str(), (int)wMeta.length());
                    
                    DeleteObject(scaledFont);
                    curY += LINE_H;
                }
                
                i += fold.foldCount;
            } else {
                draw(node.children[i], depth + 1, curY);
                i++;
            }
        }
    };
    
    int dummyY = y;
    draw(g_fileTree->getRoot(), 0, dummyY);
    
    // 保存为PNG
    Gdiplus::Bitmap bitmap(hBmp, nullptr);
    CLSID pngClsid;
    UINT num = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    Gdiplus::ImageCodecInfo* codecs = (Gdiplus::ImageCodecInfo*)malloc(size);
    Gdiplus::GetImageEncoders(num, size, codecs);
    
    for (UINT i = 0; i < num; i++) {
        if (wcscmp(codecs[i].MimeType, L"image/png") == 0) {
            pngClsid = codecs[i].Clsid;
            break;
        }
    }
    free(codecs);
    
    wchar_t filename[MAX_PATH] = {0};
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hWndMain;
    ofn.lpstrFilter = L"PNG图片 (*.png)\0*.png\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"png";
    ofn.Flags = OFN_OVERWRITEPROMPT;
    
    if (GetSaveFileNameW(&ofn)) {
        bitmap.Save(filename, &pngClsid, nullptr);
        UpdateStatus(std::wstring(L"已保存图片: ") + filename);
    }
    
    DeleteDC(hdcMem);
    DeleteObject(hBmp);
    ReleaseDC(nullptr, hdcScreen);
}

void UpdateStatus(const std::wstring& msg) {
    SetWindowTextW(g_hStatusBar, msg.c_str());
}
