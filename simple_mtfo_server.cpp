#include "config.h"
#include "object_pool.h"
#include "ring_buffer.h"
#include "runtime_profiler.h"
#include "serialize_buffer.h"
#include "simple_logger.h"

#include <WinSock2.h>
#include <ws2def.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <conio.h>
#include <timeapi.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <list>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Ws2_32.lib")

ObjectPool<Character> character_pool;
ObjectPool<Session> session_pool;
ObjectPool<SerializeBuffer> msg_pool;

std::unordered_map<unsigned int, Character*> character_map;
std::unordered_map<unsigned int, Session*> session_map;

std::list<Character*> sector_2d[SECTOR_ROW][SECTOR_COL];

std::vector<Session*> select_arr(FD_SETSIZE);
int select_idx;

std::vector<Session*> release_arr(10000);
int release_cnt;

SOCKET listen_sock = INVALID_SOCKET;
TIMEVAL timeout{ 0,0 };

unsigned int is_running = 1;

unsigned int unique_id = 1;

int quadrant = 0;

// 모니터 용도
unsigned int tps = 0; // tick per second

// 로깅 용도
unsigned int aps = 0; // accept per second
unsigned int rps = 0; // release per second
unsigned int rmps = 0; // rcvd msg per second
unsigned int smps = 0; // sent msg per second

unsigned int cur_sync_cnt = 0;

bool control_mode = false;

DWORD max_delay_time = 0;

bool startup();
int cleanup();
void stop(const wchar_t* caller, int line);

void update(DWORD cur_time, int loop_cnt);
void control();

void io_proc();
void accept_proc();
void select_proc(fd_set& read_fds, fd_set& write_fds);
void recv_proc(Session* session);
void send_proc(Session* session);
void reserve_disconnect(Session* session);
void release_proc();

void send_unicast(Session* session, SerializeBuffer* msg);
void send_sector(SectorPos pos, SerializeBuffer* msg, Session* exclude);
void send_around(SectorPos pos, SerializeBuffer* msg, Session* exclude);

void sector_around(SectorAround& around, SectorPos pos);
void sector_left_arc(SectorAround& around, SectorPos pos);
void sector_right_arc(SectorAround& around, SectorPos pos);

void update_sector(Character* character, SectorPos cur_pos);

void create_character(Session* session);
void create_character_sector(Character* character, int sector_key);
void delete_character_sector(Character* character, int sector_key);

bool dispatch(Session* session, unsigned char type, SerializeBuffer* msg);

bool cs_move_start(Session* session, SerializeBuffer* msg);
bool cs_move_stop(Session* session, SerializeBuffer* msg);
bool cs_attack_1(Session* session, SerializeBuffer* msg);
bool cs_attack_2(Session* session, SerializeBuffer* msg);
bool cs_attack_3(Session* session, SerializeBuffer* msg);
bool cs_echo(Session* session, SerializeBuffer* msg);

void check_damage(Character* character, SerializeBuffer* msg, Pos range, char damage);

void mp_sc_create_my_character(SerializeBuffer* msg, unsigned int id, char facing, Pos pos, char hp);
void mp_sc_create_other_character(SerializeBuffer* msg, unsigned int id, char facing, Pos pos, char hp);
void mp_sc_delete_character(SerializeBuffer* msg, unsigned int id);
void mp_sc_move_start(SerializeBuffer* msg, unsigned int id, char direction, Pos pos);
void mp_sc_move_stop(SerializeBuffer* msg, unsigned int id, char facing, Pos pos);
void mp_sc_attack_1(SerializeBuffer* msg, unsigned int id, char facing, Pos pos);
void mp_sc_attack_2(SerializeBuffer* msg, unsigned int id, char facing, Pos pos);
void mp_sc_attack_3(SerializeBuffer* msg, unsigned int id, char facing, Pos pos);
void mp_sc_damage(SerializeBuffer* msg, unsigned int attacker_id, unsigned int victim_id, char victim_hp);
void mp_sc_sync(SerializeBuffer* msg, unsigned int id, Pos pos);
void mp_sc_echo(SerializeBuffer* msg, DWORD time);

#define SERVER_STOP stop(__FUNCTIONW__, __LINE__)

int main()
{
	if (startup())
	{
		const DWORD start_tick_time = timeGetTime();

		DWORD fixed_tick_time = start_tick_time;
		DWORD fixed_monitor_time = start_tick_time;
		DWORD last_update_time = start_tick_time;
		DWORD cur_time;
		DWORD elapsed;

		unsigned int total_sync_cnt = 0;
		int loop_cnt = 0;


		fixed_tick_time -= MSPT;

		while (is_running)
		{
			PROFILE_BEGIN(L"TICK");
			io_proc();
			++tps;

			cur_time = timeGetTime();

			elapsed = cur_time - last_update_time;
			if (elapsed > max_delay_time)
			{
				max_delay_time = elapsed;
			}

			loop_cnt = (cur_time - fixed_tick_time) / MSPT;
			if (loop_cnt > 0)
			{
				last_update_time = cur_time;

				update(cur_time, loop_cnt);

				fixed_tick_time += MSPT * loop_cnt;
			}

			control();

			cur_time = timeGetTime();

			// MONITOR
			if (cur_time - fixed_monitor_time >= MSPM)
			{
				PROFILE_BEGIN(L"MONITOR");

				DWORD time_passed = cur_time - start_tick_time;

				wprintf(L"==============[MONITOR]==============\n"
					L" TIME PASSED  |      %12u ms\n"
					L" TPS     MDT  | %-7u %u\n"
					L" APS     RPS  | %-7u %u\n"
					L" RMPS    SMPS | %-7u %u\n"
					L" SESSION CHAR | %-7zu %zu\n"
					L" CONTROL MODE | %d | 0=LOCK 1=UNLOCK\n"
					L" L-LOCK U-UNLOCK K-KILL\n"
					L"=====================================\n",
					time_passed,
					tps, max_delay_time,
					aps, rps,
					rmps, smps,
					session_map.size(), character_map.size(),
					control_mode);

				if (tps < 10)
				{
					LOG_SYS(L"MONITOR", L"tps under 10: tps=%u", tps);
				}

				if (max_delay_time > 250)
				{
					LOG_SYS(L"MONITOR", L"max delay time over 250: mdt=%u", max_delay_time);
				}

				if (session_map.size() != character_map.size())
				{
					LOG_SYS(L"MONITOR", L"session and character cnt miss match: session=%zu character=%zu",
						session_map.size(), character_map.size());
					SERVER_STOP;
				}

				if (cur_sync_cnt > 0)
				{
					total_sync_cnt += cur_sync_cnt;
					LOG_SYS(L"MONITOR", L"sync cnt increased: tsc=%u", total_sync_cnt);
					cur_sync_cnt = 0;
				}

				LOG_SYS(L"MONITOR", L"time=%-12u sess=%-5zu char=%-5zu aps=%-4u rps=%-4u rmps=%-4u smps=%u",
					time_passed, session_map.size(), character_map.size(), aps, rps, rmps, smps);

				tps = 0;
				max_delay_time = 0;
				aps = rps = rmps = smps = 0;

				fixed_monitor_time += MSPM;

				PROFILE_END(L"MONITOR");
			}

			PROFILE_BEGIN(L"SLEEP");
			elapsed = timeGetTime() - fixed_tick_time;
			if (elapsed < MSPT)
			{
				Sleep(MSPT - elapsed);
			}
			PROFILE_END(L"SLEEP");
			PROFILE_END(L"TICK");
		}
	}

	return cleanup();
}

