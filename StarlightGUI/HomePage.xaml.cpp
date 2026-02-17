#include "pch.h"
#include "HomePage.xaml.h"
#if __has_include("HomePage.g.cpp")
#include "HomePage.g.cpp"
#endif

#include <winrt/Windows.System.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Web.Http.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.System.UserProfile.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Microsoft.UI.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <random>
#include <chrono>
#include <iomanip>
#include <iphlpapi.h>
#include "MainWindow.xaml.h"

using namespace winrt;
using namespace Windows::Web::Http;
using namespace Windows::Data::Json;
using namespace Windows::System;
using namespace Windows::Storage;
using namespace Windows::Foundation;
using namespace Windows::Globalization;
using namespace Windows::ApplicationModel;
using namespace Microsoft::UI::Dispatching;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media::Imaging;

namespace winrt::StarlightGUI::implementation
{
    static double graphX = 1;

    HomePage::HomePage()
    {
        InitializeComponent();

        TotalLineGraph().AddSeries(L"CPU", Colors::LightSkyBlue());
        TotalLineGraph().AddSeries(L"内存", Colors::DodgerBlue());
        TotalLineGraph().AddSeries(L"磁盘", Colors::LimeGreen());
        TotalLineGraph().AddSeries(L"GPU", Colors::MediumPurple());

        this->Loaded([this](auto&&, auto&&) -> IAsyncAction {
            SetGreetingText();
            SetUserProfile();
            FetchHitokoto();
            SetupClock();
            infoInitialized = true;

            co_return;
            });

        this->Unloaded([this](auto&&, auto&&) {
            clockTimer.Stop();
            });

        LOG_INFO(L"HomePage", L"HomePage initialized.");
    }

    slg::coroutine HomePage::SetGreetingText()
    {
        if (greeting.empty()) {
            std::vector<hstring> greetings = {
                L"欢迎回来",
                L"你好",
                L"Hi",
                L"TimeFormat",
            };
            greeting = greetings[GenerateRandomNumber(0, greetings.size() - 1)];

            DateTime currentDateTime;
            Calendar calendar;
            calendar.SetToNow();
            int currentHour = calendar.Hour();

            if (greeting == L"TimeFormat") {
                if (currentHour >= 4 && currentHour < 12)
                {
                    greeting = L"上午好";
                }
                else if (currentHour < 18)
                {
                    greeting = L"下午好";
                }
                else if (currentHour < 4 || currentHour >= 18)
                {
                    greeting = L"晚上好";
                }
            }
        }

        AppIntroduction().Text(L"欢迎使用 Starlight GUI！");
        co_return;
    }

    slg::coroutine HomePage::SetUserProfile()
    {
        auto weak_this = get_weak();

        try {
            if (!infoInitialized) {
                co_await winrt::resume_background();

                LOG_INFO(__WFUNCTION__, L"Retrieving user profile...");

                auto& users = co_await User::FindAllAsync(UserType::LocalUser, UserAuthenticationStatus::LocallyAuthenticated);

                if (users && users.Size() > 0)
                {
                    LOG_INFO(__WFUNCTION__, L"Retrieved user list.");
                    auto user = users.GetAt(0);
                    auto& picture = co_await user.GetPictureAsync(UserPictureSize::Size64x64);

                    if (picture)
                    {
                        LOG_INFO(__WFUNCTION__, L"Retrieved user picture.");
                        auto& stream = co_await picture.OpenReadAsync();

                        if (stream)
                        {
                            co_await wil::resume_foreground(DispatcherQueue());

                            BitmapImage bitmapImage;
                            co_await bitmapImage.SetSourceAsync(stream);
                            avatar = bitmapImage;

                            LOG_INFO(__WFUNCTION__, L"Retrieved user account picture successfully.");
                        }
                    }

                    co_await winrt::resume_background();

                    auto& displayName = co_await user.GetPropertyAsync(KnownUserProperties::DisplayName());

                    if (displayName && !displayName.as<winrt::hstring>().empty())
                    {
                        username = displayName.as<winrt::hstring>();
                        LOG_INFO(__WFUNCTION__, L"Retrieved user account name successfully.");
                    }
                }
            }

            if (auto strong_this = weak_this.get()) {
                co_await wil::resume_foreground(DispatcherQueue());
                UserAvatar().ImageSource(avatar.as<winrt::Microsoft::UI::Xaml::Media::ImageSource>());
                WelcomeText().Text(greeting + L", " + username + L"！");
            }
        }
        catch (const hresult_error& e) {
            LOG_ERROR(__WFUNCTION__, L"Failed to retrieve user profile! winrt::hresult_error: %s (%d)", e.message().c_str(), e.code().value);
            WelcomeText().Text(greeting + L"！");
        }
    }

