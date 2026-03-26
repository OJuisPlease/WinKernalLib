#include "pch.h"
#include "LogConsolePage.xaml.h"
#if __has_include("LogConsolePage.g.cpp")
#include "LogConsolePage.g.cpp"
#endif

#include <MainWindow.xaml.h>
#include <sstream>
#include <iomanip>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Windows::Foundation;

namespace winrt::StarlightGUI::implementation
{
    LogConsolePage::LogConsolePage()
    {
        InitializeComponent();

        LogConsoleTitleUid().Text(GetLocalizedString(L"LogConsole_Title.Text"));
        ClearButton().Label(GetLocalizedString(L"LogConsole_Clear.Label"));
        SaveButton().Label(GetLocalizedString(L"LogConsole_Save.Label"));
        AutoScrollButton().Label(GetLocalizedString(L"LogConsole_AutoScroll.Label"));
        LogSearchBox().PlaceholderText(GetLocalizedString(L"LogConsole_SearchBox.PlaceholderText"));
        TimestampHeaderButton().Content(tbox(L"LogConsole_ColTimestamp.Content"));
        LevelHeaderButton().Content(tbox(L"LogConsole_ColLevel.Content"));
        SourceHeaderButton().Content(tbox(L"LogConsole_ColSource.Content"));
        MessageHeaderButton().Content(tbox(L"LogConsole_ColMessage.Content"));

        LogListView().ItemsSource(m_logList);

        // 当列宽变化时（GridSplitter 拖拽），同步 DataTemplate 内的列宽
        auto columnResizedHandler = [weak = get_weak()](auto&&, auto&&) {
            if (auto self = weak.get()) {
                self->SyncItemColumnWidths();
            }
        };
        TimestampHeaderButton().SizeChanged(columnResizedHandler);
        LevelHeaderButton().SizeChanged(columnResizedHandler);
        SourceHeaderButton().SizeChanged(columnResizedHandler);
        MessageHeaderButton().SizeChanged(columnResizedHandler);

        this->Loaded([this](auto&&, auto&&) {
            LoadLogEntries();
            StartPollingTimer();
        });

        this->Unloaded([this](auto&&, auto&&) {
            StopPollingTimer();
        });

        LOG_INFO(L"LogConsolePage", L"LogConsolePage initialized.");
    }

    void LogConsolePage::StartPollingTimer()
    {
        if (!m_pollTimer) {
            m_pollTimer = winrt::Microsoft::UI::Xaml::DispatcherTimer();
            m_pollTimer.Interval(std::chrono::milliseconds(500));
            m_pollTimer.Tick([weak = get_weak()](auto&&, auto&&) {
                if (auto self = weak.get()) {
                    self->PollNewEntries();
                }
            });
        }
        m_pollTimer.Start();
    }

    void LogConsolePage::StopPollingTimer()
    {
        if (m_pollTimer) {
            m_pollTimer.Stop();
        }
    }

    void LogConsolePage::PollNewEntries()
    {
        auto history = Console::GetInstance().GetLogHistory();
        if (history.size() == m_lastKnownSize) return;

        winrt::hstring query = LogSearchBox().Text();
        std::wstring lowerQuery;
        if (!query.empty()) lowerQuery = ToLowerCase(query.c_str());

        if (history.size() > m_lastKnownSize) {
            for (size_t i = m_lastKnownSize; i < history.size(); ++i) {
                m_allEntries.push_back(history[i]);
                if (PassesFilter(history[i], lowerQuery)) {
                    m_logList.Append(ConvertLogEntry(history[i]));
                }
            }
        }
        else {
            m_allEntries.assign(history.begin(), history.end());
            ApplyFilters();
        }

        m_lastKnownSize = history.size();

        LogCountText().Text(t(L"LogConsole_EntryCount", m_logList.Size()));

        if (AutoScrollButton().IsChecked().GetBoolean() && m_logList.Size() > 0) {
            LogListView().ScrollIntoView(m_logList.GetAt(m_logList.Size() - 1));
        }
    }

    void LogConsolePage::LoadLogEntries()
    {
        m_allEntries = Console::GetInstance().GetLogHistory();
        m_lastKnownSize = m_allEntries.size();
        ApplyFilters();
    }