bool startup()
{
	do
	{
		timeBeginPeriod(1);

		srand(SEED);

		LOG_INIT_ERR;

		WSADATA wsa_data;
		int ret = WSAStartup(MAKEWORD(2, 2), &wsa_data);
		if (ret != 0)
		{
			LOG_ERR(L"STARTUP", L"WSAStartup() failed: error code=%d", WSAGetLastError());
			break;
		}

		listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (listen_sock == INVALID_SOCKET)
		{
			LOG_ERR(L"STARTUP", L"socket() failed: error code=%d", WSAGetLastError());
			break;
		}

		SOCKADDR_IN server_addr;
		ZeroMemory(&server_addr, sizeof(server_addr));
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(SERVER_PORT);
		server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		if (bind(listen_sock,
			reinterpret_cast<SOCKADDR*>(&server_addr),
			sizeof(server_addr)) == SOCKET_ERROR)
		{
			LOG_ERR(L"STARTUP", L"bind() failed: error code=%d", WSAGetLastError());
			break;
		}

		LINGER linger_opt{ 1,0 };
		if (setsockopt(listen_sock,
			SOL_SOCKET, SO_LINGER,
			reinterpret_cast<char*>(&linger_opt), sizeof(linger_opt))
			== SOCKET_ERROR)
		{
			LOG_ERR(L"STARTUP", L"setsockopt(SO_LINGER) failed: error code=%d", WSAGetLastError());
			break;
		}

		int nagle_off = 1;
		if (setsockopt(listen_sock,
			IPPROTO_TCP, TCP_NODELAY,
			reinterpret_cast<char*>(&nagle_off), sizeof(nagle_off))
			== SOCKET_ERROR)
		{
			LOG_ERR(L"STARTUP", L"setsockopt(TCP_NODELAY) failed: error code=%d", WSAGetLastError());
			break;
		}

		u_long ioctl_mode = 1;
		if (ioctlsocket(listen_sock, FIONBIO, &ioctl_mode) == SOCKET_ERROR)
		{
			LOG_ERR(L"STARTUP", L"ioctlsocket() failed: error code=%d", WSAGetLastError());
			break;
		}

		if (listen(listen_sock, SOMAXCONN_HINT(65535)) == SOCKET_ERROR)
		{
			LOG_ERR(L"STARTUP", L"listen() failed: error code=%d", WSAGetLastError());
			break;
		}

		LOG_SYS(L"STARTUP", L"startup() complete");

		return true;
	} while (false);

	LOG_SYS(L"STARTUP", L"startup() failed");

	return false;
}

int cleanup()
{
	if (listen_sock != INVALID_SOCKET)
	{
		closesocket(listen_sock);
		listen_sock = INVALID_SOCKET;
	}

	for (auto& it : session_map)
	{
		Session* session = it.second;
		closesocket(session->sock);

		auto ch_it = character_map.find(session->id);
		if (ch_it != character_map.end())
		{
			Character* character = ch_it->second;
			sector_2d[character->sector_pos.row][character->sector_pos.col].remove(character);

			character_pool.dealloc(character);

			character_map.erase(ch_it);
		}

		session_pool.dealloc(session);
	}

	session_map.clear();
	character_map.clear();

	WSACleanup();

	for (int r = 0; r < SECTOR_ROW; ++r)
	{
		for (int c = 0; c < SECTOR_COL; ++c)
		{
			sector_2d[r][c].clear();
		}
	}

	timeEndPeriod(1);

	LOG_SYS(L"CLEANUP", L"server cleanup complete");

	return 0;
}

void stop(const wchar_t* caller, int line)
{
	LOG_SYS(L"SERVER", L"server stopped: caller=%s line=%d", caller, line);
}

