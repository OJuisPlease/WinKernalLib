#include "pch.h"
#include "FilePage.xaml.h"
#if __has_include("FilePage.g.cpp")
#include "FilePage.g.cpp"
#endif

#include <chrono>
#include <shellapi.h>
#include <CopyFileDialog.xaml.h>
#include <unordered_set>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>

using namespace winrt;
using namespace WinUI3Package;
using namespace Microsoft::UI::Text;
using namespace Microsoft::UI::Xaml;

namespace winrt::StarlightGUI::implementation
{
    FilePage* g_filePageInstance = nullptr;

    template <typename T>
    T FindVisualChild(winrt::Microsoft::UI::Xaml::DependencyObject const& parent)
    {
        if (!parent) return nullptr;

        int count = winrt::Microsoft::UI::Xaml::Media::VisualTreeHelper::GetChildrenCount(parent);
        for (int i = 0; i < count; ++i) {
            auto child = winrt::Microsoft::UI::Xaml::Media::VisualTreeHelper::GetChild(parent, i);
            if (auto typed = child.try_as<T>()) return typed;
            if (auto nested = FindVisualChild<T>(child)) return nested;
        }
        return nullptr;
    }

	hstring currentDirectory = L"C:\\";
    static hstring safeAcceptedName = L"";
    static std::unordered_map<std::wstring, winrt::Microsoft::UI::Xaml::Media::ImageSource> iconCache;
    static std::unordered_set<std::wstring> iconLoadingKeys;
    static std::unordered_map<std::wstring, std::vector<winrt::StarlightGUI::FileInfo>> iconPendingFiles;
    static HDC hdc{ nullptr };

    static bool IsFastIconCacheExtension(std::wstring ext)
    {
        std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
        static const std::unordered_set<std::wstring> fastExts = {
            L".txt", L".log", L".ini", L".inf", L".cfg", L".conf",
            L".json", L".xml", L".yaml", L".yml", L".csv",
            L".dll", L".sys", L".mui", L"bin",
            L".dat", L".bak", L".tmp",
            L".reg", L".md", L".bat", L".cmd", L".ps1", L".vbs", L".js", L".jse", L".wsf",
			L".c", L".cpp", L".java", L".kt", L".cs", L".h", L".hpp", L".py", L".rb", L".go", L".rs"
        };
        return fastExts.find(ext) != fastExts.end();
    }

    FilePage::FilePage() {
        InitializeComponent();
        g_filePageInstance = this;

        hdc = GetDC(NULL);
        FileListView().ItemsSource(m_fileList);

        this->Loaded([this](auto&&, auto&&) {
            g_filePageInstance = this;
            LoadFileList();
            });

        this->Unloaded([this](auto&&, auto&&) {
            if (g_filePageInstance == this) g_filePageInstance = nullptr;
            ReleaseDC(NULL, hdc);
            });

        LOG_INFO(L"FilePage", L"FilePage initialized.");
    }

    void FilePage::HandleExternalDropFiles(std::vector<std::wstring> const& paths)
    {
        if (paths.empty()) return;

        auto lifetime = get_strong();
        CopyDroppedPathsAsync(paths);
    }

