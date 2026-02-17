#pragma once

#include "pch.h"
#include "MainWindow.xaml.h"
#include "InfoWindow.xaml.h"

using namespace winrt;
using namespace Windows::UI;
using namespace Windows::Foundation;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Media;
using namespace Microsoft::UI::Xaml::Markup;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media::Imaging;
using namespace Microsoft::UI::Xaml::Media::Animation;


namespace winrt::StarlightGUI::implementation {
    FontIcon CreateFontIcon(hstring glyph);

    InfoBar CreateInfoBar(hstring title, hstring message, InfoBarSeverity severity, XamlRoot xamlRoot);

    void DisplayInfoBar(InfoBar infobar, Panel parent, int time = 1500);

    void CreateInfoBarAndDisplay(hstring title, hstring message, InfoBarSeverity severity, XamlRoot xamlRoot, Panel parent, int time = 1500);

    void CreateInfoBarAndDisplay(hstring title, hstring message, InfoBarSeverity severity, winrt::StarlightGUI::implementation::MainWindow* instance, int time = 1500);

    void CreateInfoBarAndDisplay(hstring title, hstring message, InfoBarSeverity severity, winrt::StarlightGUI::implementation::InfoWindow* instance, int time = 1500);

    ContentDialog CreateContentDialog(hstring title, hstring content, hstring closeMessage, XamlRoot xamlRoot);

    DataTemplate GetContentDialogSuccessTemplate();

    DataTemplate GetContentDialogErrorTemplate();

    DataTemplate GetContentDialogInfoTemplate();

    DataTemplate GetTemplate(hstring xaml);

    bool CheckIllegalComboBoxAction(IInspectable const& sender, SelectionChangedEventArgs const& e);
}