void update(DWORD cur_time, int loop_cnt)
{
	PROFILE_FUNC(L"UPDATE");

	short x;
	short y;
	bool is_moved;

	for (auto& it : character_map)
	{
		Character* character = it.second;

		if (character->hp <= 0)
		{
			reserve_disconnect(character->session);
			continue;
		}

		DWORD delta_time = cur_time - character->session->last_recvd_time;
		if (delta_time > RECV_TIMEOUT)
		{
			LOG_DBG(L"UPDATE", L"session recv timeout: id=%u time=%u", character->id, delta_time);
			reserve_disconnect(character->session);
			continue;
		}

		is_moved = true;

		switch (character->direction)
		{
		case DIR_LL:
			x = character->pos.x - (DELTA_X * loop_cnt);

			if (x <= LEFT)
			{
				x = LEFT;
			}

			character->pos.x = x;
			break;
		case DIR_UL:
			x = character->pos.x - (DELTA_X * loop_cnt);
			y = character->pos.y - (DELTA_Y * loop_cnt);

			if (x <= LEFT)
			{
				x = LEFT;
				y = character->pos.y;
			}

			if (y <= TOP)
			{
				x = character->pos.x;
				y = TOP;
			}

			character->pos.x = x;
			character->pos.y = y;
			break;
		case DIR_UU:
			y = character->pos.y - (DELTA_Y * loop_cnt);

			if (y <= TOP)
			{
				y = TOP;
			}

			character->pos.y = y;
			break;
		case DIR_UR:
			x = character->pos.x + (DELTA_X * loop_cnt);
			y = character->pos.y - (DELTA_Y * loop_cnt);

			if (x >= RIGHT)
			{
				x = RIGHT - 1;
				y = character->pos.y;
			}

			if (y <= TOP)
			{
				x = character->pos.x;
				y = TOP;
			}

			character->pos.x = x;
			character->pos.y = y;
			break;
		case DIR_RR:
			x = character->pos.x + (DELTA_X * loop_cnt);

			if (x >= RIGHT)
			{
				x = RIGHT - 1;
			}

			character->pos.x = x;
			break;
		case DIR_DR:
			x = character->pos.x + (DELTA_X * loop_cnt);
			y = character->pos.y + (DELTA_Y * loop_cnt);

			if (x >= RIGHT)
			{
				x = RIGHT - 1;
				y = character->pos.y;
			}

			if (y >= BOTTOM)
			{
				x = character->pos.x;
				y = BOTTOM - 1;
			}

			character->pos.x = x;
			character->pos.y = y;
			break;
		case DIR_DD:
			y = character->pos.y + (DELTA_Y * loop_cnt);

			if (y >= BOTTOM)
			{
				y = BOTTOM - 1;
			}

			character->pos.y = y;
			break;
		case DIR_DL:
			x = character->pos.x - (DELTA_X * loop_cnt);
			y = character->pos.y + (DELTA_Y * loop_cnt);

			if (x <= LEFT)
			{
				x = LEFT;
				y = character->pos.y;
			}

			if (y >= BOTTOM)
			{
				x = character->pos.x;
				y = BOTTOM - 1;
			}

			character->pos.x = x;
			character->pos.y = y;
			break;
		case DIR_XX:
			is_moved = false;
			break;
		}

		if (is_moved)
		{
			SectorPos cur_sector;

			cur_sector.row = character->pos.y / SECTOR_SIZE;
			cur_sector.col = character->pos.x / SECTOR_SIZE;

			if (character->sector_pos.k != cur_sector.k)
			{
				update_sector(character, cur_sector);
			}
		}
	}
}

void control()
{
	PROFILE_FUNC(L"CONTROL");

	wint_t key;
	if (_kbhit())
	{
		key = _getwch();

		if (L'u' == key || L'U' == key)
		{
			control_mode = true;
		}
		else if (L'l' == key || L'L' == key)
		{
			control_mode = false;
		}
		else if (control_mode && (L'k' == key || L'K' == key))
		{
			is_running = 0;
		}
	}
}

void io_proc()
{
	PROFILE_FUNC(L"IO PROC");

	fd_set read_fds;
	fd_set write_fds;

	do
	{
		release_proc();

		FD_ZERO(&read_fds);
		FD_SET(listen_sock, &read_fds);

		int r_select = select(0, &read_fds, nullptr, nullptr, &timeout);
		if (r_select == SOCKET_ERROR)
		{
			LOG_ERR(L"IOPROC", L"select() failed: error code=%d", WSAGetLastError());
			SERVER_STOP;
			break;
		}

		if (FD_ISSET(listen_sock, &read_fds))
		{
			accept_proc();
		}

		if (session_map.empty())
		{
			break;
		}

		FD_ZERO(&read_fds);
		FD_ZERO(&write_fds);

		select_idx = 0;

		for (auto& it : session_map)
		{
			Session* session = it.second;

			FD_SET(session->sock, &read_fds);

			if (!session->send_q.empty())
			{
				FD_SET(session->sock, &write_fds);
			}

			select_arr[select_idx++] = session;

			if (select_idx == FD_SETSIZE)
			{
				select_proc(read_fds, write_fds);
				select_idx = 0;
			}
		}

		if (select_idx != 0)
		{
			select_proc(read_fds, write_fds);
		}
	} while (false);
}

void accept_proc()
{
	PROFILE_FUNC(L"ACCEPT PROC");

	SOCKET client_sock;
	SOCKADDR_IN client_addr;
	int addr_len = sizeof(client_addr);

	int e_accept;

	while (true)
	{
		client_sock = accept(listen_sock, reinterpret_cast<SOCKADDR*>(&client_addr), &addr_len);
		if (client_sock == INVALID_SOCKET)
		{
			e_accept = WSAGetLastError();
			if (e_accept != WSAEWOULDBLOCK)
			{
				LOG_ERR(L"ACCEPTPROC", L"accept() failed: error code=%d", e_accept);
				SERVER_STOP;
			}
			break;
		}

		PROFILE_BEGIN(L"ACCEPT ONE CLIENT");

		Session* session = session_pool.alloc();
		if (session == nullptr)
		{
			LOG_ERR(L"ACCEPTPROC", L"session pool allocation failed");
			SERVER_STOP;
			break;
		}

		session->id = unique_id++;
		session->is_valid = 1;
		session->sock = client_sock;
		session->recv_q.clear();
		session->send_q.clear();
		session->last_recvd_time = timeGetTime();

		session_map[session->id] = session;

		LOG_DBG(L"ACCEPTPROC", L"new client joined: id=%u", session->id);

		++aps;

		create_character(session);

		PROFILE_END(L"ACCEPT ONE CLIENT");
	}
}

