#pragma once

#include "pch.h"
#include <pdh.h>

namespace winrt::StarlightGUI::implementation {
    std::wstring GenerateRandomString(size_t length);

    int GenerateRandomNumber(size_t from, size_t to);

    int GetDateAsInt();

    std::wstring FixBackSplash(const hstring& hstr);

    std::wstring RemoveFromString(const hstring& hstr, const hstring& removeStr);

    std::wstring GetParentDirectory(const hstring& path);

    std::string WideStringToString(const std::wstring& str);

    std::wstring StringToWideString(const std::string& str);

    std::wstring ULongToHexString(ULONG64 value);

    std::wstring ULongToHexString(ULONG64 value, int w, bool uppercase, bool prefix);

    // 需要判断是否成功，因此返回 bool
    bool HexStringToULong(const std::wstring& input, ULONG64& out);

    bool StringToNumber(const std::wstring& input, LONG64& out);

    bool StringToNumber(const std::wstring& input, ULONG64& out);

    std::wstring FormatMemorySize(double bytes);

    std::wstring ExtractFunctionName(const std::string& old);

    std::wstring ExtractFileName(const std::wstring& path);

    std::wstring GetInstalledLocationPath();

    std::wstring GetStacktrace(UINT length);

    double GetValueFromCounter(PDH_HCOUNTER& counter);

    double GetValueFromCounterArray(PDH_HCOUNTER& counter);
}