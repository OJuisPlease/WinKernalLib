#include "pch.h"
#include "Process_ThreadPage.xaml.h"
#if __has_include("Process_ThreadPage.g.cpp")
#include "Process_ThreadPage.g.cpp"
#endif


#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Foundation.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <array>
#include <sstream>
#include <iomanip>
#include <Utils/Utils.h>
#include <Utils/TaskUtils.h>
#include <Utils/KernelBase.h>
#include <InfoWindow.xaml.h>
#include <MainWindow.xaml.h>

using namespace winrt;
using namespace Microsoft::UI::Text;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
using namespace Microsoft::UI::Xaml::Media::Imaging;
using namespace Windows::Storage::Streams;
using namespace Windows::Graphics::Imaging;
using namespace Windows::System;

namespace winrt::StarlightGUI::implementation
{
    static int safeAcceptedPID = -1;

    Process_ThreadPage::Process_ThreadPage() {
        InitializeComponent();

        ThreadListView().ItemsSource(m_threadList);

        this->Loaded([this](auto&&, auto&&) {
            LoadThreadList();
            });

        LOG_INFO(L"Process_ThreadPage", L"Process_ThreadPage initialized.");
    }

    void Process_ThreadPage::ThreadListView_RightTapped(IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const& e)
    {
        auto listView = sender.as<ListView>();

        slg::SelectItemOnRightTapped(listView, e);

        if (!listView.SelectedItem()) return;

        auto item = listView.SelectedItem().as<winrt::StarlightGUI::ThreadInfo>();

        auto flyoutStyles = slg::GetStyles();

        MenuFlyout menuFlyout;

        auto itemRefresh = slg::CreateMenuItem(flyoutStyles, L"\ue72c", L"刷新", [this](IInspectable const& sender, RoutedEventArgs const& e) -> winrt::Windows::Foundation::IAsyncAction {
            LoadThreadList();
            co_return;
            });

        MenuFlyoutSeparator separatorR;

        // 选项1.1
        auto item1_1 = slg::CreateMenuItem(flyoutStyles, L"\ue711", L"终止", [this, item](IInspectable const& sender, RoutedEventArgs const& e) -> winrt::Windows::Foundation::IAsyncAction {
            if (TaskUtils::_TerminateThread(item.Id())) {
                slg::CreateInfoBarAndDisplay(L"成功", L"成功终止线程: " + item.Address() + L" (" + to_hstring(item.Id()) + L")", InfoBarSeverity::Success, g_infoWindowInstance);
                LoadThreadList();
            }
            else slg::CreateInfoBarAndDisplay(L"失败", L"无法终止线程: " + item.Address() + L" (" + to_hstring(item.Id()) + L"), 错误码: " + to_hstring((int)GetLastError()), InfoBarSeverity::Error, g_infoWindowInstance);
            co_return;
            });

        // 选项1.2
        auto item1_2 = slg::CreateMenuItem(flyoutStyles, L"\ue8f0", L"终止 (内核)", [this, item](IInspectable const& sender, RoutedEventArgs const& e) -> winrt::Windows::Foundation::IAsyncAction {
            if (KernelInstance::_ZwTerminateThread(item.Id())) {
                slg::CreateInfoBarAndDisplay(L"成功", L"成功终止线程: " + item.Address() + L" (" + to_hstring(item.Id()) + L")", InfoBarSeverity::Success, g_infoWindowInstance);
                LoadThreadList();
            }
            else slg::CreateInfoBarAndDisplay(L"失败", L"无法终止线程: " + item.Address() + L" (" + to_hstring(item.Id()) + L"), 错误码: " + to_hstring((int)GetLastError()), InfoBarSeverity::Error, g_infoWindowInstance);
            co_return;
            });

        // 选项1.3
        auto item1_3 = slg::CreateMenuItem(flyoutStyles, L"\ue945", L"终止 (内存抹杀)", [this, item](IInspectable const& sender, RoutedEventArgs const& e) -> winrt::Windows::Foundation::IAsyncAction {
            if (safeAcceptedPID == item.Id() || !dangerous_confirm) {
                if (KernelInstance::MurderThread(item.Id())) {
                    slg::CreateInfoBarAndDisplay(L"成功", L"成功终止线程: " + item.Address() + L" (" + to_hstring(item.Id()) + L")", InfoBarSeverity::Success, g_infoWindowInstance);
                    LoadThreadList();
                }
                else slg::CreateInfoBarAndDisplay(L"失败", L"无法终止线程: " + item.Address() + L" (" + to_hstring(item.Id()) + L"), 错误码: " + to_hstring((int)GetLastError()), InfoBarSeverity::Error, g_infoWindowInstance);
            }
            else {
                safeAcceptedPID = item.Id();
                slg::CreateInfoBarAndDisplay(L"警告", L"该操作可能导致系统崩溃或进程数据丢失，如果确认继续请再次点击！", InfoBarSeverity::Warning, g_infoWindowInstance);
            }
            co_return;
            });

        // 分割线1
        MenuFlyoutSeparator separator1;

        // 选项2.1
        auto item2_1 = slg::CreateMenuSubItem(flyoutStyles, L"\ue912", L"设置状态");
        auto item2_1_sub1 = slg::CreateMenuItem(flyoutStyles, L"\ue769", L"暂停", [this, item](IInspectable const& sender, RoutedEventArgs const& e) -> winrt::Windows::Foundation::IAsyncAction {
            if (KernelInstance::_SuspendThread(item.Id())) {
                slg::CreateInfoBarAndDisplay(L"成功", L"成功暂停线程: " + item.Address() + L" (" + to_hstring(item.Id()) + L")", InfoBarSeverity::Success, g_infoWindowInstance);
                LoadThreadList();
            }
            else slg::CreateInfoBarAndDisplay(L"失败", L"无法暂停线程: " + item.Address() + L" (" + to_hstring(item.Id()) + L"), 错误码: " + to_hstring((int)GetLastError()), InfoBarSeverity::Error, g_infoWindowInstance);
            co_return;
            });
        item2_1.Items().Append(item2_1_sub1);
        auto item2_1_sub2 = slg::CreateMenuItem(flyoutStyles, L"\ue768", L"恢复", [this, item](IInspectable const& sender, RoutedEventArgs const& e) -> winrt::Windows::Foundation::IAsyncAction {
            if (KernelInstance::_ResumeThread(item.Id())) {
                slg::CreateInfoBarAndDisplay(L"成功", L"成功恢复进程: " + item.Address() + L" (" + to_hstring(item.Id()) + L")", InfoBarSeverity::Success, g_infoWindowInstance);
                LoadThreadList();
            }
            else slg::CreateInfoBarAndDisplay(L"失败", L"无法恢复进程: " + item.Address() + L" (" + to_hstring(item.Id()) + L"), 错误码: " + to_hstring((int)GetLastError()), InfoBarSeverity::Error, g_infoWindowInstance);
            co_return;
            });
        item2_1.Items().Append(item2_1_sub2);

        // 分割线2
        MenuFlyoutSeparator separator2;

        // 选项3.1
        auto item3_1 = slg::CreateMenuSubItem(flyoutStyles, L"\ue8c8", L"复制信息");
        auto item3_1_sub1 = slg::CreateMenuItem(flyoutStyles, L"\ue943", L"TID", [this, item](IInspectable const& sender, RoutedEventArgs const& e) -> winrt::Windows::Foundation::IAsyncAction {
            if (TaskUtils::CopyToClipboard(std::to_wstring(item.Id()))) {
                slg::CreateInfoBarAndDisplay(L"成功", L"已复制内容至剪贴板", InfoBarSeverity::Success, g_infoWindowInstance);
            }
            else slg::CreateInfoBarAndDisplay(L"失败", L"无法复制内容至剪贴板, 错误码: " + to_hstring((int)GetLastError()), InfoBarSeverity::Error, g_infoWindowInstance);
            co_return;
            });
        item3_1.Items().Append(item3_1_sub1);
        auto item3_1_sub2 = slg::CreateMenuItem(flyoutStyles, L"\ueb19", L"ETHREAD", [this, item](IInspectable const& sender, RoutedEventArgs const& e) -> winrt::Windows::Foundation::IAsyncAction {
            if (TaskUtils::CopyToClipboard(item.EThread().c_str())) {
                slg::CreateInfoBarAndDisplay(L"成功", L"已复制内容至剪贴板", InfoBarSeverity::Success, g_infoWindowInstance);
            }
            else slg::CreateInfoBarAndDisplay(L"失败", L"无法复制内容至剪贴板, 错误码: " + to_hstring((int)GetLastError()), InfoBarSeverity::Error, g_infoWindowInstance);
            co_return;
            });
        item3_1.Items().Append(item3_1_sub2);
        auto item3_1_sub3 = slg::CreateMenuItem(flyoutStyles, L"\ueb1d", L"地址", [this, item](IInspectable const& sender, RoutedEventArgs const& e) -> winrt::Windows::Foundation::IAsyncAction {
            if (TaskUtils::CopyToClipboard(item.Address().c_str())) {
                slg::CreateInfoBarAndDisplay(L"成功", L"已复制内容至剪贴板", InfoBarSeverity::Success, g_infoWindowInstance);
            }
            else slg::CreateInfoBarAndDisplay(L"失败", L"无法复制内容至剪贴板, 错误码: " + to_hstring((int)GetLastError()), InfoBarSeverity::Error, g_infoWindowInstance);
            co_return;
            });
        item3_1.Items().Append(item3_1_sub3);
        
        menuFlyout.Items().Append(itemRefresh);
        menuFlyout.Items().Append(separatorR);
        menuFlyout.Items().Append(item1_1);
        menuFlyout.Items().Append(item1_2);
        menuFlyout.Items().Append(item1_3);
        menuFlyout.Items().Append(separator1);
        menuFlyout.Items().Append(item2_1);
        menuFlyout.Items().Append(separator2);
        menuFlyout.Items().Append(item3_1);

        slg::ShowAt(menuFlyout, listView, e);
    }

