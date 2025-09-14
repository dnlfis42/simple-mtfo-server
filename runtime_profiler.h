#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstdio>

#define RUNTIME_PROFILER_MAJOR 1
#define RUNTIME_PROFILER_MINOR 0
#define RUNTIME_PROFILER_PATCH 0

#define RUNTIME_PROFILER

namespace tool
{
	namespace profile
	{
		static constexpr long long TRIM_CNT = 2;
		static constexpr long long TOTAL_TRIM_CNT = TRIM_CNT * 2;

		struct Report
		{
			const wchar_t* tag = nullptr;

			LARGE_INTEGER start_time{};
			LARGE_INTEGER end_time{};

			long long total_elapsed = 0;

			long long max_elapsed[TRIM_CNT]{};
			long long min_elapsed[TRIM_CNT]{};

			long long call_cnt = 0;
		};

		class RuntimeProfiler
		{
		private:
			static constexpr int MAX_REPORT_CNT = 128;
			static constexpr int MAX_LINE = 256;

		private:
			RuntimeProfiler()
			{
				QueryPerformanceFrequency(&freq_);
			}

			~RuntimeProfiler()
			{
				save();
			}

		public:
			static RuntimeProfiler& instance()
			{
				static RuntimeProfiler inst;
				return inst;
			}

			void begin(const wchar_t* tag)
			{
				if (Report* rep = report(tag))
				{
					QueryPerformanceCounter(&rep->start_time);
				}
			}

			void end(const wchar_t* tag)
			{
				if (Report* rep = report(tag))
				{
					QueryPerformanceCounter(&rep->end_time);

					long long elapsed_time = rep->end_time.QuadPart - rep->start_time.QuadPart;
					rep->call_cnt++;

					for (int i = 0; i < TRIM_CNT; ++i)
					{
						if (rep->max_elapsed[i] == 0)
						{
							rep->max_elapsed[i] = elapsed_time;
							return;
						}

						if (rep->max_elapsed[i] < elapsed_time)
						{
							long long temp = rep->max_elapsed[i];
							rep->max_elapsed[i] = elapsed_time;
							elapsed_time = temp;
						}
					}

					for (int i = 0; i < TRIM_CNT; ++i)
					{
						if (rep->min_elapsed[i] == 0)
						{
							rep->min_elapsed[i] = elapsed_time;
							return;
						}

						if (rep->min_elapsed[i] > elapsed_time)
						{
							long long temp = rep->min_elapsed[i];
							rep->min_elapsed[i] = elapsed_time;
							elapsed_time = temp;
						}
					}

					rep->total_elapsed += elapsed_time;
				}
			}

			Report* report(const wchar_t* tag)
			{
				if (tag == nullptr)
				{
					return nullptr;
				}

				// idx를 끝 다음 인덱스로 놓기
				int idx = report_cnt_;

				for (int i = 0; i < report_cnt_; ++i)
				{
					if (wcscmp(tag, report_list_[i].tag) == 0) // 매번 태그 비교
					{
						idx = i;
						break;
					}
				}

				// 끝 다음 인덱스가 최대치다 -> 더 이상 저장 못한다.
				if (idx == MAX_REPORT_CNT)
				{
					return nullptr;
				}

				// tag가 nullptr이 아니다. -> 기존 리포트가 존재한다.
				if (report_list_[idx].tag)
				{
					return &report_list_[idx];
				}

				// 끝 다음 공간에 새 리포트를 등록하며 끝 다음 수를 증가시킨다.
				report_list_[report_cnt_++].tag = tag;

				return &report_list_[idx];
			}

