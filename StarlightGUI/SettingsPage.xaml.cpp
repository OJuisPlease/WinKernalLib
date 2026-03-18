#include "pch.h"
#include "SettingsPage.xaml.h"
#if __has_include("SettingsPage.g.cpp")
#include "SettingsPage.g.cpp"
#endif

#include "Utils/Config.h"
#include "MainWindow.xaml.h"

using namespace winrt;
using namespace Windows::Storage;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::Windows::Storage::Pickers;

namespace winrt::StarlightGUI::implementation
{
    static bool loaded;

    SettingsPage::SettingsPage()
    {
        InitializeComponent();
        InitializeOptions();
    }

    void SettingsPage::InitializeOptions() {
        EnumFileModeComboBox().SelectedIndex(enum_file_mode);
        BackgroundComboBox().SelectedIndex(background_type);
        NavigationComboBox().SelectedIndex(navigation_style);
        MicaTypeComboBox().SelectedIndex(mica_type);
        AcrylicTypeComboBox().SelectedIndex(acrylic_type);
        ImageStretchComboBox().SelectedIndex(image_stretch);

        EnumStrengthenButton().IsOn(enum_strengthen);
        PDHFirstButton().IsOn(pdh_first);
		ListRevealFocusButton().IsOn(list_revealfocus);
		ElevatedRunButton().IsOn(elevated_run);
        DangerousConfirmButton().IsOn(dangerous_confirm);
        CheckUpdateButton().IsOn(check_update);

        ImagePathText().Text(to_hstring(background_image));
        ImageOpacitySlider().Value(image_opacity);
		DisasmCountSlider().Value(disasm_count);
    }

    void SettingsPage::EnumFileModeComboBox_SelectionChanged(IInspectable const& sender, SelectionChangedEventArgs const& e)
    {
        if (!IsLoaded()) return;
        if (slg::CheckIllegalComboBoxAction(sender, e)) return;

        enum_file_mode = (int)EnumFileModeComboBox().SelectedIndex();
        SaveConfig("enum_file_mode", (int)EnumFileModeComboBox().SelectedIndex());
    }

    void SettingsPage::EnumStrengthenButton_Toggled(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e) {
        if (!IsLoaded()) return;
		enum_strengthen = EnumStrengthenButton().IsOn();
        SaveConfig("enum_strengthen", enum_strengthen);
    }

    void SettingsPage::PDHFirstButton_Toggled(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e) {
        if (!IsLoaded()) return;
        pdh_first = PDHFirstButton().IsOn();
        SaveConfig("pdh_first", pdh_first);
    }

    void SettingsPage::BackgroundComboBox_SelectionChanged(IInspectable const& sender, SelectionChangedEventArgs const& e)
    {
        if (!IsLoaded()) return;
        if (slg::CheckIllegalComboBoxAction(sender, e)) return;

        background_type = (int)BackgroundComboBox().SelectedIndex();
        SaveConfig("background_type", (int)BackgroundComboBox().SelectedIndex());

        g_mainWindowInstance->LoadBackdrop();

        Console::GetInstance().SetBackdropByConfig();
    }

    void SettingsPage::MicaTypeComboBox_SelectionChanged(IInspectable const& sender, SelectionChangedEventArgs const& e)
    {
        if (!IsLoaded()) return;
        if (slg::CheckIllegalComboBoxAction(sender, e)) return;

        mica_type = (int)MicaTypeComboBox().SelectedIndex();

        SaveConfig("mica_type", (int)MicaTypeComboBox().SelectedIndex());

        g_mainWindowInstance->LoadBackdrop();
    }

    void SettingsPage::AcrylicTypeComboBox_SelectionChanged(IInspectable const& sender, SelectionChangedEventArgs const& e)
    {
        if (!IsLoaded()) return;
        if (slg::CheckIllegalComboBoxAction(sender, e)) return;

        acrylic_type = (int)AcrylicTypeComboBox().SelectedIndex();

        SaveConfig("acrylic_type", (int)AcrylicTypeComboBox().SelectedIndex());

        g_mainWindowInstance->LoadBackdrop();
    }
    
    void SettingsPage::NavigationComboBox_SelectionChanged(IInspectable const& sender, SelectionChangedEventArgs const& e)
    {
        if (!IsLoaded()) return;
        if (slg::CheckIllegalComboBoxAction(sender, e)) return;

        navigation_style = (int)NavigationComboBox().SelectedIndex();
        SaveConfig("navigation_style", (int)NavigationComboBox().SelectedIndex());

        g_mainWindowInstance->LoadNavigation();
    }