    void Process_ThreadPage::ThreadListView_ContainerContentChanging(
        winrt::Microsoft::UI::Xaml::Controls::ListViewBase const& sender,
        winrt::Microsoft::UI::Xaml::Controls::ContainerContentChangingEventArgs const& args)
    {

    }

    winrt::Windows::Foundation::IAsyncAction Process_ThreadPage::LoadThreadList()
    {
        if (!processForInfoWindow) co_return;

        LOG_INFO(__WFUNCTION__, L"Loading thread list... (pid=%d)", processForInfoWindow.Id());
        m_threadList.Clear();
        LoadingRing().IsActive(true);

        auto start = std::chrono::high_resolution_clock::now();

        auto lifetime = get_strong();

        co_await winrt::resume_background();

        std::vector<winrt::StarlightGUI::ThreadInfo> threads;
        threads.reserve(100);

        // 获取线程列表
        KernelInstance::EnumProcessThreads(processForInfoWindow.EProcessULong(), threads);
        LOG_INFO(__WFUNCTION__, L"Enumerated threads, %d entry(s).", threads.size());

        co_await wil::resume_foreground(DispatcherQueue());

        for (const auto& thread : threads) {
            if (thread.ModuleInfo().empty()) thread.ModuleInfo(L"(未知)");

            m_threadList.Append(thread);
        }

        ApplySort(currentSortingOption, currentSortingType);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        // 更新线程数量文本
        std::wstringstream countText;
        countText << L"共 " << m_threadList.Size() << L" 个线程 (" << duration.count() << " ms)";
        ThreadCountText().Text(countText.str());
        LoadingRing().IsActive(false);

        LOG_INFO(__WFUNCTION__, L"Loaded thread list, %d entry(s) in total.", m_threadList.Size());
    }


