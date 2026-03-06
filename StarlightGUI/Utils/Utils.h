#pragma once

#include "pch.h"
#include "MainWindow.xaml.h"
#include "InfoWindow.xaml.h"
#include <coroutine>
#include <exception>
#include <winrt/Microsoft.UI.Xaml.Input.h>

using namespace winrt;
using namespace Windows::UI;
using namespace Windows::Foundation;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Media;
using namespace Microsoft::UI::Xaml::Markup;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media::Imaging;
using namespace Microsoft::UI::Xaml::Media::Animation;

namespace slg {
    struct coroutine {
        coroutine();

        struct promise_type {
            coroutine get_return_object() const noexcept;

            void return_void() const noexcept;

            std::suspend_never initial_suspend() const noexcept;

            std::suspend_never final_suspend() const noexcept;

            void unhandled_exception() const noexcept;
        };
    };

    struct Styles
    {
        winrt::Microsoft::UI::Xaml::Style Item;
        winrt::Microsoft::UI::Xaml::Style SubItem;
    };

    Styles GetStyles();

    MenuFlyoutItem CreateMenuItem(
        Styles const& styles,
        hstring const& glyph,
        hstring const& text,
        winrt::Microsoft::UI::Xaml::RoutedEventHandler const& click = nullptr);

    MenuFlyoutItem CreateMenuItem(
        Styles const& styles,
        hstring const& text,
        winrt::Microsoft::UI::Xaml::RoutedEventHandler const& click = nullptr);

    MenuFlyoutSubItem CreateMenuSubItem(
        Styles const& styles,
        hstring const& glyph,
        hstring const& text);

    MenuFlyoutSubItem CreateMenuSubItem(
        Styles const& styles,
        hstring const& text);

    void ShowAt(
        winrt::Microsoft::UI::Xaml::Controls::MenuFlyout const& flyout,
        winrt::Microsoft::UI::Xaml::Controls::ListView const& listView,
        winrt::Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const& e);

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