    slg::coroutine HomePage::FetchHitokoto()
    {
        auto weak_this = get_weak();
        try {
            if (!infoInitialized) {
                co_await winrt::resume_background();

                LOG_INFO(__WFUNCTION__, L"Sending hitokoto request...");

                // 异步获取随机词条
                HttpClient client;
                /*
                * 移除：
                * a	动画
                * b	漫画
                * c	游戏
                * 因为太唐了受不了了 为什么说的话都那么逆天
                */
                Uri uri(L"https://v1.hitokoto.cn/?c=d&c=e&c=i&c=j&c=k");
                auto& result = co_await client.GetStringAsync(uri);

                // 读取 json 内容
                auto json = Windows::Data::Json::JsonObject::Parse(result);
                hitokoto = L"“" + json.GetNamedString(L"hitokoto") + L"”";

                LOG_INFO(__WFUNCTION__, L"Hitokoto json result: %s", result.c_str());
            }

            if (auto strong_this = weak_this.get()) {
                co_await wil::resume_foreground(DispatcherQueue());
                HitokotoText().Text(hitokoto);
            }
        }
        catch (const hresult_error& e) {
            LOG_ERROR(__WFUNCTION__, L"Failed to fetch hitokoto! winrt::hresult_error: %s (%d)", e.message().c_str(), e.code().value);
            HitokotoText().Text(L"获取失败... :(");
        }
    }

    slg::coroutine HomePage::SetupClock()
    {
        UpdateClock();
        UpdateGauges();

        // 每秒更新一次
        clockTimer.Interval(std::chrono::seconds(1));
        clockTimer.Tick({ this, &HomePage::OnClockTick });
        clockTimer.Start();

        co_return;
    }

    slg::coroutine HomePage::OnClockTick(IInspectable const&, IInspectable const&)
    {
        if (!IsLoaded()) co_return;
        auto weak_this = get_weak();

        try {
            if (auto strong_this = weak_this.get()) {
                UpdateClock();
                UpdateGauges();
            }
            else {
                clockTimer.Stop();
            }
        }
        catch (const hresult_error& e) {
            LOG_ERROR(__WFUNCTION__, L"Error while clock ticking! winrt::hresult_error: %s (%d)", e.message().c_str(), e.code().value);
            HitokotoText().Text(L"获取失败... :(");
        }
    }