    void FilePage::FileListView_RightTapped(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const& e)
    {
        auto listView = FileListView();

        slg::SelectItemOnRightTapped(listView, e, true);

        if (!listView.SelectedItems() || listView.SelectedItems().Size() == 0 ||
            // 只选择"上个文件夹"时不显示，多选的话在下面跳过处理，认为是误选
            (listView.SelectedItems().Size() == 1 && listView.SelectedItems().GetAt(0).as<winrt::StarlightGUI::FileInfo>().Flag() == 999))
            return;

        auto list = listView.SelectedItems();

        std::vector<winrt::StarlightGUI::FileInfo> selectedFiles;

        for (const auto& file : list) {
            auto item = file.as<winrt::StarlightGUI::FileInfo>();
            // 跳过"上个文件夹"选项
            if (item.Flag() == 999) continue;
            if ((item.Name() == L"Windows" || item.Name() == L"Boot" || item.Name() == L"System32" || item.Name() == L"SysWOW64" || item.Name() == L"Microsoft") &&
                (safeAcceptedName != L"Windows" && safeAcceptedName != L"Boot" && safeAcceptedName != L"System32" && safeAcceptedName != L"SysWOW64" && safeAcceptedName != L"Microsoft")) {
                safeAcceptedName = item.Name();
                slg::CreateInfoBarAndDisplay(L"警告", L"该文件可能为重要文件，如果确认继续请再次点击！", InfoBarSeverity::Warning, g_mainWindowInstance);
                return;
            }
            selectedFiles.push_back(item);
        }

        safeAcceptedName = L"";

        auto flyoutStyles = slg::GetStyles();

        MenuFlyout menuFlyout;

        /*
        * 注意，由于这里是磁盘 IO，我们不要使用异步，否则刷新时可能会出问题
        */
        // 选项1.1
        auto item1_1 = slg::CreateMenuItem(flyoutStyles, L"\ue8e5", L"打开", [this, selectedFiles](IInspectable const& sender, RoutedEventArgs const& e) {
            for (const auto& item : selectedFiles) {
                if (item.Directory()) {
                    currentDirectory = currentDirectory + L"\\" + item.Name();
                    LoadFileList();
                }
                else ShellExecuteW(nullptr, L"open", item.Path().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }
            });

        MenuFlyoutSeparator separator1;

        // 选项2.1
        auto item2_1 = slg::CreateMenuItem(flyoutStyles, L"\ue74d", L"删除", [this, selectedFiles](IInspectable const& sender, RoutedEventArgs const& e) {
            for (const auto& item : selectedFiles) {
                if (KernelInstance::DeleteFileAuto(item.Path().c_str())) {
                    slg::CreateInfoBarAndDisplay(L"成功", L"成功删除文件/文件夹: " + item.Name() + L" (" + item.Path() + L")", InfoBarSeverity::Success, g_mainWindowInstance);
                    WaitAndReloadAsync(1000);
                }
                else slg::CreateInfoBarAndDisplay(L"失败", L"无法删除文件/文件夹: " + item.Name() + L" (" + item.Path() + L"), 错误码: " + to_hstring((int)GetLastError()), InfoBarSeverity::Error, g_mainWindowInstance);
            }
            });

        // 选项2.2
        auto item2_2 = slg::CreateMenuItem(flyoutStyles, L"\ue733", L"删除 (内核)", [this, selectedFiles](IInspectable const& sender, RoutedEventArgs const& e) {
            for (const auto& item : selectedFiles) {
                if (KernelInstance::_DeleteFileAuto(item.Path().c_str())) {
                    slg::CreateInfoBarAndDisplay(L"成功", L"成功删除文件/文件夹: " + item.Name() + L" (" + item.Path() + L")", InfoBarSeverity::Success, g_mainWindowInstance);
                    WaitAndReloadAsync(1000);
                }
                else slg::CreateInfoBarAndDisplay(L"失败", L"无法删除文件/文件夹: " + item.Name() + L" (" + item.Path() + L"), 错误码: " + to_hstring((int)GetLastError()), InfoBarSeverity::Error, g_mainWindowInstance);
            }
            });

        // 选项2.3
        auto item2_3 = slg::CreateMenuItem(flyoutStyles, L"\uf5ab", L"删除 (内存抹杀)", [this, selectedFiles](IInspectable const& sender, RoutedEventArgs const& e) {
            for (const auto& item : selectedFiles) {
                if (KernelInstance::MurderFileAuto(item.Path().c_str())) {
                    slg::CreateInfoBarAndDisplay(L"成功", L"成功删除文件/文件夹: " + item.Name() + L" (" + item.Path() + L")", InfoBarSeverity::Success, g_mainWindowInstance);
                    WaitAndReloadAsync(1000);
                }
                else slg::CreateInfoBarAndDisplay(L"失败", L"无法删除文件/文件夹: " + item.Name() + L" (" + item.Path() + L"), 错误码: " + to_hstring((int)GetLastError()), InfoBarSeverity::Error, g_mainWindowInstance);
            }
            });

        // 选项2.4
        auto item2_4 = slg::CreateMenuItem(flyoutStyles, L"\ue72e", L"锁定", [this, selectedFiles](IInspectable const& sender, RoutedEventArgs const& e) {
            for (const auto& item : selectedFiles) {
                if (KernelInstance::LockFile(item.Path().c_str())) {
                    slg::CreateInfoBarAndDisplay(L"成功", L"成功锁定文件: " + item.Name() + L" (" + item.Path() + L")", InfoBarSeverity::Success, g_mainWindowInstance);
                    WaitAndReloadAsync(1000);
                }
                else slg::CreateInfoBarAndDisplay(L"失败", L"无法锁定文件: " + item.Name() + L" (" + item.Path() + L"), 错误码: " + to_hstring((int)GetLastError()), InfoBarSeverity::Error, g_mainWindowInstance);
            }
            });

        // 选项2.5
        auto item2_5 = slg::CreateMenuItem(flyoutStyles, L"\ue8c8", L"复制", [this, selectedFiles](IInspectable const& sender, RoutedEventArgs const& e) {
            CopyFiles();
            });

        MenuFlyoutSeparator separator2;

        // 选项3.1
        auto item3_1 = slg::CreateMenuSubItem(flyoutStyles, L"\ue8c8", L"复制信息");
        auto item3_1_sub1 = slg::CreateMenuItem(flyoutStyles, L"\ue8ac", L"名称", [this, selectedFiles](IInspectable const& sender, RoutedEventArgs const& e) -> winrt::Windows::Foundation::IAsyncAction {
            if (TaskUtils::CopyToClipboard(selectedFiles[0].Name().c_str())) {
                slg::CreateInfoBarAndDisplay(L"成功", L"已复制内容至剪贴板", InfoBarSeverity::Success, g_mainWindowInstance);
            }
            else slg::CreateInfoBarAndDisplay(L"失败", L"无法复制内容至剪贴板, 错误码: " + to_hstring((int)GetLastError()), InfoBarSeverity::Error, g_mainWindowInstance);
            co_return;
            });
        item3_1.Items().Append(item3_1_sub1);
        auto item3_1_sub2 = slg::CreateMenuItem(flyoutStyles, L"\uec6c", L"路径", [this, selectedFiles](IInspectable const& sender, RoutedEventArgs const& e) -> winrt::Windows::Foundation::IAsyncAction {
            if (TaskUtils::CopyToClipboard(selectedFiles[0].Path().c_str())) {
                slg::CreateInfoBarAndDisplay(L"成功", L"已复制内容至剪贴板", InfoBarSeverity::Success, g_mainWindowInstance);
            }
            else slg::CreateInfoBarAndDisplay(L"失败", L"无法复制内容至剪贴板, 错误码: " + to_hstring((int)GetLastError()), InfoBarSeverity::Error, g_mainWindowInstance);
            co_return;
            });
        item3_1.Items().Append(item3_1_sub2);
        auto item3_1_sub3 = slg::CreateMenuItem(flyoutStyles, L"\uec92", L"修改日期", [this, selectedFiles](IInspectable const& sender, RoutedEventArgs const& e) -> winrt::Windows::Foundation::IAsyncAction {
            if (TaskUtils::CopyToClipboard(selectedFiles[0].ModifyTime().c_str())) {
                slg::CreateInfoBarAndDisplay(L"成功", L"已复制内容至剪贴板", InfoBarSeverity::Success, g_mainWindowInstance);
            }
            else slg::CreateInfoBarAndDisplay(L"失败", L"无法复制内容至剪贴板, 错误码: " + to_hstring((int)GetLastError()), InfoBarSeverity::Error, g_mainWindowInstance);
            co_return;
            });
        item3_1.Items().Append(item3_1_sub3);

        // 选项3.2
        auto item3_2 = slg::CreateMenuItem(flyoutStyles, L"\uec50", L"在文件管理器内打开", [this, selectedFiles](IInspectable const& sender, RoutedEventArgs const& e) -> winrt::Windows::Foundation::IAsyncAction {
            if (TaskUtils::OpenFolderAndSelectFile(selectedFiles[0].Path().c_str())) {
                slg::CreateInfoBarAndDisplay(L"成功", L"已打开文件夹", InfoBarSeverity::Success, g_mainWindowInstance);
            }
            else slg::CreateInfoBarAndDisplay(L"失败", L"无法打开文件夹, 错误码: " + to_hstring((int)GetLastError()), InfoBarSeverity::Error, g_mainWindowInstance);
            co_return;
            });


        // 选项3.3
        auto item3_3 = slg::CreateMenuItem(flyoutStyles, L"\ue8ec", L"属性", [this, selectedFiles](IInspectable const& sender, RoutedEventArgs const& e) -> winrt::Windows::Foundation::IAsyncAction {
            if (TaskUtils::OpenFileProperties(selectedFiles[0].Path().c_str())) {
                slg::CreateInfoBarAndDisplay(L"成功", L"已打开文件属性", InfoBarSeverity::Success, g_mainWindowInstance);
            }
            else slg::CreateInfoBarAndDisplay(L"失败", L"无法打开文件属性, 错误码: " + to_hstring((int)GetLastError()), InfoBarSeverity::Error, g_mainWindowInstance);
            co_return;
            });

        // 当选中多个内容并且其中一个是文件夹时禁用锁定部分只能操作文件的按钮
        // 当选中多个内容并且其中一个是文件夹时禁用打开按钮
        for (const auto& item : selectedFiles) {
            if (item.Directory()) {
                item2_4.IsEnabled(false);
                item2_5.IsEnabled(false);
                if (selectedFiles.size() > 1) item1_1.IsEnabled(false);
                break;
            }
        }

        menuFlyout.Items().Append(item1_1);
        menuFlyout.Items().Append(separator1);
        menuFlyout.Items().Append(item2_1);
        menuFlyout.Items().Append(item2_2);
        menuFlyout.Items().Append(item2_3);
        menuFlyout.Items().Append(item2_4);
        menuFlyout.Items().Append(item2_5);
        menuFlyout.Items().Append(separator2);
        menuFlyout.Items().Append(item3_1);
        menuFlyout.Items().Append(item3_2);
        menuFlyout.Items().Append(item3_3);

        slg::ShowAt(menuFlyout, listView, e);
    }

	void FilePage::FileListView_DoubleTapped(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::DoubleTappedRoutedEventArgs const& e)
	{
        if (!FileListView().SelectedItem()) return;

        auto item = FileListView().SelectedItem().as<winrt::StarlightGUI::FileInfo>();

        if (item.Flag() == 999) {
            currentDirectory = GetParentDirectory(currentDirectory.c_str());
            LoadFileList();
        } else if (item.Directory()) {
            currentDirectory = currentDirectory + L"\\" + item.Name();
            LoadFileList();
        }
        else {
            ShellExecuteW(nullptr, L"open", item.Path().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
	}

    void FilePage::FileListView_DragOver(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::DragEventArgs const& e)
    {
        using namespace winrt::Windows::ApplicationModel::DataTransfer;

        if (e.DataView().Contains(StandardDataFormats::StorageItems())) {
            e.AcceptedOperation(DataPackageOperation::Copy);
            auto dragUI = e.DragUIOverride();
            dragUI.IsCaptionVisible(true);
            dragUI.Caption(L"复制到当前目录");
        }
        else {
            e.AcceptedOperation(DataPackageOperation::None);
        }
    }

    void FilePage::FileListView_Drop(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::DragEventArgs const& e)
    {
        using namespace winrt::Windows::ApplicationModel::DataTransfer;
        using namespace winrt::Windows::Storage;

        e.AcceptedOperation(DataPackageOperation::Copy);

        auto deferral = e.GetDeferral();
        auto dataView = e.DataView();
        auto lifetime = get_strong();

        [this, lifetime, deferral, dataView]() -> winrt::Windows::Foundation::IAsyncAction {
            if (m_isLoadingFiles || m_isPostLoading) {
                slg::CreateInfoBarAndDisplay(L"提示", L"当前正在加载，请稍后再试", InfoBarSeverity::Warning, g_mainWindowInstance);
                deferral.Complete();
                co_return;
            }

            try {
                if (!dataView.Contains(StandardDataFormats::StorageItems())) {
                    deferral.Complete();
                    co_return;
                }

                auto droppedItems = co_await dataView.GetStorageItemsAsync();
                if (!droppedItems || droppedItems.Size() == 0) {
                    deferral.Complete();
                    co_return;
                }

                std::vector<std::wstring> droppedPaths;
                droppedPaths.reserve(droppedItems.Size());
                for (auto const& item : droppedItems) {
                    auto storageItem = item.try_as<IStorageItem>();
                    if (!storageItem) {
                        continue;
                    }
                    droppedPaths.push_back(storageItem.Path().c_str());
                }

                co_await CopyDroppedPathsAsync(std::move(droppedPaths));
            }
            catch (...) {
                slg::CreateInfoBarAndDisplay(L"失败", L"拖拽复制时出现异常", InfoBarSeverity::Error, g_mainWindowInstance);
            }

            deferral.Complete();
            co_return;
            }();
    }

    winrt::Windows::Foundation::IAsyncAction FilePage::CopyDroppedPathsAsync(std::vector<std::wstring> paths)
    {
        if (paths.empty()) co_return;
        if (m_isLoadingFiles || m_isPostLoading) {
            slg::CreateInfoBarAndDisplay(L"警告", L"当前正在加载，请稍后再试!", InfoBarSeverity::Warning, g_mainWindowInstance);
            co_return;
        }

        int successCount = 0, failedCount = 0;
        std::wstring targetDirectory = FixBackSplash(currentDirectory.c_str());

        for (auto const& rawPath : paths) {
            std::wstring sourcePath = FixBackSplash(rawPath.c_str());
            if (sourcePath.empty()) {
                failedCount++;
                continue;
            }

            auto pos = sourcePath.find_last_of(L'\\');
            if (pos == std::wstring::npos) {
                failedCount++;
                continue;
            }

            std::wstring itemName = sourcePath.substr(pos + 1);
            std::wstring targetPath = targetDirectory + L"\\" + itemName;

            bool copied = false;

            try {
                if (fs::is_directory(sourcePath)) {
                    fs::copy(sourcePath, targetPath, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                    copied = true;
                }
                else if (fs::is_regular_file(sourcePath)) {
                    copied = fs::copy_file(sourcePath, targetPath, fs::copy_options::overwrite_existing);
                }
            }
            catch (...) {
                copied = false;
            }

            if (copied) {
                LOG_INFO(__WFUNCTION__, L"Copied by drag-drop: %s -> %s", sourcePath.c_str(), targetPath.c_str());
                successCount++;
            }
            else {
                LOG_ERROR(__WFUNCTION__, L"Failed to copy by drag-drop: %s -> %s", sourcePath.c_str(), targetPath.c_str());
                failedCount++;
            }
        }

        if (successCount > 0) {
            slg::CreateInfoBarAndDisplay(L"成功", L"成功复制" + to_hstring(successCount) + L"个项目!", InfoBarSeverity::Success, g_mainWindowInstance);
            co_await LoadFileList();
        }
        if (failedCount > 0) {
            slg::CreateInfoBarAndDisplay(L"失败", L"有" + to_hstring(failedCount) + L"个项目复制失败! 请尝试以高权限运行!", InfoBarSeverity::Error, g_mainWindowInstance);
        }

        co_return;
    }

    void FilePage::FileListView_ContainerContentChanging(
        winrt::Microsoft::UI::Xaml::Controls::ListViewBase const& sender,
        winrt::Microsoft::UI::Xaml::Controls::ContainerContentChangingEventArgs const& args)
    {
        slg::ApplyListRevealFocusTag(sender, args);

        if (auto file = args.Item().try_as<winrt::StarlightGUI::FileInfo>()) {
            if (file.Icon()) {
                UpdateRealizedItemIcon(file, file.Icon());
            }
            else {
                GetFileIconAsync(file);
            }
        }
    }

    winrt::Windows::Foundation::IAsyncAction FilePage::LoadFileList()
    {
        if (m_isLoadingFiles || m_isPostLoading) co_return;
        m_isLoadingFiles = true;

        LOG_INFO(__WFUNCTION__, L"Loading file list...");
        ResetState();
        LoadingRing().IsActive(true);

        auto start = std::chrono::steady_clock::now();

        auto lifetime = get_strong();

        std::wstring path = FixBackSplash(currentDirectory);
        auto loadToken = ++m_currentLoadToken;
        currentDirectory = path;
        PathBox().Text(currentDirectory);
        LOG_INFO(__WFUNCTION__, L"Path = %s", path.c_str());

        // 简单判断根目录
        PreviousButton().IsEnabled(path.length() > 3);

        co_await winrt::resume_background();

        m_allFiles.clear();

        KernelInstance::QueryFile(path, m_allFiles);
        LOG_INFO(__WFUNCTION__, L"Enumerated files, %d entry(s).", m_allFiles.size());

        co_await wil::resume_foreground(DispatcherQueue());
        if (loadToken != m_currentLoadToken) {
            m_isLoadingFiles = false;
            co_return;
        }

        ApplySort(currentSortingOption, currentSortingType);
        std::stable_partition(m_allFiles.begin(), m_allFiles.end(), [](auto const& file) { return file.Directory(); });

        auto newFileList = winrt::multi_threaded_observable_vector<winrt::StarlightGUI::FileInfo>();

        if (currentDirectory.size() > 3) {
            auto previousPage = winrt::make<winrt::StarlightGUI::implementation::FileInfo>();
            previousPage.Name(L"上个文件夹");
            previousPage.Flag(999);
            newFileList.Append(previousPage);
        }

        winrt::hstring query = SearchBox().Text();
        for (size_t i = 0; i < m_allFiles.size(); ++i) {
            bool shouldRemove = query.empty() ? false : ApplyFilter(m_allFiles[i], query);
            if (shouldRemove) continue;

            newFileList.Append(m_allFiles[i]);
        }
        m_fileList = newFileList;
        FileListView().ItemsSource(m_fileList);

        m_isPostLoading = true;
        LoadMetaForCurrentList(path, loadToken);

        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        std::wstringstream countText;
        countText << L"共 " << m_allFiles.size() << L" 个文件 (" << duration << " ms)";
        FileCountText().Text(countText.str());
        LoadingRing().IsActive(false);

        LOG_INFO(__WFUNCTION__, L"Loaded file list, %d entry(s) in total.", m_allFiles.size());

        m_isLoadingFiles = false;
    }

    winrt::Windows::Foundation::IAsyncAction FilePage::LoadMetaForCurrentList(std::wstring path, uint64_t loadToken)
    {
        auto lifetime = get_strong();

        co_await winrt::resume_background();

        PopulateFileMetaBatch(path);

        co_await wil::resume_foreground(DispatcherQueue());
        if (!IsLoaded() || loadToken != m_currentLoadToken) {
            m_isPostLoading = false;
            co_return;
        }

        // 触发一次轻量刷新，确保更新后的属性及时反映到列表，并避免播放第二次刷新动画
        auto oldTransitions = FileListView().ItemContainerTransitions();
        FileListView().ItemContainerTransitions().Clear();
        FileListView().ItemsSource(nullptr);
        FileListView().ItemsSource(m_fileList);
        FileListView().ItemContainerTransitions(oldTransitions);
        m_isPostLoading = false;
    }

    void FilePage::PopulateFileMetaBatch(std::wstring const& directoryPath)
    {
        auto fillUnknownMeta = [](winrt::StarlightGUI::FileInfo const& file) {
            if (!file.Directory()) {
                if (file.SizeULong() == 0) file.Size(L"0 B");
                else file.Size(FormatMemorySize(file.SizeULong()));
            }
            else {
                file.SizeULong(0);
                file.Size(L"");
            }
            if (file.ModifyTime().empty()) file.ModifyTime(L"(未知)");
            };

        std::wstring searchPath = directoryPath + L"\\*";
        WIN32_FIND_DATAW data{};
        HANDLE hFind = FindFirstFileExW(searchPath.c_str(), FindExInfoBasic, &data, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
        if (hFind == INVALID_HANDLE_VALUE) {
            for (auto const& file : m_allFiles) {
                file.Path(FixBackSplash(file.Path()));
                fillUnknownMeta(file);
            }
            return;
        }

        std::unordered_map<std::wstring, WIN32_FIND_DATAW> metaMap;
        metaMap.reserve(m_allFiles.size() * 2);

        auto normalize = [](std::wstring str) {
            std::transform(str.begin(), str.end(), str.begin(), ::towlower);
            return str;
            };

        do {
            if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0) continue;
            metaMap[normalize(data.cFileName)] = data;
        } while (FindNextFileW(hFind, &data));

        FindClose(hFind);

        for (auto const& file : m_allFiles) {
            file.Path(FixBackSplash(file.Path()));
            std::wstring name = file.Name().c_str();
            auto it = metaMap.find(normalize(name));
            if (it == metaMap.end()) {
                fillUnknownMeta(file);
                continue;
            }

            auto const& d = it->second;
            const bool isDir = (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            if (!isDir) {
                ULONG64 size = ((ULONG64)d.nFileSizeHigh << 32) | d.nFileSizeLow;
                file.SizeULong(size);
                file.Size(FormatMemorySize(size));
            }
            else {
                file.SizeULong(0);
                file.Size(L"");
            }

            file.ModifyTimeULong(((ULONG64)d.ftLastWriteTime.dwHighDateTime << 32) | d.ftLastWriteTime.dwLowDateTime);
            SYSTEMTIME st{};
            if (FileTimeToSystemTime(&d.ftLastWriteTime, &st))
            {
                std::wstringstream ss;
                ss << std::setw(4) << std::setfill(L'0') << st.wYear << L"/"
                    << std::setw(2) << std::setfill(L'0') << st.wMonth << L"/"
                    << std::setw(2) << std::setfill(L'0') << st.wDay << L" "
                    << std::setw(2) << std::setfill(L'0') << st.wHour << L":"
                    << std::setw(2) << std::setfill(L'0') << st.wMinute << L":"
                    << std::setw(2) << std::setfill(L'0') << st.wSecond;
                file.ModifyTime(ss.str());
            }
            else
            {
                file.ModifyTime(L"(未知)");
            }
        }
    }

    std::wstring FilePage::GetIconCacheKey(winrt::StarlightGUI::FileInfo file)
    {
        if (!file) return L"__invalid__";
        if (file.Flag() == 999) return L"__dir__";
        if (file.Directory()) return L"__dir__";

        std::wstring name = file.Name().c_str();
        auto dot = name.find_last_of(L'.');
        if (dot != std::wstring::npos) {
            std::wstring ext = name.substr(dot);
            if (IsFastIconCacheExtension(ext)) {
                std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
                return L"__ext__" + ext;
            }
        }

        std::wstring path = file.Path().c_str();
        std::transform(path.begin(), path.end(), path.begin(), ::towlower);
        return L"__path__" + path;
    }

    winrt::Windows::Foundation::IAsyncAction FilePage::GetFileIconAsync(winrt::StarlightGUI::FileInfo file)
    {
        if (!file) co_return;
        co_await wil::resume_foreground(DispatcherQueue());

        std::wstring cacheKey = GetIconCacheKey(file);
        auto found = iconCache.find(cacheKey);
        if (found != iconCache.end()) {
            file.Icon(found->second);
            UpdateRealizedItemIcon(file, found->second);
            co_return;
        }

        iconPendingFiles[cacheKey].push_back(file);
        if (iconLoadingKeys.find(cacheKey) != iconLoadingKeys.end()) co_return;
        iconLoadingKeys.insert(cacheKey);

        SHFILEINFO shfi{};
        bool useFastAttrQuery = file.Directory() || cacheKey.find(L"__ext__") == 0;
        if (useFastAttrQuery) {
            DWORD attrs = file.Directory() ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
            std::wstring lookup = file.Directory() ? L"folder" : cacheKey.substr(7);
            if (!SHGetFileInfoW(lookup.c_str(), attrs, &shfi, sizeof(SHFILEINFO), SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES)) {
                if (!SHGetFileInfoW(L".", FILE_ATTRIBUTE_NORMAL, &shfi, sizeof(SHFILEINFO), SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES)) {
                    iconLoadingKeys.erase(cacheKey);
                    iconPendingFiles.erase(cacheKey);
                    co_return;
                }
            }
        }
        else {
            if (!SHGetFileInfoW(file.Path().c_str(), 0, &shfi, sizeof(SHFILEINFO), SHGFI_ICON | SHGFI_SMALLICON)) {
                if (!SHGetFileInfoW(L".", FILE_ATTRIBUTE_NORMAL, &shfi, sizeof(SHFILEINFO), SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES)) {
                    iconLoadingKeys.erase(cacheKey);
                    iconPendingFiles.erase(cacheKey);
                    co_return;
                }
            }
        }

        ICONINFO iconInfo{};
        if (!GetIconInfo(shfi.hIcon, &iconInfo)) {
            DestroyIcon(shfi.hIcon);
            iconLoadingKeys.erase(cacheKey);
            iconPendingFiles.erase(cacheKey);
            co_return;
        }

        BITMAP bmp{};
        GetObject(iconInfo.hbmColor, sizeof(bmp), &bmp);
        BITMAPINFOHEADER bmiHeader = { 0 };
        bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmiHeader.biWidth = bmp.bmWidth;
        bmiHeader.biHeight = bmp.bmHeight;
        bmiHeader.biPlanes = 1;
        bmiHeader.biBitCount = 32;
        bmiHeader.biCompression = BI_RGB;

        int dataSize = bmp.bmWidthBytes * bmp.bmHeight;
        std::vector<BYTE> buffer(dataSize);
        GetDIBits(hdc, iconInfo.hbmColor, 0, bmp.bmHeight, buffer.data(), reinterpret_cast<BITMAPINFO*>(&bmiHeader), DIB_RGB_COLORS);

        winrt::Microsoft::UI::Xaml::Media::Imaging::WriteableBitmap writeableBitmap(bmp.bmWidth, bmp.bmHeight);
        uint8_t* pixelData = writeableBitmap.PixelBuffer().data();
        int rowSize = bmp.bmWidth * 4;
        for (int i = 0; i < bmp.bmHeight; ++i) {
            int srcOffset = i * rowSize;
            int dstOffset = (bmp.bmHeight - 1 - i) * rowSize;
            std::memcpy(pixelData + dstOffset, buffer.data() + srcOffset, rowSize);
        }

        DeleteObject(iconInfo.hbmColor);
        DeleteObject(iconInfo.hbmMask);
        DestroyIcon(shfi.hIcon);

        auto icon = writeableBitmap.as<winrt::Microsoft::UI::Xaml::Media::ImageSource>();
        iconCache.insert_or_assign(cacheKey, icon);

        auto pendingIt = iconPendingFiles.find(cacheKey);
        if (pendingIt != iconPendingFiles.end()) {
            for (auto const& pendingFile : pendingIt->second) {
                if (pendingFile && !pendingFile.Icon()) {
                    pendingFile.Icon(icon);
                    UpdateRealizedItemIcon(pendingFile, icon);
                }
            }
            iconPendingFiles.erase(pendingIt);
        }

        if (file && !file.Icon()) {
            file.Icon(icon);
            UpdateRealizedItemIcon(file, icon);
        }
        iconLoadingKeys.erase(cacheKey);

        co_return;
    }

    void FilePage::UpdateRealizedItemIcon(winrt::StarlightGUI::FileInfo const& file, winrt::Microsoft::UI::Xaml::Media::ImageSource const& icon)
    {
        if (!file || !icon || !IsLoaded()) return;

        auto container = FileListView().ContainerFromItem(file).try_as<ListViewItem>();
        if (!container) return;

        auto root = container.ContentTemplateRoot();
        if (!root) return;

        auto image = FindVisualChild<Image>(root);
        if (image) {
            image.Source(icon);
        }
    }

    void FilePage::SearchBox_TextChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e) {
        if (!IsLoaded()) return;
        WaitAndReloadAsync(100);
    }

    void FilePage::PathBox_KeyDown(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& e) {
        if (e.Key() == winrt::Windows::System::VirtualKey::Enter)
        {
            if (m_isLoadingFiles || m_isPostLoading) {
                e.Handled(true);
                return;
            }
            try
            {
                fs::path path(PathBox().Text().c_str());
                if (fs::exists(path) && fs::is_directory(path)) {
                    currentDirectory = PathBox().Text();
                }
            }
            catch (...) {}
            PathBox().Text(currentDirectory);
            LoadFileList();
            e.Handled(true);
        }
    }

    bool FilePage::ApplyFilter(const winrt::StarlightGUI::FileInfo& file, hstring& query) {
        std::wstring name = file.Name().c_str();
        std::wstring queryWStr = query.c_str();

        // 不比较大小写
        std::transform(name.begin(), name.end(), name.begin(), ::towlower);
        std::transform(queryWStr.begin(), queryWStr.end(), queryWStr.begin(), ::towlower);

        return name.find(queryWStr) == std::wstring::npos;
    }

    void FilePage::ColumnHeader_Click(IInspectable const& sender, RoutedEventArgs const& e)
    {
        ++m_currentLoadToken;

        Button clickedButton = sender.as<Button>();
        winrt::hstring columnName = clickedButton.Tag().as<winrt::hstring>();

        if (columnName == L"Previous") {
            currentDirectory = GetParentDirectory(currentDirectory.c_str());
            LoadFileList();
            return;
        }

        if (columnName == L"Name")
        {
            ApplySort(m_isNameAscending, "Name");
        }
        else if (columnName == L"ModifyTime")
        {
            ApplySort(m_isModifyTimeAscending, "ModifyTime");
        }
        else if (columnName == L"Size")
        {
            ApplySort(m_isSizeAscending, "Size");
        }

        ResetState();

        auto newFileList = winrt::multi_threaded_observable_vector<winrt::StarlightGUI::FileInfo>();

        if (currentDirectory.size() > 3) {
            auto previousPage = winrt::make<winrt::StarlightGUI::implementation::FileInfo>();
            previousPage.Name(L"上个文件夹");
            previousPage.Flag(999);
            newFileList.Append(previousPage);
        }

        winrt::hstring query = SearchBox().Text();
        for (size_t i = 0; i < m_allFiles.size(); ++i) {
            bool shouldRemove = query.empty() ? false : ApplyFilter(m_allFiles[i], query);
            if (shouldRemove) continue;

            newFileList.Append(m_allFiles[i]);
        }

        m_fileList = newFileList;
        FileListView().ItemsSource(m_fileList);

    }

    // 排序切换
    slg::coroutine FilePage::ApplySort(bool& isAscending, const std::string& column)
    {
        NameHeaderButton().Content(box_value(L"文件"));
        ModifyTimeHeaderButton().Content(box_value(L"修改时间"));
        SizeHeaderButton().Content(box_value(L"大小"));

        if (column == "Name") {
            if (isAscending) {
                NameHeaderButton().Content(box_value(L"文件 ↓"));
                std::sort(m_allFiles.begin(), m_allFiles.end(), [](auto a, auto b) {
                    std::wstring aName = a.Name().c_str();
                    std::wstring bName = b.Name().c_str();
                    std::transform(aName.begin(), aName.end(), aName.begin(), ::towlower);
                    std::transform(bName.begin(), bName.end(), bName.begin(), ::towlower);

                    return aName < bName;
                    });

            }
            else {
                NameHeaderButton().Content(box_value(L"文件 ↑"));
                std::sort(m_allFiles.begin(), m_allFiles.end(), [](auto a, auto b) {
                    std::wstring aName = a.Name().c_str();
                    std::wstring bName = b.Name().c_str();
                    std::transform(aName.begin(), aName.end(), aName.begin(), ::towlower);
                    std::transform(bName.begin(), bName.end(), bName.begin(), ::towlower);

                    return aName > bName;
                    });
            }
        } else if (column == "ModifyTime") {
            if (isAscending) {
                ModifyTimeHeaderButton().Content(box_value(L"修改时间 ↓"));
                std::sort(m_allFiles.begin(), m_allFiles.end(), [](auto a, auto b) {
                    return a.ModifyTimeULong() < b.ModifyTimeULong();
                    });

            }
            else {
                ModifyTimeHeaderButton().Content(box_value(L"修改时间 ↑"));
                std::sort(m_allFiles.begin(), m_allFiles.end(), [](auto a, auto b) {
                    return a.ModifyTimeULong() > b.ModifyTimeULong();
                    });
            }
        } else if (column == "Size") {
            if (isAscending) {
                SizeHeaderButton().Content(box_value(L"大小 ↓"));
                std::sort(m_allFiles.begin(), m_allFiles.end(), [](auto a, auto b) {
                    return a.SizeULong() < b.SizeULong();
                    });

            }
            else {
                SizeHeaderButton().Content(box_value(L"大小 ↑"));
                std::sort(m_allFiles.begin(), m_allFiles.end(), [](auto a, auto b) {
                    return a.SizeULong() > b.SizeULong();
                    });
            }
        }

        isAscending = !isAscending;
        currentSortingOption = !isAscending;
        currentSortingType = column;

        co_return;
    }

    slg::coroutine FilePage::RefreshButton_Click(IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        if (m_isLoadingFiles || m_isPostLoading) co_return;
        RefreshButton().IsEnabled(false);

        iconCache.clear();
        iconLoadingKeys.clear();
        iconPendingFiles.clear();

        co_await LoadFileList();
        RefreshButton().IsEnabled(true);
        co_return;
    }

    slg::coroutine FilePage::NextDriveButton_Click(IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        if (m_isLoadingFiles || m_isPostLoading) co_return;
        static std::vector<std::wstring> drives;
        static int currentIndex = 1;

        if (drives.empty()) {
            wchar_t driveBuffer[256];
            GetLogicalDriveStrings(255, driveBuffer);

            for (wchar_t* drive = driveBuffer; *drive; drive += wcslen(drive) + 1) {
                drives.push_back(drive);
            }

            if (drives.empty()) drives = { L"C:\\" };
        }

        currentDirectory = drives[currentIndex];
        currentIndex = (currentIndex + 1) % drives.size();

        co_await LoadFileList();
    }

    void FilePage::ResetState() {
        m_fileList.Clear();
    }

    winrt::Windows::Foundation::IAsyncAction FilePage::WaitAndReloadAsync(int interval) {
        auto lifetime = get_strong();

        reloadTimer.Stop();
        reloadTimer.Interval(std::chrono::milliseconds(interval));
        reloadTimer.Tick([this](auto&&, auto&&) {
            LoadFileList();
            reloadTimer.Stop();
            });
        reloadTimer.Start();

        co_return;
    }

    slg::coroutine FilePage::CopyFiles() {
        auto dialog = winrt::make<winrt::StarlightGUI::implementation::CopyFileDialog>();
        dialog.XamlRoot(this->XamlRoot());

        auto result = co_await dialog.ShowAsync();

        if (result == ContentDialogResult::Primary) {
            std::wstring copyPath = dialog.CopyPath().c_str();

            std::vector<winrt::StarlightGUI::FileInfo> selectedFiles;

            for (const auto& file : FileListView().SelectedItems()) {
                auto item = file.as<winrt::StarlightGUI::FileInfo>();
                // 跳过上个文件夹选项
                if (item.Flag() == 999) continue;
				if (item.Directory()) continue;
                selectedFiles.push_back(item);
            }

            int successCount = 0, failedCount = 0;
            for (const auto& item : selectedFiles) {
                bool result = false;
                try {
                    if (fs::is_directory(item.Path().c_str())) {
                        fs::copy(item.Path().c_str(), copyPath + item.Name().c_str(), fs::copy_options::recursive | fs::copy_options::overwrite_existing);
                        result = true;
                    }
                    else if (fs::is_regular_file(item.Path().c_str())) {
                        result = fs::copy_file(item.Path().c_str(), copyPath + item.Name().c_str(), fs::copy_options::overwrite_existing);
                    }
                }
                catch (...) {
                    result = false;
                }
                
                if (result) successCount++;
				else failedCount++;
            }
            if (successCount > 0) {
                slg::CreateInfoBarAndDisplay(L"成功", L"成功复制" + to_hstring(successCount) + L"个文件!", InfoBarSeverity::Success, g_mainWindowInstance);
                co_await LoadFileList();
            }
            if (failedCount > 0) {
                slg::CreateInfoBarAndDisplay(L"失败", L"有" + to_hstring(failedCount) + L"个文件复制失败! 请尝试以高权限运行!", InfoBarSeverity::Error, g_mainWindowInstance);
            }
        }
    }
}