    void SettingsPage::ListRevealFocusButton_Toggled(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        if (!IsLoaded()) return;
        list_revealfocus = ListRevealFocusButton().IsOn();
        SaveConfig("list_revealfocus", list_revealfocus);
    }

    void SettingsPage::ElevatedRunButton_Toggled(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        if (!IsLoaded()) return;
        slg::CreateInfoBarAndDisplay(L"提示", L"该设置需要重启以生效!", InfoBarSeverity::Informational, g_mainWindowInstance);
        elevated_run = ElevatedRunButton().IsOn();
        SaveConfig("elevated_run", elevated_run);
    }

    void SettingsPage::DangerousConfirmButton_Toggled(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        if (!IsLoaded()) return;
        dangerous_confirm = DangerousConfirmButton().IsOn();
        SaveConfig("dangerous_confirm", dangerous_confirm);
    }

    void SettingsPage::CheckUpdateButton_Toggled(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        if (!IsLoaded()) return;
		slg::CreateInfoBarAndDisplay(L"提示", L"该设置需要重启以生效!", InfoBarSeverity::Informational, g_mainWindowInstance);
        check_update = CheckUpdateButton().IsOn();
        SaveConfig("check_update", check_update);
    }

    void SettingsPage::ClearImageButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e) {
        if (!IsLoaded()) return;
        SaveConfig("background_image", "");
        ImagePathText().Text(L"");

        g_mainWindowInstance->LoadBackground();
    }

    slg::coroutine SettingsPage::SetImageButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e) {
        if (!IsLoaded()) co_return;
        HWND hWnd = g_mainWindowInstance->GetWindowHandle();

        FileOpenPicker picker = FileOpenPicker(winrt::Microsoft::UI::GetWindowIdFromWindow(hWnd));

        picker.SuggestedStartLocation(PickerLocationId::ComputerFolder);
        picker.FileTypeFilter().Append(L".png");
        picker.FileTypeFilter().Append(L".jpg");
        picker.FileTypeFilter().Append(L".bmp");
        picker.FileTypeFilter().Append(L".jpeg");

        auto result = co_await picker.PickSingleFileAsync();

        if (!result) co_return;

        try {
            auto file = co_await StorageFile::GetFileFromPathAsync(result.Path());

            if (file && file.IsAvailable() && (file.FileType() == L".png" || file.FileType() == L".jpg" || file.FileType() == L".bmp" || file.FileType() == L".jpeg")) {
                std::string path = WideStringToString(file.Path().c_str());
                SaveConfig("background_image", path);
                ImagePathText().Text(to_hstring(background_image));

                g_mainWindowInstance->LoadBackground();
            }
        }
        catch (hresult_error) {

        }
    }


    void SettingsPage::RefreshOpacityButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e) {
        if (!IsLoaded()) return;
        g_mainWindowInstance->LoadBackground();
    }

    void SettingsPage::ImageStretchComboBox_SelectionChanged(IInspectable const& sender, SelectionChangedEventArgs const& e)
    {
        if (!IsLoaded()) return;
        if (slg::CheckIllegalComboBoxAction(sender, e)) return;

        image_stretch = (int)ImageStretchComboBox().SelectedIndex();
        SaveConfig("image_stretch", (int)ImageStretchComboBox().SelectedIndex());

        g_mainWindowInstance->LoadBackground();
    }

    void SettingsPage::LogButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        if (!IsLoaded()) return;
        LOGGER_TOGGLE();
    }

    void SettingsPage::FixButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        if (!IsLoaded()) return;
        slg::CreateInfoBarAndDisplay(L"提示", L"该设置需要重启以生效!", InfoBarSeverity::Informational, g_mainWindowInstance);
        DriverUtils::FixServices();
    }

    void SettingsPage::ImageOpacitySlider_ValueChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs const& e)
    {
        if (!IsLoaded()) return;
        SaveConfig("image_opacity", ImageOpacitySlider().Value());
    }

    void SettingsPage::DisasmCountSlider_ValueChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs const& e)
    {
        if (!IsLoaded()) return;
        SaveConfig("disasm_count", DisasmCountSlider().Value());
    }
}
