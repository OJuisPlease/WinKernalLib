#include "pch.h"
#include "Utils.h"
#include <winrt/XamlToolkit.WinUI.Controls.h>

namespace slg {
    coroutine::coroutine() = default;

    coroutine coroutine::promise_type::get_return_object() const noexcept { return {}; }

    void coroutine::promise_type::return_void() const noexcept {}

    std::suspend_never coroutine::promise_type::initial_suspend() const noexcept { return {}; }

    std::suspend_never coroutine::promise_type::final_suspend() const noexcept { return {}; }

    void coroutine::promise_type::unhandled_exception() const noexcept
    {
        try {
            std::rethrow_exception(std::current_exception());
        }
        catch (const winrt::hresult_error& e) {
            LOG_ERROR(L"App", L"===== Unhandled exception detected! =====");
            LOG_ERROR(L"App", L"Type: 'winrt::hresult_error'");
            LOG_ERROR(L"App", L"Code: %d", e.code().value);
            LOG_ERROR(L"App", L"Message: %s", e.message().c_str());
            LOG_ERROR(L"App", L"=========================================");
        }
        catch (const std::exception& e) {
            LOG_ERROR(L"App", L"===== Unhandled exception detected! =====");
            LOG_ERROR(L"App", L"Type: 'std::exception'");
            LOG_ERROR(L"App", L"Message: %hs", e.what());
            LOG_ERROR(L"App", L"=========================================");
        }
        catch (...) {
            LOG_ERROR(L"App", L"===== Unhandled exception detected! =====");
            LOG_ERROR(L"App", L"Type: OTHER/UNKNOWN");
            LOG_ERROR(L"App", L"This should not happen!");
            LOG_ERROR(L"App", L"=========================================");
        }
    }
    Styles GetStyles()
    {
        auto resources = winrt::Microsoft::UI::Xaml::Application::Current().Resources();
        return {
            winrt::unbox_value<winrt::Microsoft::UI::Xaml::Style>(resources.TryLookup(winrt::box_value(L"MenuFlyoutItemStyle"))),
            winrt::unbox_value<winrt::Microsoft::UI::Xaml::Style>(resources.TryLookup(winrt::box_value(L"MenuFlyoutSubItemStyle")))
        };
    }

    MenuFlyoutItem CreateMenuItem(
        Styles const& styles,
        hstring const& glyph,
        hstring const& text,
        winrt::Microsoft::UI::Xaml::RoutedEventHandler const& click)
    {
        MenuFlyoutItem item;
        item.Style(styles.Item);
        item.Icon(CreateFontIcon(glyph));
        item.Text(text);
        if (click) item.Click(click);
        return item;
    }

    MenuFlyoutItem CreateMenuItem(
        Styles const& styles,
        hstring const& text,
        winrt::Microsoft::UI::Xaml::RoutedEventHandler const& click)
    {
        MenuFlyoutItem item;
        item.Style(styles.Item);
        item.Text(text);
        if (click) item.Click(click);
        return item;
    }

    MenuFlyoutSubItem CreateMenuSubItem(
        Styles const& styles,
        hstring const& glyph,
        hstring const& text)
    {
        MenuFlyoutSubItem item;
        item.Style(styles.SubItem);
        item.Icon(CreateFontIcon(glyph));
        item.Text(text);
        return item;
    }

    MenuFlyoutSubItem CreateMenuSubItem(
        Styles const& styles,
        hstring const& text)
    {
        MenuFlyoutSubItem item;
        item.Style(styles.SubItem);
        item.Text(text);
        return item;
    }

    void ShowAt(
        winrt::Microsoft::UI::Xaml::Controls::MenuFlyout const& flyout,
        winrt::Microsoft::UI::Xaml::Controls::ListView const& listView,
        winrt::Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const& e)
    {
        flyout.ShowAt(listView, e.GetPosition(listView));
    }
    FontIcon CreateFontIcon(hstring glyph) {
        FontIcon fontIcon;
        fontIcon.Glyph(glyph);
        fontIcon.FontFamily(FontFamily(L"Segoe Fluent Icons"));
        fontIcon.FontSize(16);

        return fontIcon;
    }

    InfoBar CreateInfoBar(hstring title, hstring message, InfoBarSeverity severity, XamlRoot xamlRoot) {
        InfoBar infobar;

        infobar.Title(title);
        infobar.Message(message);
        infobar.Severity(severity);
        infobar.XamlRoot(xamlRoot);
        infobar.HorizontalAlignment(HorizontalAlignment::Right);
        infobar.VerticalAlignment(VerticalAlignment::Top);

        auto themeResources = Application::Current().Resources();
        auto color = unbox_value<Color>(themeResources.TryLookup(box_value(L"SystemChromeMediumColor")));

        SolidColorBrush bg;
        bg.Color(color);
        bg.Opacity(0.9);
        infobar.Background(bg);

        return infobar;
    }