void select_proc(fd_set& read_fds, fd_set& write_fds)
{
	PROFILE_FUNC(L"SELECT PROC");

	if (select_idx > FD_SETSIZE)
	{
		__debugbreak();
	}

	int r_select = select(0, &read_fds, &write_fds, nullptr, &timeout);
	if (r_select == SOCKET_ERROR)
	{
		LOG_ERR(L"SELECTPROC", L"select() failed: error code=%d", WSAGetLastError());
		SERVER_STOP;
		return;
	}

	for (int i = 0; i < select_idx; ++i)
	{
		if (FD_ISSET(select_arr[i]->sock, &read_fds))
		{
			recv_proc(select_arr[i]);
		}

		if (FD_ISSET(select_arr[i]->sock, &write_fds))
		{
			send_proc(select_arr[i]);
		}
	}

	FD_ZERO(&read_fds);
	FD_ZERO(&write_fds);
}

void recv_proc(Session* session)
{
	PROFILE_FUNC(L"RECV PROC");

	if (session->is_valid == 0)
	{
		return;
	}

	RingBuffer& rq = session->recv_q;

	int r_recv = recv(session->sock, rq.write_pos(), static_cast<int>(rq.direct_write_size()), 0);
	if (r_recv > 0)
	{
		size_t r_mwp = rq.move_write_pos(r_recv);
		if (r_mwp != r_recv)
		{
			reserve_disconnect(session);

			LOG_ERR(L"RECVPROC", L"rq.move_write_pos() failed: id=%u expected=%d result=%zu",
				session->id, r_recv, r_mwp);
		}
		else
		{
			Header header;
			SerializeBuffer* msg = msg_pool.alloc();
			if (msg == nullptr)
			{
				LOG_ERR(L"RECVPROC", L"msg pool allocation failed");
				SERVER_STOP;
				return;
			}

			size_t size = rq.size();

			while (size >= sizeof(header))
			{
				size_t r_pk = rq.peek(reinterpret_cast<char*>(&header), sizeof(header));
				if (r_pk != sizeof(header))
				{
					reserve_disconnect(session);

					LOG_ERR(L"RECVPROC", L"rq.peek(header) failed: id=%u expected=%zu result=%zu",
						session->id, sizeof(header), r_pk);

					break;
				}

				if (size < sizeof(header) + header.size)
				{
					break;
				}

				size_t r_mrp = rq.move_read_pos(sizeof(header));
				if (r_mrp != sizeof(header))
				{
					reserve_disconnect(session);

					LOG_ERR(L"RECVPROC", L"rq.move_read_pos() failed: id=%u expected=%zu result=%zu",
						session->id, sizeof(header), r_mrp);

					break;
				}

				msg->clear();

				size_t r_read = rq.read(msg->read_pos(), header.size);
				if (r_read != header.size)
				{
					reserve_disconnect(session);

					LOG_ERR(L"RECVPROC", L"rq.read() failed: id=%u expected=%u result=%zu",
						session->id, header.size, r_read);

					break;
				}

				size_t r_msg_mwp = msg->move_write_pos(r_read);
				if (r_msg_mwp != r_read)
				{
					reserve_disconnect(session);

					LOG_ERR(L"RECVPROC", L"msg.move_write_pos() failed: id=%u expected=%zu result=%zu",
						session->id, r_read, r_msg_mwp);

					break;
				}

				++rmps;

				size = rq.size();

				if (dispatch(session, header.type, msg))
				{
					session->last_recvd_time = timeGetTime();
				}
				else
				{
					reserve_disconnect(session);
					LOG_ERR(L"RECVPROC", L"dispatch() failed: id=%u recv_q.use_size=%zu",
						session->id, size);
					break;
				}
			}

			if (!msg_pool.dealloc(msg))
			{
				reserve_disconnect(session);
				LOG_ERR(L"RECVPROC", L"msg pool deallocation failed");
				SERVER_STOP;
			}
		}
	}
	else if (r_recv == 0)
	{
		reserve_disconnect(session);
		LOG_DBG(L"RECVPROC", L"client connection closed: id=%u", session->id);
	}
	else
	{
		int e_recv = WSAGetLastError();
		if (e_recv != WSAEWOULDBLOCK)
		{
			reserve_disconnect(session);

			if (e_recv != WSAECONNABORTED && e_recv != WSAECONNRESET)
			{
				LOG_ERR(L"RECVPROC", L"recv() failed: error code=%d", e_recv);
			}
		}
	}
}

void send_proc(Session* session)
{
	PROFILE_FUNC(L"SEND PROC");

	if (session->is_valid == 0)
	{
		return;
	}

	RingBuffer& sq = session->send_q;

	int r_send = send(session->sock, sq.read_pos(), static_cast<int>(sq.direct_read_size()), 0);
	if (r_send != SOCKET_ERROR)
	{
		size_t r_mrp = sq.move_read_pos(r_send);
		if (r_mrp != r_send)
		{
			reserve_disconnect(session);

			LOG_ERR(L"SENDPROC", L"sq.move_read_pos() failed: expected=%d result=%zu", r_send, r_mrp);
		}
	}
	else
	{
		int e_send = WSAGetLastError();
		if (e_send != WSAEWOULDBLOCK)
		{
			reserve_disconnect(session);

			if (e_send != WSAECONNABORTED && e_send != WSAECONNRESET)
			{
				LOG_ERR(L"SENDPROC", L"send() failed: error code=%d", e_send);
			}
		}
	}
}

void reserve_disconnect(Session* session)
{
	if (session->is_valid == 1)
	{
		session->is_valid = 0;
		release_arr[release_cnt++] = session;
	}
}

void release_proc()
{
	PROFILE_FUNC(L"RELEASE PROC");

	SerializeBuffer* msg = msg_pool.alloc();
	if (msg == nullptr)
	{
		LOG_ERR(L"RELEASEPROC", L"msg pool allocation failed");
		SERVER_STOP;
		return;
	}

	for (int i = 0; i < release_cnt; ++i)
	{
		Session* session = release_arr[i];
		unsigned int id = session->id;

		auto it = character_map.find(id);
		if (it == character_map.end())
		{
			LOG_ERR(L"RELEASEPROC", L"character map find failed: id=%u", id);
			SERVER_STOP;
			break;
		}

		Character* character = it->second;

		mp_sc_delete_character(msg, id);
		send_around(character->sector_pos, msg, session);

		sector_2d[character->sector_pos.row][character->sector_pos.col].remove(character);
		character_map.erase(id);

		if (!character_pool.dealloc(character))
		{
			LOG_ERR(L"RELEASEPROC", L"character pool deallocation failed");
			SERVER_STOP;
			break;
		}

		closesocket(session->sock);

		session_map.erase(id);

		if (!session_pool.dealloc(session))
		{
			LOG_ERR(L"RELEASEPROC", L"session pool deallocation failed");
			SERVER_STOP;
			break;
		}

		++rps;
	}

	if (!msg_pool.dealloc(msg))
	{
		LOG_ERR(L"RELEASEPROC", L"msg pool deallocation failed");
		SERVER_STOP;
	}

	release_cnt = 0;
}