    /*
    * ! 至尊答辩代码 !
    * @Author Stars
    */
    slg::coroutine HomePage::UpdateGauges() {
        if (!IsLoaded()) co_return;
        auto weak_this = get_weak();

        co_await winrt::resume_background();

        // 初始化性能计数器
        if (!initialized) {
            PdhOpenQueryW(NULL, 0, &query);
            PdhAddCounterW(query, L"\\Processor(_Total)\\% Processor Time", 0, &counter_cpu_time);
            PdhAddCounterW(query, L"\\Processor Information(_Total)\\Actual Frequency", 0, &counter_cpu_freq);
            PdhAddCounterW(query, L"\\Processor Information(_Total)\\Actual Frequency", 0, &counter_cpu_freq);
            PdhAddCounterW(query, L"\\System\\Processes", 0, &counter_cpu_process);
            PdhAddCounterW(query, L"\\System\\Threads", 0, &counter_cpu_thread);
            PdhAddCounterW(query, L"\\System\\System Calls/sec", 0, &counter_cpu_syscall);
            PdhAddCounterW(query, L"\\Memory\\Cache Bytes", 0, &counter_mem_cached);
            PdhAddCounterW(query, L"\\Memory\\Committed Bytes", 0, &counter_mem_committed);
            PdhAddCounterW(query, L"\\Memory\\Page Reads/sec", 0, &counter_mem_read);
            PdhAddCounterW(query, L"\\Memory\\Page Writes/sec", 0, &counter_mem_write);
            PdhAddCounterW(query, L"\\Memory\\Pages Input/sec", 0, &counter_mem_input);
            PdhAddCounterW(query, L"\\Memory\\Pages Output/sec", 0, &counter_mem_output);
            PdhAddCounterW(query, L"\\LogicalDisk(_Total)\\% Disk Time", 0, &counter_disk_time);
            PdhAddCounterW(query, L"\\LogicalDisk(_Total)\\Disk Transfers/sec", 0, &counter_disk_trans);
            PdhAddCounterW(query, L"\\LogicalDisk(_Total)\\Disk Read Bytes/sec", 0, &counter_disk_read);
            PdhAddCounterW(query, L"\\LogicalDisk(_Total)\\Disk Write Bytes/sec", 0, &counter_disk_write);
            PdhAddCounterW(query, L"\\LogicalDisk(_Total)\\Split IO/Sec", 0, &counter_disk_io);
            PdhAddCounterW(query, L"\\GPU Engine(*)\\Utilization Percentage", 0, &counter_gpu_time);
            PdhAddCounterW(query, L"\\Network Interface(*)\\Bytes Received/sec", 0, &counter_net_receive);
            PdhAddCounterW(query, L"\\Network Interface(*)\\Bytes Sent/sec", 0, &counter_net_send);
            PdhAddCounterW(query, L"\\Network Interface(*)\\Packets Received/sec", 0, &counter_net_packet_receive);
            PdhAddCounterW(query, L"\\Network Interface(*)\\Packets Sent/sec", 0, &counter_net_packet_send);

            LOG_INFO(L"MonitorInstance", L"Initialized PDH counters.");

            // 获取虚拟化
            int cpuInfo[4] = { 0 };
            __cpuid(cpuInfo, 1);
            bool intelVT = (cpuInfo[2] & (1 << 5)) != 0; // VMX
            __cpuid(cpuInfo, 0x80000001);
            bool amdV = (cpuInfo[2] & (1 << 2)) != 0; // SVM
            __cpuid(cpuInfo, 0x80000002);
            virtualization = intelVT || amdV;

            // 获取 CPU 型号
            char cpu_name[49] = { 0 };
            memcpy(cpu_name, cpuInfo, sizeof(cpuInfo));
            __cpuid(cpuInfo, 0x80000003);
            memcpy(cpu_name + 16, cpuInfo, sizeof(cpuInfo));
            __cpuid(cpuInfo, 0x80000004);
            memcpy(cpu_name + 32, cpuInfo, sizeof(cpuInfo));
            cpu_manufacture = to_hstring(cpu_name);

            // 获取 L1/L2/L3 缓存
            DWORD bufferSize = 0;
            GetLogicalProcessorInformation(NULL, &bufferSize);

            std::vector<BYTE> buffer(bufferSize);
            PSYSTEM_LOGICAL_PROCESSOR_INFORMATION slpi = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION>(buffer.data());
            GetLogicalProcessorInformation(slpi, &bufferSize);
            for (size_t i = 0; i < bufferSize / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION); ++i) {
                if (slpi[i].Relationship == RelationCache) {
                    if (slpi[i].Cache.Level == 1) cache_l1 += slpi[i].Cache.Size / 1024.0;
                    else if (slpi[i].Cache.Level == 2) cache_l2 += slpi[i].Cache.Size / (1024.0 * 1024.0);
                    else if (slpi[i].Cache.Level == 3) cache_l3 += slpi[i].Cache.Size / (1024.0 * 1024.0);
                }
            }

            LOG_INFO(L"MonitorInstance", L"Initialized CPU information.");

            // 获取硬盘型号
            HANDLE hDevice = CreateFileW(L"\\\\.\\PhysicalDrive0", 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

            if (hDevice != INVALID_HANDLE_VALUE) {
                STORAGE_PROPERTY_QUERY spq{};
                spq.PropertyId = StorageDeviceProperty;
                spq.QueryType = PropertyStandardQuery;

                buffer = std::vector<BYTE>(1024);
                if (DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY, &spq, sizeof(spq), buffer.data(), buffer.size(), NULL, NULL))
                {
                    PSTORAGE_DEVICE_DESCRIPTOR sdd = reinterpret_cast<PSTORAGE_DEVICE_DESCRIPTOR>(buffer.data());
                    if (sdd->ProductIdOffset != 0) {
                        LPCSTR productId = reinterpret_cast<LPCSTR>(buffer.data() + sdd->ProductIdOffset);
                        disk_manufacture = to_hstring(productId);
                    }
                } else LOG_ERROR(L"MonitorInstance", L"Failed to complete I/O with PhysicalDrive0.");
            } else LOG_ERROR(L"MonitorInstance", L"Failed to open PhysicalDrive0.");

            // 获取 GPU 型号
            try {
                HMODULE hNvml = LoadLibraryW(L"nvml.dll");
                if (hNvml) {
                    FreeLibrary(hNvml);
                    if (nvmlInit_v2() == NVML_SUCCESS) {
                        UINT deviceCount = 0;
                        nvmlDeviceGetCount_v2(&deviceCount);
                        if (deviceCount > 0) {
                            isNvidia = true;
                            nvmlDeviceGetHandleByIndex_v2(0, &device);

                            char name[NVML_DEVICE_NAME_BUFFER_SIZE];
                            nvmlDeviceGetName(device, name, NVML_DEVICE_NAME_BUFFER_SIZE);
                            gpu_manufacture = StringToWideString(name);

                            LOG_INFO(L"MonitorInstance", L"Initialized NVML.");
                        }
                        else {
                            LOG_ERROR(L"MonitorInstance", L"NVML return device count as 0!");
                            nvmlShutdown();
                        }
                    }
                }
            }
            catch (...) {
                isNvidia = false;
                LOG_ERROR(L"MonitorInstance", L"Failed to initialize NVML. Probably unsupported firmware. Try fallback solution.");
            }
            
            // 非 NVIDIA/不支持的设备，使用备用方案
            if (!isNvidia) {
                LOG_ERROR(L"MonitorInstance", L"Failed to initialize NVML. Probably unsupported firmware. Try fallback solution.");
                isNvidia = false;
                DISPLAY_DEVICEW dd{};
                dd.cb = sizeof(dd);
                for (DWORD i = 0; EnumDisplayDevicesW(nullptr, i, &dd, 0); i++) {
                    if (dd.StateFlags & DISPLAY_DEVICE_ACTIVE) {
                        gpu_manufacture = dd.DeviceString;
                        break;
                    }
                }
            }

            // 获取网络适配器型号
            bufferSize = 0;
            if (GetAdaptersInfo(NULL, &bufferSize) == ERROR_BUFFER_OVERFLOW) {
                buffer = std::vector<BYTE>(bufferSize);
                PIP_ADAPTER_INFO iai = reinterpret_cast<PIP_ADAPTER_INFO>(buffer.data());
                if (GetAdaptersInfo(iai, &bufferSize) == ERROR_SUCCESS) {
                    int index = 0;
                    while (iai) {
                        adpt_name_map[index] = to_hstring(iai->Description);
                        iai = iai->Next;
                        index++;
                    }
                    netadpt_manufacture = adpt_name_map[0];
                    adptIndex++;
                }
                else LOG_ERROR(L"MonitorInstance", L"Failed to get adapter info.");
            }

            initialized = true;
        }