    void LogConsolePage::ApplyFilters()
    {
        m_logList.Clear();

        winrt::hstring query = LogSearchBox().Text();
        std::wstring lowerQuery;
        if (!query.empty()) lowerQuery = ToLowerCase(query.c_str());

        for (const auto& entry : m_allEntries) {
            if (PassesFilter(entry, lowerQuery)) {
                m_logList.Append(ConvertLogEntry(entry));
            }
        }

        LogCountText().Text(t(L"LogConsole_EntryCount", m_logList.Size()));

        if (AutoScrollButton().IsChecked().GetBoolean() && m_logList.Size() > 0) {
            LogListView().ScrollIntoView(m_logList.GetAt(m_logList.Size() - 1));
        }
    }

    bool LogConsolePage::PassesFilter(const LogEntry& entry, const std::wstring& lowerQuery)
    {
        switch (entry.level) {
        case LogLevel::INFO:    if (!m_filterInfo) return false; break;
        case LogLevel::WARNING: if (!m_filterWarning) return false; break;
        case LogLevel::ERROR:   if (!m_filterError) return false; break;
        case LogLevel::OTHER:   if (!m_filterOther) return false; break;
        }

        if (!lowerQuery.empty()) {
            if (!ContainsIgnoreCaseLowerQuery(entry.message.c_str(), lowerQuery) &&
                !ContainsIgnoreCaseLowerQuery(entry.source.c_str(), lowerQuery)) {
                return false;
            }
        }

        return true;
    }

    winrt::StarlightGUI::LogEntryInfo LogConsolePage::ConvertLogEntry(const LogEntry& entry)
    {
        auto info = winrt::make<winrt::StarlightGUI::implementation::LogEntryInfo>();

        auto tt = std::chrono::system_clock::to_time_t(entry.timestamp);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(entry.timestamp.time_since_epoch()) % 1000;
        struct tm tmBuf {};
        localtime_s(&tmBuf, &tt);

        wchar_t timeBuf[32]{};
        swprintf_s(timeBuf, L"%02d:%02d:%02d.%03lld",
            tmBuf.tm_hour, tmBuf.tm_min, tmBuf.tm_sec, static_cast<long long>(ms.count()));
        info.Timestamp(timeBuf);

        switch (entry.level) {
        case LogLevel::INFO:    info.Level(L"INFO");    info.LevelInt(0); break;
        case LogLevel::WARNING: info.Level(L"WARN");    info.LevelInt(1); break;
        case LogLevel::ERROR:   info.Level(L"ERROR");   info.LevelInt(2); break;
        case LogLevel::OTHER:   info.Level(L"OTHER");   info.LevelInt(3); break;
        }

        info.Source(entry.source);
        info.Message(entry.message);

        return info;
    }

