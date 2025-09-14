#pragma once

#ifndef SIMPLE_LOGGER
#define SIMPLE_LOGGER

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstdio>
#include <cstdlib>

#define SIMPLE_LOGGER_MAJOR 1
#define SIMPLE_LOGGER_MINOR 0
#define SIMPLE_LOGGER_PATCH 0

namespace tool
{
	namespace log
	{
		inline constexpr int BUF_SIZE = 256;

		enum class Level
		{
			DBG,
			ERR,
			SYS,
		};

		inline Level limit;
		inline bool is_valid;

		inline wchar_t filename[128];
		inline const wchar_t* level_str[] = { L"DEBUG",L"ERROR",L"SYSTEM" };

		inline wchar_t buff[BUF_SIZE];

		inline void init(Level level)
		{
			limit = level;

			DWORD log_attr = GetFileAttributesW(L"log");
			if (log_attr == INVALID_FILE_ATTRIBUTES || (log_attr & FILE_ATTRIBUTE_DIRECTORY) != 0)
			{
				CreateDirectoryW(L"log", nullptr);
			}

			SYSTEMTIME st;
			GetLocalTime(&st);

			swprintf_s(filename, L"log/LOG_%d%02d%02d_%02d%02d%02d.txt",
				st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

			HANDLE hfile = CreateFileW(
				filename,
				GENERIC_WRITE,
				0,
				nullptr,
				CREATE_ALWAYS,
				FILE_ATTRIBUTE_NORMAL,
				nullptr
			);

			if (hfile == INVALID_HANDLE_VALUE)
			{
				is_valid = false;
				return;
			}

			is_valid = true;

			WORD bom = 0xFEFF;
			DWORD bytes_written;
			WriteFile(hfile, &bom, sizeof(bom), &bytes_written, nullptr);

			swprintf_s(buff, L"LOGGER  | LOG LEVEL %-7s\r\n", level_str[static_cast<int>(level)]);

			WriteFile(
				hfile,
				buff,
				static_cast<DWORD>(wcslen(buff) * sizeof(wchar_t)),
				&bytes_written,
				nullptr);

			CloseHandle(hfile);
		}

		inline void write(Level level, const wchar_t* category)
		{
			HANDLE hfile = CreateFileW(
				filename,
				FILE_APPEND_DATA,
				FILE_SHARE_READ,
				nullptr,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				nullptr
			);

			if (hfile == INVALID_HANDLE_VALUE)
			{
				is_valid = false;
				return;
			}

			wchar_t line[BUF_SIZE];
			swprintf_s(
				line,
				L"%-7s | %-17s | %s\r\n",
				level_str[static_cast<int>(level)],
				category,
				buff);

			DWORD bytes_written;
			WriteFile(
				hfile,
				line,
				static_cast<DWORD>(wcslen(line) * sizeof(wchar_t)),
				&bytes_written,
				nullptr);

			CloseHandle(hfile);
		}
	}
}

#define LOG_INIT_DBG tool::log::init(tool::log::Level::DBG)
#define LOG_INIT_ERR tool::log::init(tool::log::Level::ERR)
#define LOG_INIT_SYS tool::log::init(tool::log::Level::SYS)

#define LOG(level, category, fmt, ...)							\
do {															\
	if (tool::log::limit <= (level) && tool::log::is_valid) {	\
		_snwprintf_s(tool::log::buff,							\
			_countof(tool::log::buff), _TRUNCATE,				\
			(fmt), __VA_ARGS__);								\
		tool::log::write((level), (category));					\
	}															\
} while (false)

#define LOG_DBG(category, fmt, ...)						\
LOG(tool::log::Level::DBG, category, fmt, __VA_ARGS__)

#define LOG_ERR(category, fmt, ...)						\
LOG(tool::log::Level::ERR, category, fmt, __VA_ARGS__)

#define LOG_SYS(category, fmt, ...)						\
LOG(tool::log::Level::SYS, category, fmt, __VA_ARGS__)

#endif // !SIMPLE_LOGGER