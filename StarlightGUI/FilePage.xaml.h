#pragma once

#include "FilePage.g.h"

namespace winrt::StarlightGUI::implementation
{
    struct FilePage : FilePageT<FilePage>
    {
        FilePage();

        slg::coroutine RefreshButton_Click(IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        slg::coroutine NextDriveButton_Click(IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        void HandleExternalDropFiles(std::vector<std::wstring> const& paths);

        void FileListView_RightTapped(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const& e);
        void FileListView_DoubleTapped(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::DoubleTappedRoutedEventArgs const& e);
        void FileListView_DragOver(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::DragEventArgs const& e);
        void FileListView_Drop(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::DragEventArgs const& e);
        void FileListView_ContainerContentChanging(
            winrt::Microsoft::UI::Xaml::Controls::ListViewBase const& sender,
            winrt::Microsoft::UI::Xaml::Controls::ContainerContentChangingEventArgs const& args);

        void ColumnHeader_Click(IInspectable const& sender, RoutedEventArgs const& e);
        slg::coroutine ApplySort(bool& isAscending, const std::string& column);
        void SortFileList(bool isAscending, const std::string& column, bool updateHeader);

        void PathBox_KeyDown(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& e);
        void SearchBox_TextChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        bool ApplyFilter(const winrt::StarlightGUI::FileInfo& file, hstring& query);

        winrt::Windows::Foundation::IAsyncAction LoadFileList();
        winrt::Windows::Foundation::IAsyncAction WaitAndReloadAsync(int interval);
        winrt::Windows::Foundation::IAsyncAction GetFileIconAsync(winrt::StarlightGUI::FileInfo file);
        winrt::Windows::Foundation::IAsyncAction CopyDroppedPathsAsync(std::vector<std::wstring> paths);
        void PopulateFileMetaBatch(std::wstring const& directoryPath);
        static std::wstring GetIconCacheKey(winrt::StarlightGUI::FileInfo file);

        winrt::Windows::Foundation::Collections::IObservableVector<winrt::StarlightGUI::FileInfo> m_fileList{
            winrt::multi_threaded_observable_vector<winrt::StarlightGUI::FileInfo>()
        };

        slg::coroutine CopyFiles();

        winrt::Windows::Foundation::IAsyncAction LoadMetaForCurrentList(std::wstring path, uint64_t loadToken);
        void UpdateRealizedItemIcon(winrt::StarlightGUI::FileInfo const& file, winrt::Microsoft::UI::Xaml::Media::ImageSource const& icon);
        void ResetState();
        winrt::Microsoft::UI::Xaml::DispatcherTimer reloadTimer;
        std::vector<winrt::StarlightGUI::FileInfo> m_allFiles;
        bool m_isLoadingFiles = false;
        bool m_isPostLoading = false;
        uint64_t m_currentLoadToken = 0;

        inline static bool m_isNameAscending = true;
        inline static bool m_isModifyTimeAscending = true;
        inline static bool m_isSizeAscending = true;
        inline static bool currentSortingOption;
        inline static std::string currentSortingType;
    };

    extern winrt::hstring currentDirectory;
    extern FilePage* g_filePageInstance;
}

namespace winrt::StarlightGUI::factory_implementation
{
    struct FilePage : FilePageT<FilePage, implementation::FilePage>
    {
    };
}