    void LogConsolePage::ClearButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        m_allEntries.clear();
        m_logList.Clear();
        m_lastKnownSize = Console::GetInstance().GetLogHistory().size();
        LogCountText().Text(t(L"LogConsole_EntryCount", 0u));
    }

    void LogConsolePage::SaveButton_Click(IInspectable const&, RoutedEventArgs const&)
    {
        wchar_t tempPath[MAX_PATH]{};
        GetTempPathW(MAX_PATH, tempPath);
        std::wstring filePath = std::wstring(tempPath) + L"StarlightGUI.log";

        if (Console::GetInstance().SaveToFile(filePath)) {
            slg::CreateInfoBarAndDisplay(
                GetLocalizedString(L"Msg_Success").c_str(),
                GetLocalizedString(L"LogConsole_SaveSuccess").c_str(),
                InfoBarSeverity::Success, g_mainWindowInstance);
        }
        else {
            slg::CreateInfoBarAndDisplay(
                GetLocalizedString(L"Msg_Failure").c_str(),
                GetLocalizedString(L"LogConsole_SaveFailed").c_str(),
                InfoBarSeverity::Error, g_mainWindowInstance);
        }
    }

    void LogConsolePage::FilterButton_Click(IInspectable const& sender, RoutedEventArgs const&)
    {
        auto button = sender.as<Controls::Primitives::ToggleButton>();
        auto tag = unbox_value<hstring>(button.Tag());

        if (tag == L"INFO")         m_filterInfo = button.IsChecked().GetBoolean();
        else if (tag == L"WARNING") m_filterWarning = button.IsChecked().GetBoolean();
        else if (tag == L"ERROR")   m_filterError = button.IsChecked().GetBoolean();
        else if (tag == L"OTHER")   m_filterOther = button.IsChecked().GetBoolean();

        ApplyFilters();
    }

    void LogConsolePage::LogSearchBox_TextChanged(IInspectable const&, RoutedEventArgs const&)
    {
        ApplyFilters();
    }

    void LogConsolePage::LogListView_RightTapped(IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const& e)
    {
        auto listView = LogListView();
        slg::SelectItemOnRightTapped(listView, e);

        if (!listView.SelectedItem()) return;

        auto item = listView.SelectedItem().as<winrt::StarlightGUI::LogEntryInfo>();
        auto flyoutStyles = slg::GetStyles();

        MenuFlyout menuFlyout;

        auto copyMsg = slg::CreateMenuItem(flyoutStyles, L"\ue8c8", GetLocalizedString(L"LogConsole_CopyMessage").c_str(),
            [this, item](IInspectable const&, RoutedEventArgs const&) -> IAsyncAction {
                if (TaskUtils::CopyToClipboard(item.Message().c_str())) {
                    slg::CreateInfoBarAndDisplay(GetLocalizedString(L"Msg_Success").c_str(), GetLocalizedString(L"Msg_CopiedToClipboard").c_str(), InfoBarSeverity::Success, g_mainWindowInstance);
                }
                co_return;
            });

        auto copyLine = slg::CreateMenuItem(flyoutStyles, L"\ue8c8", GetLocalizedString(L"LogConsole_CopyLine").c_str(),
            [this, item](IInspectable const&, RoutedEventArgs const&) -> IAsyncAction {
                auto line = std::wstring(L"[") + item.Timestamp().c_str() + L"] [" + item.Level().c_str() + L"] [" + item.Source().c_str() + L"] " + item.Message().c_str();
                if (TaskUtils::CopyToClipboard(line.c_str())) {
                    slg::CreateInfoBarAndDisplay(GetLocalizedString(L"Msg_Success").c_str(), GetLocalizedString(L"Msg_CopiedToClipboard").c_str(), InfoBarSeverity::Success, g_mainWindowInstance);
                }
                co_return;
            });

        menuFlyout.Items().Append(copyMsg);
        menuFlyout.Items().Append(copyLine);

        slg::ShowAt(menuFlyout, listView, e);
    }

    void LogConsolePage::SyncItemColumnWidths()
    {
        // 读取共享 Grid 中各列的实际宽度（包含 splitter 间距）
        auto splitterWidth = TableGrid().ColumnDefinitions().GetAt(1).ActualWidth();
        m_colTimestampWidth = ColTimestamp().ActualWidth() + splitterWidth;
        m_colLevelWidth = ColLevel().ActualWidth() + splitterWidth;
        m_colSourceWidth = ColSource().ActualWidth() + splitterWidth;

        // 更新所有已实现的 ListView 项
        for (uint32_t i = 0; i < m_logList.Size(); ++i) {
            auto container = LogListView().ContainerFromIndex(i);
            if (!container) continue;
            auto lvi = container.try_as<ListViewItem>();
            if (lvi) ApplyColumnWidthsToItem(lvi);
        }
    }

    void LogConsolePage::ApplyColumnWidthsToItem(ListViewItem const& item)
    {
        auto grid = item.ContentTemplateRoot().try_as<Grid>();
        if (!grid) return;

        auto cols = grid.ColumnDefinitions();
        if (cols.Size() < 4) return;

        cols.GetAt(0).Width(GridLengthHelper::FromPixels(m_colTimestampWidth));
        cols.GetAt(1).Width(GridLengthHelper::FromPixels(m_colLevelWidth));
        cols.GetAt(2).Width(GridLengthHelper::FromPixels(m_colSourceWidth));
        // Column 3 stays as * (auto-fill remaining)
    }

    void LogConsolePage::LogListView_ContainerContentChanging(
        ListViewBase const&,
        ContainerContentChangingEventArgs const& args)
    {
        if (args.InRecycleQueue()) return;

        auto lvi = args.ItemContainer().try_as<ListViewItem>();
        if (lvi) ApplyColumnWidthsToItem(lvi);
    }
}
