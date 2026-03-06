#include "pch.h"
#include "UtilityPage.xaml.h"
#if __has_include("UtilityPage.g.cpp")
#include "UtilityPage.g.cpp"
#endif

#include "MainWindow.xaml.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::StarlightGUI::implementation{
	static hstring safeAcceptedTag = L"";

	UtilityPage::UtilityPage() {
		InitializeComponent();

		if (hypervisor_mode) {
			ObCallbackCard().Header(box_value(L"注册对象操作回调 (Hypervisor mode)"));
			DSECard().Header(box_value(L"驱动签名强制 (Hypervisor mode)"));
		}

		LOG_INFO(L"UtilityPage", L"UtilityPage initialized.");
	}

	slg::coroutine UtilityPage::Button_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
	{
		auto button = sender.as<Button>();
		std::wstring tag = button.Tag().as<winrt::hstring>().c_str();

		if (safeAcceptedTag != tag) {
			safeAcceptedTag = tag;
			slg::CreateInfoBarAndDisplay(L"警告", L"该操作可能导致系统不稳定或崩溃！如果你知道自己在做什么，请再次点击！", InfoBarSeverity::Warning, g_mainWindowInstance);
			slg::CreateInfoBarAndDisplay(L"警告", L"如果多次进行了同一个禁用操作，也需要多次进行启用才可恢复！", InfoBarSeverity::Warning, g_mainWindowInstance);
			co_return;
		}

		ULONG color = BSODColorComboBox().SelectedIndex() - 1;
		ULONG type = PGTypeComboBox().SelectedIndex();

		co_await winrt::resume_background();

		LOG_INFO(L"UtilityPage", L"Confirmed we will do: %s", tag.c_str());

		BOOL result = FALSE;

		if (tag == L"ENABLE_HVM") {
			result = KernelInstance::EnableHVM();
			hypervisor_mode = result;
		}
		else if (tag == L"ENABLE_CREATE_PROCESS") {
			result = KernelInstance::EnableCreateProcess();
		}
		else if (tag == L"DISABLE_CREATE_PROCESS") {
			result = KernelInstance::DisableCreateProcess();
		}
		else if (tag == L"ENABLE_CREATE_FILE") {
			result = KernelInstance::EnableCreateFile();
		}
		else if (tag == L"DISABLE_CREATE_FILE") {
			result = KernelInstance::DisableCreateFile();
		}
		else if (tag == L"ENABLE_LOAD_DRV") {
			result = KernelInstance::EnableLoadDriver();
		}
		else if (tag == L"DISABLE_LOAD_DRV") {
			result = KernelInstance::DisableLoadDriver();
		}
		else if (tag == L"ENABLE_UNLOAD_DRV") {
			result = KernelInstance::EnableUnloadDriver();
		}
		else if (tag == L"DISABLE_UNLOAD_DRV") {
			result = KernelInstance::DisableUnloadDriver();
		}
		else if (tag == L"ENABLE_MODIFY_REG") {
			result = KernelInstance::EnableModifyRegistry();
		}
		else if (tag == L"DISABLE_MODIFY_REG") {
			result = KernelInstance::DisableModifyRegistry();
		}
		else if (tag == L"ENABLE_MODIFY_BOOTSEC") {
			result = KernelInstance::ProtectDisk();
		}
		else if (tag == L"DISABLE_MODIFY_BOOTSEC") {
			result = KernelInstance::UnprotectDisk();
		}
		else if (tag == L"ENABLE_OBJ_REG_CB") {
			result = KernelInstance::EnableObCallback();
		}
		else if (tag == L"DISABLE_OBJ_REG_CB") {
			result = KernelInstance::DisableObCallback();
		}
		else if (tag == L"ENABLE_CM_REG_CB") {
			result = KernelInstance::EnableCmpCallback();
		}
		else if (tag == L"DISABLE_CM_REG_CB") {
			result = KernelInstance::DisableCmpCallback();
		}
		else if (tag == L"ENABLE_DSE") {
			result = KernelInstance::EnableDSE();
		}
		else if (tag == L"DISABLE_DSE") {
			result = KernelInstance::DisableDSE();
		}
		else if (tag == L"ENABLE_LKD") {
			result = KernelInstance::EnableLKD();
		}
		else if (tag == L"DISABLE_LKD") {
			result = KernelInstance::DisableLKD();
		}
		else if (tag == L"BSOD") {
			result = KernelInstance::BlueScreen(color);
		}
		else if (tag == L"PatchGuard") {
			result = KernelInstance::DisablePatchGuard(type);
		}
		else {
			co_await wil::resume_foreground(DispatcherQueue());
			slg::CreateInfoBarAndDisplay(L"错误", L"未知操作！", InfoBarSeverity::Error, g_mainWindowInstance);
			co_return;
		}

		co_await wil::resume_foreground(DispatcherQueue());

		if (result) {
			slg::CreateInfoBarAndDisplay(L"成功", L"成功完成操作！", InfoBarSeverity::Success, g_mainWindowInstance);
		}
		else {
			if (GetLastError() == 0) {
				slg::CreateInfoBarAndDisplay(L"失败", L"无法完成操作，该功能可能已经是预期的状态了！", InfoBarSeverity::Error, g_mainWindowInstance);
			}
			else {
				slg::CreateInfoBarAndDisplay(L"失败", L"无法完成操作，错误码: " + to_hstring((int)GetLastError()), InfoBarSeverity::Error, g_mainWindowInstance);
			}
		}

		if (hypervisor_mode) {
			ObCallbackCard().Header(box_value(L"注册对象操作回调 (Hypervisor mode)"));
			DSECard().Header(box_value(L"驱动签名强制 (Hypervisor mode)"));
		}

		co_return;
	}

	slg::coroutine UtilityPage::Button_Click2(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
	{
		auto button = sender.as<Button>();
		std::wstring tag = button.Tag().as<winrt::hstring>().c_str();

		co_await winrt::resume_background();

		LOG_INFO(L"UtilityPage", L"Confirmed we will do: %s", tag.c_str());

		BOOL result = FALSE;

		if (tag == L"POWER_SHUTDOWN") {
			result = KernelInstance::Shutdown();
		}
		else if (tag == L"POWER_REBOOT") {
			result = KernelInstance::Reboot();
		}
		else if (tag == L"POWER_REBOOT_FORCE") {
			result = KernelInstance::RebootForce();
		}
		else {
			slg::CreateInfoBarAndDisplay(L"错误", L"未知操作！", InfoBarSeverity::Error, g_mainWindowInstance);
			co_return;
		}

		co_await wil::resume_foreground(DispatcherQueue());

		if (result) {
			slg::CreateInfoBarAndDisplay(L"成功", L"成功完成操作！", InfoBarSeverity::Success, g_mainWindowInstance);
		}
		else {
			slg::CreateInfoBarAndDisplay(L"失败", L"无法完成操作，错误码: " + to_hstring((int)GetLastError()), InfoBarSeverity::Error, g_mainWindowInstance);
		}

		co_return;
	}
}