        PdhCollectQueryData(query);

        // CPU
        ULONG64 seconds = GetTickCount64() / 1000;
        ULONG64 days = seconds / (24 * 3600);
        seconds %= (24 * 3600);
        ULONG64 hours = seconds / 3600;
        seconds %= 3600;
        ULONG64 minutes = seconds / 60;
        seconds %= 60;

        wchar_t timebuffer[256];
        swprintf_s(timebuffer, L"%llu:%02llu:%02llu:%02llu", days, hours, minutes, seconds);

        // 内存
        MEMORYSTATUSEX memInfo{};
        memInfo.dwLength = sizeof(memInfo);
        if (!GlobalMemoryStatusEx(&memInfo)) LOG_ERROR(L"MonitorInstance", L"Failed to get memory status.");

        // GPU
        nvmlUtilization_t gpu_utilization{};
        nvmlMemory_t gpu_memory{};
        UINT gpu_temp, gpu_clock_graphics, gpu_clock_mem;
        if (isNvidia) {
            nvmlDeviceGetUtilizationRates(device, &gpu_utilization);
            nvmlDeviceGetMemoryInfo(device, &gpu_memory);
            nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &gpu_temp);
            nvmlDeviceGetClockInfo(device, NVML_CLOCK_GRAPHICS, &gpu_clock_graphics);
            nvmlDeviceGetClockInfo(device, NVML_CLOCK_MEM, &gpu_clock_mem);
        }

        if (auto strong_this = weak_this.get()) {
            co_await wil::resume_foreground(DispatcherQueue());

            graphX += 1;

            std::wstringstream ss;
            CpuGauge().Value(GetValueFromCounter(counter_cpu_time));
            ss << std::fixed << std::setprecision(1) << GetValueFromCounter(counter_cpu_time) << "%";
            CpuPercent().Text(ss.str());
            CpuManufacture().Text(to_hstring(cpu_manufacture));
            ss = std::wstringstream{};
            ss << std::fixed << std::setprecision(2) << GetValueFromCounter(counter_cpu_freq) / 1024.0 << " GHz";
            CpuFrequency().Text(ss.str());
            CpuProcess().Text(to_hstring(GetValueFromCounter(counter_cpu_process)));
            CpuThread().Text(to_hstring(GetValueFromCounter(counter_cpu_thread)));
            ss = std::wstringstream{};
            ss << std::fixed << std::setprecision(1) << GetValueFromCounter(counter_cpu_syscall) << "/s";
            CpuSyscall().Text(ss.str());
            CpuRunTime().Text(timebuffer);
            CpuCore().Text(to_hstring(std::thread::hardware_concurrency()));
            CpuVirtualization().Text(virtualization ? L"支持" : L"不支持");
            CpuCacheL1().Text(to_hstring(cache_l1) + L" KB");
            CpuCacheL2().Text(to_hstring(cache_l2) + L" MB");
            CpuCacheL3().Text(to_hstring(cache_l3) + L" MB");
            TotalLineGraph().AddDataPoint(L"CPU", graphX, GetValueFromCounter(counter_cpu_time));

            MemGauge().Value(memInfo.dwMemoryLoad);
            MemPercent().Text(to_hstring((int)memInfo.dwMemoryLoad) + L"%");
            MemSize().Text(FormatMemorySize(memInfo.ullTotalPhys));
            MemUsing().Text(FormatMemorySize(memInfo.ullTotalPhys - memInfo.ullAvailPhys));
            MemUsable().Text(FormatMemorySize(memInfo.ullAvailPhys));
            MemCached().Text(FormatMemorySize(GetValueFromCounter(counter_mem_cached)));
            MemCommitted().Text(FormatMemorySize(GetValueFromCounter(counter_mem_committed)));
            MemPageRead().Text(FormatMemorySize(GetValueFromCounter(counter_mem_read)) + L"/s");
            MemPageWrite().Text(FormatMemorySize(GetValueFromCounter(counter_mem_write)) + L"/s");
            MemPageInput().Text(FormatMemorySize(GetValueFromCounter(counter_mem_input)) + L"/s");
            MemPageOutput().Text(FormatMemorySize(GetValueFromCounter(counter_mem_output)) + L"/s");
            TotalLineGraph().AddDataPoint(L"内存", graphX, memInfo.dwMemoryLoad);

            DiskGauge().Value(GetValueFromCounter(counter_disk_time));
            DiskManufacture().Text(disk_manufacture);
            ss = std::wstringstream{};
            ss << std::fixed << std::setprecision(1) << GetValueFromCounter(counter_disk_time) << "%";
            DiskPercent().Text(ss.str());
            DiskRead().Text(FormatMemorySize(GetValueFromCounter(counter_disk_read)) + L"/s");
            DiskWrite().Text(FormatMemorySize(GetValueFromCounter(counter_disk_write)) + L"/s");
            ss = std::wstringstream{};
            ss << std::fixed << std::setprecision(1) << GetValueFromCounter(counter_disk_trans) << "/s";
            DiskTrans().Text(ss.str());
            ss = std::wstringstream{};
            ss << std::fixed << std::setprecision(1) << GetValueFromCounter(counter_disk_io) << "/s";
            DiskIO().Text(ss.str());
            TotalLineGraph().AddDataPoint(L"磁盘", graphX, GetValueFromCounter(counter_disk_time));

            double gpu_time = 0.0;
            if (isNvidia) {
                if (pdh_first) {
                    gpu_time = GetValueFromCounterArray(counter_gpu_time);
                }
                else {
                    gpu_time = gpu_utilization.gpu;
                }
                ss = std::wstringstream{};
                ss << std::fixed << std::setprecision(1) << FormatMemorySize(gpu_memory.used) << "/" << FormatMemorySize(gpu_memory.total);
                GpuMem().Text(ss.str());
                GpuTemp().Text(to_hstring(gpu_temp) + L" ℃");
                ss = std::wstringstream{};
                ss << std::fixed << std::setprecision(2) << gpu_clock_graphics / 1024.0 << " GHz";
                GpuClockGraphics().Text(ss.str());
                ss = std::wstringstream{};
                ss << std::fixed << std::setprecision(2) << gpu_clock_mem / 1024.0 << " GHz";
                GpuClockMem().Text(ss.str());
            }
            else {
                gpu_time = GetValueFromCounterArray(counter_gpu_time);
                GpuMem().Text(L"NaN");
                GpuTemp().Text(L"NaN");
                GpuClockGraphics().Text(L"NaN");
                GpuClockMem().Text(L"NaN");
            }
            GpuGauge().Value(gpu_time);
            ss = std::wstringstream{};
            ss << std::fixed << std::setprecision(1) << gpu_time << "%";
            GpuPercent().Text(ss.str());
            GpuManufacture().Text(gpu_manufacture);
            TotalLineGraph().AddDataPoint(L"GPU", graphX, gpu_time);

            if (isNetSend) {
                NetGauge().Value(GetValueFromCounterArray(counter_net_send) / (1024 * 1024));
                NetGauge().ValueStringFormat(L"↑ {0} MB/s");
            }
            else {
                NetGauge().Value(GetValueFromCounterArray(counter_net_receive) / (1024 * 1024));
                NetGauge().ValueStringFormat(L"↓ {0} MB/s");
            }
            NetManufacture().Text(netadpt_manufacture);
            NetReceive().Text(FormatMemorySize(GetValueFromCounterArray(counter_net_receive)) + L"/s");
            NetSend().Text(FormatMemorySize(GetValueFromCounterArray(counter_net_send)) + L"/s");
            ss = std::wstringstream{};
            ss << std::fixed << std::setprecision(1) << GetValueFromCounterArray(counter_net_packet_receive) << "/s";
            NetPacketReceive().Text(ss.str());
            ss = std::wstringstream{};
            ss << std::fixed << std::setprecision(1) << GetValueFromCounterArray(counter_net_packet_send) << "/s";
            NetPacketSend().Text(ss.str());
        }
    }

    slg::coroutine HomePage::UpdateClock()
    {
        if (!IsLoaded()) co_return;

        Calendar calendar;
        calendar.SetToNow();

        int hour = calendar.Hour();
        int minute = calendar.Minute();
        int second = calendar.Second();

        auto splitDigits = [](int value) -> std::pair<hstring, hstring> {
            int digit1 = value / 10;  // 十位
            int digit2 = value % 10;  // 个位
            return { to_hstring(digit1), to_hstring(digit2) };
            };

        auto hourDigits = splitDigits(hour);
        Hour1().Text(hourDigits.first);
        Hour2().Text(hourDigits.second);

        auto minuteDigits = splitDigits(minute);
        Minute1().Text(minuteDigits.first);
        Minute2().Text(minuteDigits.second);

        auto secondDigits = splitDigits(second);
        Second1().Text(secondDigits.first);
        Second2().Text(secondDigits.second);

        co_return;
    }

    void HomePage::NextAdapterName_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        if (adptIndex >= adpt_name_map.size()) {
            adptIndex = 0;
            netadpt_manufacture = to_hstring(adpt_name_map[0]);
        }
        else {
            netadpt_manufacture = to_hstring(adpt_name_map[adptIndex]);
        }
        NetManufacture().Text(netadpt_manufacture);
        adptIndex++;
    }

    void HomePage::ChangeMode_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
    {
        isNetSend = !isNetSend;
        UpdateGauges();
    }

}