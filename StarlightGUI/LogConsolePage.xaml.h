#pragma once

#include "LogConsolePage.g.h"
#include <winrt/Windows.Foundation.Collections.h>

namespace winrt::StarlightGUI::implementation
{
    struct LogConsolePage : LogConsolePageT<LogConsolePage>
    {
        LogConsolePage();

        void ClearButton_Click(IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void SaveButton_Click(IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void FilterButton_Click(IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void LogSearchBox_TextChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void LogListView_RightTapped(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const& e);
        void LogListView_ContainerContentChanging(
            winrt::Microsoft::UI::Xaml::Controls::ListViewBase const& sender,
            winrt::Microsoft::UI::Xaml::Controls::ContainerContentChangingEventArgs const& args);

    private:
        void LoadLogEntries();
        void ApplyFilters();
        bool PassesFilter(const LogEntry& entry, const std::wstring& lowerQuery);
        winrt::StarlightGUI::LogEntryInfo ConvertLogEntry(const LogEntry& entry);
        void StartPollingTimer();
        void StopPollingTimer();
        void PollNewEntries();
        void SyncItemColumnWidths();
        void ApplyColumnWidthsToItem(winrt::Microsoft::UI::Xaml::Controls::ListViewItem const& item);

        winrt::Windows::Foundation::Collections::IObservableVector<winrt::StarlightGUI::LogEntryInfo> m_logList{
            winrt::single_threaded_observable_vector<winrt::StarlightGUI::LogEntryInfo>()
        };

        std::vector<LogEntry> m_allEntries;
        size_t m_lastKnownSize{ 0 };

        bool m_filterInfo{ true };
        bool m_filterWarning{ true };
        bool m_filterError{ true };
        bool m_filterOther{ true };

        winrt::Microsoft::UI::Xaml::DispatcherTimer m_pollTimer{ nullptr };

        double m_colTimestampWidth{ 160 };
        double m_colLevelWidth{ 80 };
        double m_colSourceWidth{ 160 };
    };
}

namespace winrt::StarlightGUI::factory_implementation
{
    struct LogConsolePage : LogConsolePageT<LogConsolePage, implementation::LogConsolePage>
    {
    };
}
