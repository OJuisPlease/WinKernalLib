#pragma once

#include "LogEntryInfo.g.h"

namespace winrt::StarlightGUI::implementation
{
	struct LogEntryInfo : LogEntryInfoT<LogEntryInfo>
	{
		LogEntryInfo() = default;

		[[nodiscard]] hstring Timestamp() const noexcept { return m_timestamp; }
		void Timestamp(hstring const& value) { m_timestamp = value; }

		[[nodiscard]] hstring Level() const noexcept { return m_level; }
		void Level(hstring const& value) { m_level = value; }

		[[nodiscard]] hstring Source() const noexcept { return m_source; }
		void Source(hstring const& value) { m_source = value; }

		[[nodiscard]] hstring Message() const noexcept { return m_message; }
		void Message(hstring const& value) { m_message = value; }

		[[nodiscard]] int32_t LevelInt() const noexcept { return m_levelInt; }
		void LevelInt(int32_t value) { m_levelInt = value; }

	private:
		hstring m_timestamp{ L"" };
		hstring m_level{ L"" };
		hstring m_source{ L"" };
		hstring m_message{ L"" };
		int32_t m_levelInt{ 0 };
	};
}

namespace winrt::StarlightGUI::factory_implementation
{
	struct LogEntryInfo : LogEntryInfoT<LogEntryInfo, implementation::LogEntryInfo>
	{
	};
}