    void Process_ThreadPage::ColumnHeader_Click(IInspectable const& sender, RoutedEventArgs const& e)
    {
        Button clickedButton = sender.as<Button>();
        winrt::hstring columnName = clickedButton.Tag().as<winrt::hstring>();

        struct SortBinding {
            wchar_t const* tag;
            char const* column;
            bool* ascending;
        };

        static const std::array<SortBinding, 4> bindings{ {
            { L"Id", "Id", &Process_ThreadPage::m_isIdAscending },
            { L"EThread", "EThread", &Process_ThreadPage::m_isEThreadAscending },
            { L"Address", "Address", &Process_ThreadPage::m_isAddressAscending },
            { L"Priority", "Priority", &Process_ThreadPage::m_isPriorityAscending },
        } };

        for (auto const& binding : bindings) {
            if (columnName == binding.tag) {
                ApplySort(*binding.ascending, binding.column);
                break;
            }
        }
    }

    // 排序切换
    void Process_ThreadPage::ApplySort(bool& isAscending, const std::string& column)
    {
        SortThreadList(isAscending, column, true);

        isAscending = !isAscending;
        currentSortingOption = !isAscending;
        currentSortingType = column;
    }

    void Process_ThreadPage::SortThreadList(bool isAscending, const std::string& column, bool updateHeader)
    {
        if (column.empty()) return;

        enum class SortColumn {
            Unknown,
            Id,
            EThread,
            Address,
            Priority
        };

        auto resolveSortColumn = [&](const std::string& key) -> SortColumn {
            if (key == "Id") return SortColumn::Id;
            if (key == "EThread") return SortColumn::EThread;
            if (key == "Address") return SortColumn::Address;
            if (key == "Priority") return SortColumn::Priority;
            return SortColumn::Unknown;
            };

        auto activeColumn = resolveSortColumn(column);
        if (activeColumn == SortColumn::Unknown) return;

        if (updateHeader) {
            IdHeaderButton().Content(box_value(L"TID"));
            EThreadHeaderButton().Content(box_value(L"ETHREAD"));
            AddressHeaderButton().Content(box_value(L"地址"));
            PriorityHeaderButton().Content(box_value(L"优先级"));

            if (activeColumn == SortColumn::Id) IdHeaderButton().Content(box_value(isAscending ? L"TID ↓" : L"TID ↑"));
            if (activeColumn == SortColumn::EThread) EThreadHeaderButton().Content(box_value(isAscending ? L"ETHREAD ↓" : L"ETHREAD ↑"));
            if (activeColumn == SortColumn::Address) AddressHeaderButton().Content(box_value(isAscending ? L"地址 ↓" : L"地址 ↑"));
            if (activeColumn == SortColumn::Priority) PriorityHeaderButton().Content(box_value(isAscending ? L"优先级 ↓" : L"优先级 ↑"));
        }

        std::vector<winrt::StarlightGUI::ThreadInfo> sortedThreads;
        sortedThreads.reserve(m_threadList.Size());
        for (auto const& process : m_threadList) {
            sortedThreads.push_back(process);
        }

        auto parseHex = [](winrt::hstring const& text) -> ULONG64 {
            ULONG64 value = 0;
            if (HexStringToULong(text.c_str(), value)) return value;
            return 0;
            };

        auto sortActiveColumn = [&](const winrt::StarlightGUI::ThreadInfo& a, const winrt::StarlightGUI::ThreadInfo& b) -> bool {
            switch (activeColumn) {
            case SortColumn::Id:
                return a.Id() < b.Id();
            case SortColumn::EThread:
            {
                auto aValue = parseHex(a.EThread());
                auto bValue = parseHex(b.EThread());
                if (aValue != bValue) return aValue < bValue;
                return a.EThread() < b.EThread();
            }
            case SortColumn::Address:
            {
                auto aValue = parseHex(a.Address());
                auto bValue = parseHex(b.Address());
                if (aValue != bValue) return aValue < bValue;
                return a.Address() < b.Address();
            }
            case SortColumn::Priority:
                return a.Priority() < b.Priority();
            default:
                return false;
            }
            };

        if (isAscending) {
            std::sort(sortedThreads.begin(), sortedThreads.end(), sortActiveColumn);
        }
        else {
            std::sort(sortedThreads.begin(), sortedThreads.end(), [&](const auto& a, const auto& b) {
                return sortActiveColumn(b, a);
                });
        }

        m_threadList.Clear();
        for (auto& thread : sortedThreads) {
            m_threadList.Append(thread);
        }
    }
}