void send_unicast(Session* session, SerializeBuffer* msg)
{
	RingBuffer& sq = session->send_q;

	size_t msg_size = msg->size();
	size_t r_eq = sq.write(msg->read_pos(), msg_size);
	if (r_eq != msg_size)
	{
		reserve_disconnect(session);

		LOG_ERR(L"SENDUNICAST", L"sq.write() failed: id=%u expected=%zu result=%zu",
			session->id, msg_size, r_eq);
	}
	else
	{
		++smps;
	}
}

void send_sector(SectorPos pos, SerializeBuffer* msg, Session* exclude)
{
	PROFILE_FUNC(L"SEND SECTOR");

	for (auto other : sector_2d[pos.row][pos.col])
	{
		if (other->session == exclude)
		{
			continue;
		}

		send_unicast(other->session, msg);
	}
}

void send_around(SectorPos pos, SerializeBuffer* msg, Session* exclude)
{
	PROFILE_FUNC(L"SEND AROUND");

	SectorAround around;
	sector_around(around, pos);

	for (int i = 0; i < around.cnt; ++i)
	{
		send_sector(around.around[i], msg, exclude);
	}
}

void sector_around(SectorAround& around, SectorPos pos)
{
	int cnt = 0;

	for (short r = -1; r <= 1; ++r)
	{
		short tr = pos.row + r;

		if (tr < 0 || tr >= SECTOR_ROW)
		{
			continue;
		}

		for (short c = -1; c <= 1; ++c)
		{
			short tc = pos.col + c;

			if (tc < 0 || tc >= SECTOR_COL)
			{
				continue;
			}

			around.around[cnt++] = { tr,tc };
		}
	}

	around.cnt = cnt;
}

void sector_left_arc(SectorAround& around, SectorPos pos)
{
	int cnt = 0;

	for (short r = -1; r <= 1; ++r)
	{
		short tr = pos.row + r;

		if (tr < 0 || tr >= SECTOR_ROW)
		{
			continue;
		}

		for (short c = -1; c <= 0; ++c)
		{
			short tc = pos.col + c;

			if (tc < 0 || tc >= SECTOR_COL)
			{
				continue;
			}

			around.around[cnt++] = { tr,tc };
		}
	}

	around.cnt = cnt;
}

void sector_right_arc(SectorAround& around, SectorPos pos)
{
	int cnt = 0;

	for (short r = -1; r <= 1; ++r)
	{
		short tr = pos.row + r;

		if (tr < 0 || tr >= SECTOR_ROW)
		{
			continue;
		}

		for (short c = 0; c <= 1; ++c)
		{
			short tc = pos.col + c;

			if (tc < 0 || tc >= SECTOR_COL)
			{
				continue;
			}

			around.around[cnt++] = { tr,tc };
		}
	}

	around.cnt = cnt;
}

void update_sector(Character* character, SectorPos cur_pos)
{
	PROFILE_FUNC(L"UPDATE SECTOR");

	SectorPos old_pos;
	old_pos.k = character->sector_pos.k;

	character->sector_pos.k = cur_pos.k;

	sector_2d[old_pos.row][old_pos.col].remove(character);
	sector_2d[cur_pos.row][cur_pos.col].push_back(character);

	SectorAround old_around;
	sector_around(old_around, old_pos);

	SectorAround cur_around;
	sector_around(cur_around, cur_pos);

	std::unordered_set<int> old_sector_set;
	std::unordered_set<int> cur_sector_set;

	for (int i = 0; i < old_around.cnt; ++i)
	{
		old_sector_set.insert(old_around.around[i].k);
	}

	for (int i = 0; i < cur_around.cnt; ++i)
	{
		cur_sector_set.insert(cur_around.around[i].k);
	}

	for (int k : old_sector_set)
	{
		if (!cur_sector_set.erase(k))
		{
			delete_character_sector(character, k);
		}
	}

	for (int k : cur_sector_set)
	{
		create_character_sector(character, k);
	}
}

void create_character(Session* session)
{
	PROFILE_FUNC(L"CREATE CHARACTER");

	do
	{
		Character* character = character_pool.alloc();
		if (character == nullptr)
		{
			LOG_ERR(L"CRTCHAR", L"character pool allocation failed");
			SERVER_STOP;
			break;
		}

		short x = (rand() % HALF_RIGHT) + (HALF_RIGHT * (quadrant % 2));
		short y = (rand() % HALF_BOTTOM) + (HALF_BOTTOM * (quadrant / 2));
		quadrant = (quadrant + 1) % 4;

		short row = y / SECTOR_SIZE;
		short col = x / SECTOR_SIZE;

		character->session = session;
		character->id = session->id;
		character->pos.x = x;
		character->pos.y = y;
		character->direction = DIR_XX;
		character->facing = DIR_RR;
		character->hp = DEFAULT_HP;
		character->is_valid = true;
		character->sector_pos.row = row;
		character->sector_pos.col = col;

		character_map[character->id] = character;

		sector_2d[row][col].push_back(character);

		SerializeBuffer* msg = msg_pool.alloc();
		if (msg == nullptr)
		{
			LOG_ERR(L"CRTCHAR", L"msg pool allocation failed");
			SERVER_STOP;
			break;
		}

		mp_sc_create_my_character(msg, character->id, character->facing, character->pos, character->hp);
		send_unicast(session, msg);

		SectorAround around;
		sector_around(around, character->sector_pos);

		for (int i = 0; i < around.cnt; ++i)
		{
			short sector_row = around.around[i].row;
			short sector_col = around.around[i].col;

			// 주변 섹터 9개에 남들에게 내가 왔다고 보내기
			mp_sc_create_other_character(msg, character->id, character->facing, character->pos, character->hp);

			for (auto other : sector_2d[sector_row][sector_col])
			{
				if (other == character)
				{
					continue;
				}

				send_unicast(other->session, msg);
			}

			for (auto other : sector_2d[sector_row][sector_col])
			{
				if (other == character)
				{
					continue;
				}

				mp_sc_create_other_character(msg, other->id, other->facing, other->pos, other->hp);
				send_unicast(session, msg);

				if (other->direction != DIR_XX)
				{
					mp_sc_move_start(msg, other->id, other->direction, other->pos);
					send_unicast(session, msg);
				}
			}
		}

		if (!msg_pool.dealloc(msg))
		{
			LOG_ERR(L"CRTCHAR", L"msg pool deallocation failed");
			SERVER_STOP;
			break;
		}
	} while (false);
}