			void save()
			{
				DWORD log_attr = GetFileAttributesW(L"log");
				if (log_attr == INVALID_FILE_ATTRIBUTES || (log_attr & FILE_ATTRIBUTE_DIRECTORY) != 0)
				{
					CreateDirectoryW(L"log", nullptr);
				}

				wchar_t filename[64];


				SYSTEMTIME st;
				GetLocalTime(&st);

				swprintf_s(filename, L"log/PROFILE_%d%02d%02d_%02d%02d%02d.txt",
					st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

				HANDLE hFile = CreateFileW(
					filename,
					GENERIC_WRITE,
					0,
					nullptr,
					CREATE_ALWAYS,
					FILE_ATTRIBUTE_NORMAL,
					nullptr);

				if (hFile == INVALID_HANDLE_VALUE)
				{
					return;
				}

				DWORD bytes_written = 0;

				// UTF-16 BOM
				WORD bom = 0xFEFF;
				WriteFile(hFile, &bom, sizeof(bom), &bytes_written, nullptr);

				// 헤더 작성
				wchar_t line[MAX_LINE];

				int len = swprintf_s(line, L" %-30s | %-13s | %-17s |", L"Tag", L"Calls", L"Average (us)");

				for (int i = 0; i < TRIM_CNT; ++i)
				{
					len += swprintf_s(line + len, MAX_LINE - len, L" %-17s |", L"Min (us)");
				}
				for (int i = 0; i < TRIM_CNT; ++i)
				{
					len += swprintf_s(line + len, MAX_LINE - len, L" %-17s |", L"Max (us)");
				}
				len += swprintf_s(line + len, MAX_LINE - len, L" Ratio (%%)\r\n");

				WriteFile(hFile, line, len * sizeof(wchar_t), &bytes_written, nullptr);

				// 구분선
				len = 0;
				for (int i = 0; i < 16; ++i)
				{
					len += swprintf_s(line + len, MAX_LINE - len, L"----------");
				}
				len += swprintf_s(line + len, MAX_LINE - len, L"\r\n");

				WriteFile(hFile, line, len * sizeof(wchar_t), &bytes_written, nullptr);

				long long us = freq_.QuadPart / 1'000'000;

				Report* base_report = nullptr;
				double base_avg = 0;
				double avg_time[MAX_REPORT_CNT]{};

				for (int i = 0; i < report_cnt_; ++i)
				{
					if (report_list_[i].call_cnt > TOTAL_TRIM_CNT)
					{
						avg_time[i] = static_cast<double>(report_list_[i].total_elapsed) /
							(report_list_[i].call_cnt - TOTAL_TRIM_CNT) / us;

						if (avg_time[i] > base_avg)
						{
							base_report = &report_list_[i];
							base_avg = avg_time[i];
						}
					}
				}

				// 기준 출력
				if (base_report)
				{
					len = swprintf_s(line, MAX_LINE, L" %-30s | %-13lld | %-17.1f |",
						base_report->tag, base_report->call_cnt, base_avg);
					for (int i = 0; i < TRIM_CNT; ++i)
					{
						len += swprintf_s(line + len, MAX_LINE - len, L" %-17.1f |",
							static_cast<double>(base_report->min_elapsed[i]) / us);
					}
					for (int i = TRIM_CNT - 1; i >= 0; --i)
					{
						len += swprintf_s(line + len, MAX_LINE - len, L" %-17.1f |",
							static_cast<double>(base_report->max_elapsed[i]) / us);
					}
					len += swprintf_s(line + len, MAX_LINE - len, L" 100.0\r\n");

					WriteFile(hFile, line, len * sizeof(wchar_t), &bytes_written, nullptr);
				}

				// 나머지 출력
				for (int i = 0; i < report_cnt_; ++i)
				{
					if (&report_list_[i] == base_report)
						continue;

					len = swprintf_s(line, MAX_LINE, L" %-30s | %-13lld | %-17.1f |",
						report_list_[i].tag, report_list_[i].call_cnt, avg_time[i]);
					for (int j = 0; j < TRIM_CNT; ++j)
					{
						len += swprintf_s(line + len, MAX_LINE - len, L" %-17.1f |",
							static_cast<double>(report_list_[i].min_elapsed[j]) / us);
					}
					for (int j = TRIM_CNT - 1; j >= 0; --j)
					{
						len += swprintf_s(line + len, MAX_LINE - len, L" %-17.1f |",
							static_cast<double>(report_list_[i].max_elapsed[j]) / us);
					}

					if (base_avg > 0.0)
					{
						len += swprintf_s(line + len, MAX_LINE - len, L" %5.1f\r\n",
							avg_time[i] * 100 / base_avg);
					}
					else
					{
						len += swprintf_s(line + len, MAX_LINE - len, L" -----\r\n");
					}

					WriteFile(hFile, line, len * sizeof(wchar_t), &bytes_written, nullptr);
				}

				CloseHandle(hFile);
			} // save()

		private:
			LARGE_INTEGER freq_;
			int report_cnt_ = 0;
			Report report_list_[MAX_REPORT_CNT]{};
		};

		class Stopwatch
		{
		public:
			Stopwatch(const wchar_t* tag) :tag_(tag)
			{
				RuntimeProfiler::instance().begin(tag);
			}

			~Stopwatch()
			{
				RuntimeProfiler::instance().end(tag_);
			}
		private:
			const wchar_t* tag_;
		};
	}
}

#ifdef RUNTIME_PROFILER
#define PROFILE_FUNC(X)		tool::profile::Stopwatch stopwatch(X)
#define PROFILE_BEGIN(X)	tool::profile::RuntimeProfiler::instance().begin(X)
#define PROFILE_END(X)		tool::profile::RuntimeProfiler::instance().end(X)
#else
#define PROFILE_FUNC(X)
#define PROFILE_BEGIN(X)
#define PROFILE_END(X)
#endif // RUNTIME_PROFILER