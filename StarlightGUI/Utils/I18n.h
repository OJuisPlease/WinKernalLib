#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <windows.h>
#include <winrt/base.h>

namespace slg {
    namespace detail {
        // Minimal flat-JSON parser: {"key": "value", ...}
        // Handles escaped quotes (\") and backslashes (\\) in values.
        inline std::unordered_map<std::wstring, std::wstring> ParseJsonFile(const std::wstring& path) {
            std::unordered_map<std::wstring, std::wstring> map;

            // Read as raw UTF-8 bytes
            std::ifstream f(path, std::ios::binary);
            if (!f.is_open()) return map;
            std::string utf8((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            f.close();
            if (utf8.empty()) return map;

            // Skip BOM if present
            if (utf8.size() >= 3 && utf8[0] == '\xEF' && utf8[1] == '\xBB' && utf8[2] == '\xBF')
                utf8 = utf8.substr(3);

            // Convert UTF-8 → wstring
            int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), nullptr, 0);
            if (wlen <= 0) return map;
            std::wstring json(wlen, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), json.data(), wlen);

            size_t i = 0;
            auto skipWs = [&]() { while (i < json.size() && (json[i] == L' ' || json[i] == L'\t' || json[i] == L'\n' || json[i] == L'\r')) ++i; };
            auto parseStr = [&]() -> std::wstring {
                if (i >= json.size() || json[i] != L'"') return {};
                ++i; // skip opening "
                std::wstring result;
                while (i < json.size() && json[i] != L'"') {
                    if (json[i] == L'\\' && i + 1 < json.size()) {
                        ++i;
                        switch (json[i]) {
                            case L'"': result += L'"'; break;
                            case L'\\': result += L'\\'; break;
                            case L'n': result += L'\n'; break;
                            case L't': result += L'\t'; break;
                            default: result += L'\\'; result += json[i]; break;
                        }
                    } else {
                        result += json[i];
                    }
                    ++i;
                }
                if (i < json.size()) ++i; // skip closing "
                return result;
            };

            skipWs();
            if (i < json.size() && json[i] == L'{') ++i; // skip {
            while (i < json.size()) {
                skipWs();
                if (json[i] == L'}') break;
                auto key = parseStr();
                skipWs();
                if (i < json.size() && json[i] == L':') ++i; // skip :
                skipWs();
                auto val = parseStr();
                if (!key.empty()) map[key] = val;
                skipWs();
                if (i < json.size() && json[i] == L',') ++i; // skip ,
            }
            return map;
        }

        inline const std::unordered_map<std::wstring, std::wstring>& GetStrings() {
            static std::unordered_map<std::wstring, std::wstring> s_map = [] {
                // Find exe directory
                wchar_t exe[MAX_PATH]{};
                GetModuleFileNameW(nullptr, exe, MAX_PATH);
                std::wstring dir(exe);
                dir = dir.substr(0, dir.rfind(L'\\') + 1);

                // Read language from config.
                // "system" or empty → detect from OS preferred UI language.
                extern std::string language;
                std::wstring lang;
                if (language.empty() || language == "system") {
                    // Detect system UI language
                    ULONG numLangs = 0;
                    ULONG bufSize = 0;
                    if (GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &numLangs, nullptr, &bufSize) && bufSize > 0) {
                        std::wstring buf(bufSize, L'\0');
                        if (GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &numLangs, buf.data(), &bufSize)) {
                            lang = buf.c_str(); // first language (null-terminated in multi-string)
                        }
                    }
                    // Map to supported languages: zh-* → zh-CN, everything else → en-US
                    if (lang.size() >= 2 && (lang[0] == L'z' || lang[0] == L'Z') && (lang[1] == L'h' || lang[1] == L'H'))
                        lang = L"zh-CN";
                    else
                        lang = L"en-US";
                } else {
                    lang.assign(language.begin(), language.end());
                }

                auto map = ParseJsonFile(dir + L"Strings\\" + lang + L"\\Resources.json");
                if (map.empty()) {
                    // Fallback to zh-CN
                    map = ParseJsonFile(dir + L"Strings\\zh-CN\\Resources.json");
                }
                return map;
            }();
            return s_map;
        }
    }

    // Returns a localized string from Resources.json by key name.
    // Falls back to the key itself if not found.
    inline winrt::hstring GetLocalizedString(const wchar_t* key) noexcept {
        try {
            auto& map = detail::GetStrings();
            auto it = map.find(key);
            if (it != map.end()) {
                return winrt::hstring{ it->second };
            }
        }
        catch (...) { }
        return winrt::hstring{ key };
    }

    // t() — shorthand for GetLocalizedString().
    inline winrt::hstring t(const wchar_t* key) noexcept {
        return GetLocalizedString(key);
    }

    inline winrt::hstring t(std::wstring_view key) noexcept {
        return GetLocalizedString(std::wstring(key).c_str());
    }

    inline winrt::hstring t(const char* key) noexcept {
        const auto len = std::strlen(key);
        std::wstring wkey(len, L'\0');
        for (size_t i = 0; i < len; ++i) wkey[i] = static_cast<wchar_t>(key[i]);
        return GetLocalizedString(wkey.c_str());
    }

    inline winrt::hstring t(std::string_view key) noexcept {
        std::wstring wkey(key.size(), L'\0');
        for (size_t i = 0; i < key.size(); ++i) wkey[i] = static_cast<wchar_t>(key[i]);
        return GetLocalizedString(wkey.c_str());
    }
}