void create_character_sector(Character* character, int sector_key)
{
	PROFILE_FUNC(L"CREATE CHARACTER SECTOR");

	SectorPos pos;
	pos.k = sector_key;

	SerializeBuffer* msg = msg_pool.alloc();
	if (msg == nullptr)
	{
		LOG_ERR(L"CRTCHARSEC", L"message pool allocation failed");
		SERVER_STOP;
		return;
	}

	mp_sc_create_other_character(msg, character->id, character->facing, character->pos, character->hp);
	send_sector(pos, msg, character->session);

	if (character->direction != DIR_XX)
	{
		mp_sc_move_start(msg, character->id, character->direction, character->pos);
		send_sector(pos, msg, character->session);
	}

	for (auto other : sector_2d[pos.row][pos.col])
	{
		if (other == character)
		{
			continue;
		}

		mp_sc_create_other_character(msg, other->id, other->facing, other->pos, other->hp);
		send_unicast(character->session, msg);

		if (other->direction != DIR_XX)
		{
			mp_sc_move_start(msg, other->id, other->direction, other->pos);
			send_unicast(character->session, msg);
		}
	}

	if (!msg_pool.dealloc(msg))
	{
		LOG_ERR(L"CRTCHARSEC", L"message pool deallocation failed");
		SERVER_STOP;
		return;
	}
}

void delete_character_sector(Character* character, int sector_key)
{
	PROFILE_FUNC(L"DELETE CHARACTER SECTOR");

	SectorPos pos;
	pos.k = sector_key;

	SerializeBuffer* msg = msg_pool.alloc();
	if (msg == nullptr)
	{
		LOG_ERR(L"DELCHARSEC", L"message pool allocation failed");
		SERVER_STOP;
		return;
	}

	mp_sc_delete_character(msg, character->id);
	send_sector(pos, msg, character->session);

	for (auto other : sector_2d[pos.row][pos.col])
	{
		if (other == character)
		{
			continue;
		}

		mp_sc_delete_character(msg, other->id);
		send_unicast(character->session, msg);
	}

	if (!msg_pool.dealloc(msg))
	{
		LOG_ERR(L"DELCHARSEC", L"message pool deallocation failed");
		SERVER_STOP;
		return;
	}
}

bool dispatch(Session* session, unsigned char type, SerializeBuffer* msg)
{
	PROFILE_FUNC(L"DISPATCH");

	try
	{
		switch (type)
		{
		case CS_MOVE_START:
			return cs_move_start(session, msg);
		case CS_MOVE_STOP:
			return cs_move_stop(session, msg);
		case CS_ATTACK_1:
			return cs_attack_1(session, msg);
		case CS_ATTACK_2:
			return cs_attack_2(session, msg);
		case CS_ATTACK_3:
			return cs_attack_3(session, msg);
		case CS_ECHO:
			return cs_echo(session, msg);
		default:
			return false;
		}
	}
	catch (const std::runtime_error& e)
	{
		LOG_ERR(L"DISPATCH", L"exception caught: %hs", e.what());
		return false;
	}
}

bool cs_move_start(Session* session, SerializeBuffer* msg)
{
	PROFILE_FUNC(L"CS MOVE START");

	char direction;
	Pos client_pos;

	*msg >> direction >> client_pos.x >> client_pos.y;

	auto it = character_map.find(session->id);
	if (it == character_map.end())
	{
		reserve_disconnect(session);

		LOG_ERR(L"MOVESTART", L"character map find failed: id=%u", session->id);

		return false;
	}

	Character* character = it->second;

	character->direction = direction;

	switch (direction)
	{
	case DIR_LL:
	case DIR_UL:
	case DIR_DL:
		character->facing = DIR_LL;
		break;
	case DIR_UR:
	case DIR_RR:
	case DIR_DR:
		character->facing = DIR_RR;
		break;
	}

	if (abs(client_pos.x - character->pos.x) >= ERROR_RANGE || abs(client_pos.y - character->pos.y) >= ERROR_RANGE)
	{
		++cur_sync_cnt;

		LOG_ERR(L"MOVESTART", L"sync failed: server x=%d server y=%d client x=%d client y=%d",
			character->pos.x, character->pos.y, client_pos.x, client_pos.y);

		mp_sc_sync(msg, character->id, character->pos);
		send_unicast(session, msg);

		mp_sc_move_start(msg, character->id, character->direction, character->pos);
		send_around(character->sector_pos, msg, session);
	}
	else
	{
		character->pos.k = client_pos.k;

		SectorPos cur_sector;

		cur_sector.row = client_pos.y / SECTOR_SIZE;
		cur_sector.col = client_pos.x / SECTOR_SIZE;

		if (cur_sector.k != character->sector_pos.k)
		{
			update_sector(character, cur_sector);
		}

		mp_sc_move_start(msg, character->id, character->direction, character->pos);
		send_around(cur_sector, msg, session);
	}

	return true;
}