    void DisplayInfoBar(InfoBar infobar, Panel parent, int time) {
        if (!infobar || !parent) return;

        // Entrance animation
        EdgeUIThemeTransition transition;
        TransitionCollection transitions;
        transitions.Append(transition);
        infobar.Transitions(transitions);

        // Add and display
        parent.Children().Append(infobar);
        infobar.IsOpen(true);

        // Auto close timer
        auto timer = DispatcherTimer();
        timer.Interval(std::chrono::milliseconds(time));
        timer.Tick([infobar, parent, timer](auto&&, auto&&) {
            // Run fade out animation first
            Storyboard storyboard;
            auto fadeOutAnimation = FadeOutThemeAnimation();
            Storyboard::SetTarget(fadeOutAnimation, infobar);
            storyboard.Children().Append(fadeOutAnimation.as<Timeline>());
            storyboard.Begin();

            // Then close and remove from parent
            auto timer2 = DispatcherTimer();
            timer2.Interval(std::chrono::milliseconds(300));
            timer2.Tick([infobar, parent, timer2](auto&&, auto&&) {
                infobar.IsOpen(false);
                uint32_t index;
                if (parent.Children().IndexOf(infobar, index)) {
                    parent.Children().RemoveAt(index);
                }
                timer2.Stop();
                });
            timer2.Start();

            timer.Stop();
            });
        timer.Start();
    }

    void CreateInfoBarAndDisplay(hstring title, hstring message, InfoBarSeverity severity, XamlRoot xamlRoot, Panel parent, int time) {
        DisplayInfoBar(CreateInfoBar(title, message, severity, xamlRoot), parent, time);
    }

    void CreateInfoBarAndDisplay(hstring title, hstring message, InfoBarSeverity severity, winrt::StarlightGUI::implementation::MainWindow* instance, int time) {
        DisplayInfoBar(CreateInfoBar(title, message, severity, instance->MainWindowGrid().XamlRoot()), instance->InfoBarPanel(), time);
    }

    void CreateInfoBarAndDisplay(hstring title, hstring message, InfoBarSeverity severity, winrt::StarlightGUI::implementation::InfoWindow* instance, int time) {
        DisplayInfoBar(CreateInfoBar(title, message, severity, instance->InfoWindowGrid().XamlRoot()), instance->InfoBarPanel(), time);
    }

    ContentDialog CreateContentDialog(hstring title, hstring content, hstring closeMessage, XamlRoot xamlRoot) {
        ContentDialog dialog;

        dialog.Title(winrt::box_value(title));
        dialog.Content(winrt::box_value(content));
        dialog.CloseButtonText(closeMessage);
        dialog.XamlRoot(xamlRoot);

        return dialog;
    }

    DataTemplate GetContentDialogSuccessTemplate() {
        return XamlReader::Load(LR"(
        <DataTemplate xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation">
            <StackPanel Orientation="Horizontal" Spacing="8">
                <FontIcon Glyph="&#xec61;" FontSize="30" FontFamily="Segoe Fluent Icons" Foreground="Green" Margin="0,5,0,0"/>
                <TextBlock Text="{Binding}" VerticalAlignment="Center"/>
            </StackPanel>
        </DataTemplate>
    )").as<DataTemplate>();
    }

    DataTemplate GetContentDialogErrorTemplate() {
        return XamlReader::Load(LR"(
        <DataTemplate xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation">
            <StackPanel Orientation="Horizontal" Spacing="8">
                <FontIcon Glyph="&#xeb90;" FontSize="30" FontFamily="Segoe Fluent Icons" Foreground="OrangeRed" Margin="0,5,0,0"/>
                <TextBlock Text="{Binding}" VerticalAlignment="Center"/>
            </StackPanel>
        </DataTemplate>
    )").as<DataTemplate>();
    }

    DataTemplate GetContentDialogInfoTemplate() {
        return XamlReader::Load(LR"(
        <DataTemplate xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation">
            <StackPanel Orientation="Horizontal" Spacing="8">
                <FontIcon Glyph="&#xf167;" FontSize="30" FontFamily="Segoe Fluent Icons" Foreground="LightBlue" Margin="0,5,0,0"/>
                <TextBlock Text="{Binding}" VerticalAlignment="Center"/>
            </StackPanel>
        </DataTemplate>
    )").as<DataTemplate>();
    }

    DataTemplate GetTemplate(hstring xaml) {
        return XamlReader::Load(xaml).as<DataTemplate>();
    }

    bool CheckIllegalComboBoxAction(IInspectable const& sender, SelectionChangedEventArgs const& e) {
        auto cb = sender.as<ComboBox>();

        if (!cb) return true;

        int index = cb.SelectedIndex();
        int itemCount = cb.Items().Size();

        // 非法索引，返回true并重置索引
        if (index < 0 || index >= itemCount) {
            cb.SelectedIndex(0);
            return true; 
        }

        // 正常索引，返回false
        return false;
    }
}
