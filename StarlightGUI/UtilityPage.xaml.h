#pragma once

#include "UtilityPage.g.h"

namespace winrt::StarlightGUI::implementation
{
    struct UtilityPage : UtilityPageT<UtilityPage>
    {
        UtilityPage();

        slg::coroutine Button_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
        slg::coroutine Button_Click2(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);
    };
}

namespace winrt::StarlightGUI::factory_implementation
{
    struct UtilityPage : UtilityPageT<UtilityPage, implementation::UtilityPage>
    {
    };
}