bool cs_move_stop(Session* session, SerializeBuffer* msg)
{
	PROFILE_FUNC(L"CS MOVE STOP");

	char facing;
	Pos client_pos;

	*msg >> facing >> client_pos.x >> client_pos.y;

	auto it = character_map.find(session->id);
	if (it == character_map.end())
	{
		reserve_disconnect(session);

		LOG_ERR(L"MOVESTOP", L"character map find failed: id=%u", session->id);

		return false;
	}

	Character* character = it->second;

	character->direction = DIR_XX;
	character->facing = facing;



	if (abs(client_pos.x - character->pos.x) >= ERROR_RANGE || abs(client_pos.y - character->pos.y) >= ERROR_RANGE)
	{
		LOG_ERR(L"MOVESTOP", L"sync failed: server x=%d server y=%d client x=%d client y=%d",
			character->pos.x, character->pos.y, client_pos.x, client_pos.y);

		++cur_sync_cnt;

		mp_sc_sync(msg, character->id, character->pos);
		send_unicast(session, msg);

		mp_sc_move_stop(msg, character->id, facing, character->pos);
		send_around(character->sector_pos, msg, session);
	}
	else
	{
		character->pos.k = client_pos.k;

		SectorPos cur_sector;

		cur_sector.row = client_pos.y / SECTOR_SIZE;
		cur_sector.col = client_pos.x / SECTOR_SIZE;

		if (cur_sector.k != character->sector_pos.k)
		{
			update_sector(character, cur_sector);
		}

		mp_sc_move_stop(msg, character->id, facing, character->pos);
		send_around(cur_sector, msg, session);
	}

	return true;
}

bool cs_attack_1(Session* session, SerializeBuffer* msg)
{
	PROFILE_FUNC(L"CS ATTACK 1");

	char facing;
	Pos client_pos;

	*msg >> facing >> client_pos.x >> client_pos.y;

	auto it = character_map.find(session->id);
	if (it == character_map.end())
	{
		reserve_disconnect(session);

		LOG_ERR(L"ATTACK1", L"character map find failed: id=%u", session->id);

		return false;
	}

	Character* character = it->second;

	character->facing = facing;

	if (abs(client_pos.x - character->pos.x) >= ERROR_RANGE || abs(client_pos.y - character->pos.y) >= ERROR_RANGE)
	{
		++cur_sync_cnt;

		LOG_ERR(L"ATTACK1", L"sync failed: server x=%d server y=%d client x=%d client y=%d",
			character->pos.x, character->pos.y, client_pos.x, client_pos.y);

		mp_sc_sync(msg, character->id, character->pos);
		send_unicast(session, msg);

		mp_sc_attack_1(msg, character->id, facing, character->pos);
		send_around(character->sector_pos, msg, session);
	}
	else
	{
		character->pos.k = client_pos.k;

		SectorPos cur_sector;

		cur_sector.row = client_pos.y / SECTOR_SIZE;
		cur_sector.col = client_pos.x / SECTOR_SIZE;

		if (cur_sector.k != character->sector_pos.k)
		{
			update_sector(character, cur_sector);
		}

		mp_sc_attack_1(msg, character->id, facing, character->pos);
		send_around(cur_sector, msg, session);
	}

	Pos range = { ATTACK_1_RANGE_Y,ATTACK_1_RANGE_X };
	check_damage(character, msg, range, ATTACK_1_DAMAGE);

	return true;
}

bool cs_attack_2(Session* session, SerializeBuffer* msg)
{
	PROFILE_FUNC(L"CS ATTACK 2");

	char facing;
	Pos client_pos;

	*msg >> facing >> client_pos.x >> client_pos.y;

	auto it = character_map.find(session->id);
	if (it == character_map.end())
	{
		reserve_disconnect(session);

		LOG_ERR(L"ATTACK2", L"character map find failed: id=%u", session->id);

		return false;
	}

	Character* character = it->second;

	character->facing = facing;

	if (abs(client_pos.x - character->pos.x) >= ERROR_RANGE || abs(client_pos.y - character->pos.y) >= ERROR_RANGE)
	{
		++cur_sync_cnt;

		LOG_ERR(L"ATTACK2", L"sync failed: server x=%d server y=%d client x=%d client y=%d",
			character->pos.x, character->pos.y, client_pos.x, client_pos.y);

		mp_sc_sync(msg, character->id, character->pos);
		send_unicast(session, msg);

		mp_sc_attack_2(msg, character->id, facing, character->pos);
		send_around(character->sector_pos, msg, session);
	}
	else
	{
		character->pos.k = client_pos.k;

		SectorPos cur_sector;

		cur_sector.row = client_pos.y / SECTOR_SIZE;
		cur_sector.col = client_pos.x / SECTOR_SIZE;

		if (cur_sector.k != character->sector_pos.k)
		{
			update_sector(character, cur_sector);
		}

		mp_sc_attack_2(msg, character->id, facing, character->pos);
		send_around(cur_sector, msg, session);
	}

	Pos range = { ATTACK_2_RANGE_Y,ATTACK_2_RANGE_X };
	check_damage(character, msg, range, ATTACK_2_DAMAGE);

	return true;
}

bool cs_attack_3(Session* session, SerializeBuffer* msg)
{
	PROFILE_FUNC(L"CS ATTACK 3");

	char facing;
	Pos client_pos;

	*msg >> facing >> client_pos.x >> client_pos.y;

	auto it = character_map.find(session->id);
	if (it == character_map.end())
	{
		reserve_disconnect(session);

		LOG_ERR(L"ATTACK3", L"character map find failed: id=%u", session->id);

		return false;
	}

	Character* character = it->second;

	character->facing = facing;

	if (abs(client_pos.x - character->pos.x) >= ERROR_RANGE || abs(client_pos.y - character->pos.y) >= ERROR_RANGE)
	{
		++cur_sync_cnt;

		LOG_ERR(L"ATTACK3", L"sync failed: server x=%d server y=%d client x=%d client y=%d",
			character->pos.x, character->pos.y, client_pos.x, client_pos.y);

		mp_sc_sync(msg, character->id, character->pos);
		send_unicast(session, msg);

		mp_sc_attack_3(msg, character->id, facing, character->pos);
		send_around(character->sector_pos, msg, session);
	}
	else
	{
		character->pos.k = client_pos.k;

		SectorPos cur_sector;

		cur_sector.row = client_pos.y / SECTOR_SIZE;
		cur_sector.col = client_pos.x / SECTOR_SIZE;

		if (cur_sector.k != character->sector_pos.k)
		{
			update_sector(character, cur_sector);
		}

		mp_sc_attack_3(msg, character->id, facing, character->pos);
		send_around(cur_sector, msg, session);
	}

	Pos range = { ATTACK_3_RANGE_Y,ATTACK_3_RANGE_X };
	check_damage(character, msg, range, ATTACK_3_DAMAGE);

	return true;
}

bool cs_echo(Session* session, SerializeBuffer* msg)
{
	PROFILE_FUNC(L"CS ECHO");

	DWORD time;

	*msg >> time;

	mp_sc_echo(msg, time);
	send_unicast(session, msg);

	return true;
}

void check_damage(Character* character, SerializeBuffer* msg, Pos range, char damage)
{
	PROFILE_FUNC(L"CHECK DAMAGE");

	SectorAround around;
	bool hitted = false;

	short row;
	short col;

	if (character->facing == DIR_LL)
	{
		sector_left_arc(around, character->sector_pos);

		for (int i = 0; i < around.cnt; ++i)
		{
			row = around.around[i].row;
			col = around.around[i].col;

			for (auto other : sector_2d[row][col])
			{
				if (other == character)
				{
					continue;
				}

				if (character->pos.x < other->pos.x || other->pos.x + range.x < character->pos.x)
				{
					continue;
				}

				if (abs(character->pos.y - other->pos.y) > range.y)
				{
					continue;
				}

				other->hp -= damage;

				mp_sc_damage(msg, character->id, other->id, other->hp);
				send_around(character->sector_pos, msg, nullptr);

				hitted = true;
				break;
			}

			if (hitted)
			{
				break;
			}
		}

	}
	else
	{
		sector_right_arc(around, character->sector_pos);

		for (int i = 0; i < around.cnt; ++i)
		{
			row = around.around[i].row;
			col = around.around[i].col;

			for (auto other : sector_2d[row][col])
			{
				if (other == character)
				{
					continue;
				}

				if (other->pos.x < character->pos.x || character->pos.x + range.x < other->pos.x)
				{
					continue;
				}

				if (abs(character->pos.y - other->pos.y) > range.y)
				{
					continue;
				}

				other->hp -= damage;

				mp_sc_damage(msg, character->id, other->id, other->hp);
				send_around(character->sector_pos, msg, nullptr);

				hitted = true;
				break;
			}

			if (hitted)
			{
				break;
			}
		}
	}
}

void mp_sc_create_my_character(SerializeBuffer* msg, unsigned int id, char facing, Pos pos, char hp)
{
	msg->clear();
	Header header{ HEADER_CODE,10,SC_CREATE_MY_CHARACTER };
	msg->write(reinterpret_cast<char*>(&header), sizeof(header));
	*msg << id << facing << pos.x << pos.y << hp;
}

void mp_sc_create_other_character(SerializeBuffer* msg, unsigned int id, char facing, Pos pos, char hp)
{
	msg->clear();
	Header header{ HEADER_CODE,10,SC_CREATE_OTHER_CHARACTER };
	msg->write(reinterpret_cast<char*>(&header), sizeof(header));
	*msg << id << facing << pos.x << pos.y << hp;
}

void mp_sc_delete_character(SerializeBuffer* msg, unsigned int id)
{
	msg->clear();
	Header header{ HEADER_CODE,4,SC_DELETE_CHARACTER };
	msg->write(reinterpret_cast<char*>(&header), sizeof(header));
	*msg << id;
}

void mp_sc_move_start(SerializeBuffer* msg, unsigned int id, char direction, Pos pos)
{
	msg->clear();
	Header header{ HEADER_CODE,9,SC_MOVE_START };
	msg->write(reinterpret_cast<char*>(&header), sizeof(header));
	*msg << id << direction << pos.x << pos.y;
}

void mp_sc_move_stop(SerializeBuffer* msg, unsigned int id, char facing, Pos pos)
{
	msg->clear();
	Header header{ HEADER_CODE,9,SC_MOVE_STOP };
	msg->write(reinterpret_cast<char*>(&header), sizeof(header));
	*msg << id << facing << pos.x << pos.y;
}

void mp_sc_attack_1(SerializeBuffer* msg, unsigned int id, char facing, Pos pos)
{
	msg->clear();
	Header header{ HEADER_CODE,9,SC_ATTACK_1 };
	msg->write(reinterpret_cast<char*>(&header), sizeof(header));
	*msg << id << facing << pos.x << pos.y;
}

void mp_sc_attack_2(SerializeBuffer* msg, unsigned int id, char facing, Pos pos)
{
	msg->clear();
	Header header{ HEADER_CODE,9,SC_ATTACK_2 };
	msg->write(reinterpret_cast<char*>(&header), sizeof(header));
	*msg << id << facing << pos.x << pos.y;
}

void mp_sc_attack_3(SerializeBuffer* msg, unsigned int id, char facing, Pos pos)
{
	msg->clear();
	Header header{ HEADER_CODE,9,SC_ATTACK_3 };
	msg->write(reinterpret_cast<char*>(&header), sizeof(header));
	*msg << id << facing << pos.x << pos.y;
}

void mp_sc_damage(SerializeBuffer* msg, unsigned int attacker_id, unsigned int victim_id, char victim_hp)
{
	msg->clear();
	Header header{ HEADER_CODE,9,SC_DAMAGE };
	msg->write(reinterpret_cast<char*>(&header), sizeof(header));
	*msg << attacker_id << victim_id << victim_hp;
}

void mp_sc_sync(SerializeBuffer* msg, unsigned int id, Pos pos)
{
	msg->clear();
	Header header{ HEADER_CODE,8,SC_SYNC };
	msg->write(reinterpret_cast<char*>(&header), sizeof(header));
	*msg << id << pos.x << pos.y;
}

void mp_sc_echo(SerializeBuffer* msg, DWORD time)
{
	msg->clear();
	Header header{ HEADER_CODE,4,SC_ECHO };
	msg->write(reinterpret_cast<char*>(&header), sizeof(header));
	*msg << time;
}