#pragma once

#include "ring_buffer.h"

#include <WinSock2.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

constexpr DWORD TPS = 50;
constexpr DWORD MSPT = 1000 / TPS;
constexpr DWORD MSPM = 1000;
constexpr DWORD RECV_TIMEOUT = 30000;

constexpr unsigned int SEED = 77;

constexpr u_short SERVER_PORT = 20000;

constexpr short LEFT = 0;
constexpr short RIGHT = 6400;
constexpr short HALF_RIGHT = RIGHT / 2;
constexpr short TOP = 0;
constexpr short BOTTOM = 6400;
constexpr short HALF_BOTTOM = BOTTOM / 2;

constexpr short SECTOR_SIZE = 160;
constexpr short SECTOR_ROW = BOTTOM / SECTOR_SIZE;
constexpr short SECTOR_COL = RIGHT / SECTOR_SIZE;

constexpr short DELTA_X = 3;
constexpr short DELTA_Y = 2;

constexpr short ERROR_RANGE = 50;

constexpr short ATTACK_1_RANGE_X = 80;
constexpr short ATTACK_1_RANGE_Y = 10;
constexpr short ATTACK_2_RANGE_X = 90;
constexpr short ATTACK_2_RANGE_Y = 10;
constexpr short ATTACK_3_RANGE_X = 100;
constexpr short ATTACK_3_RANGE_Y = 20;

constexpr char ATTACK_1_DAMAGE = 1;
constexpr char ATTACK_2_DAMAGE = 2;
constexpr char ATTACK_3_DAMAGE = 3;

constexpr char DEFAULT_HP = 100;

constexpr char DIR_LL = 0;
constexpr char DIR_UL = 1;
constexpr char DIR_UU = 2;
constexpr char DIR_UR = 3;
constexpr char DIR_RR = 4;
constexpr char DIR_DR = 5;
constexpr char DIR_DD = 6;
constexpr char DIR_DL = 7;
constexpr char DIR_XX = 8;

constexpr unsigned char SC_CREATE_MY_CHARACTER = 0;
constexpr unsigned char SC_CREATE_OTHER_CHARACTER = 1;
constexpr unsigned char SC_DELETE_CHARACTER = 2;

constexpr unsigned char CS_MOVE_START = 10;
constexpr unsigned char SC_MOVE_START = 11;
constexpr unsigned char CS_MOVE_STOP = 12;
constexpr unsigned char SC_MOVE_STOP = 13;

constexpr unsigned char CS_ATTACK_1 = 20;
constexpr unsigned char SC_ATTACK_1 = 21;
constexpr unsigned char CS_ATTACK_2 = 22;
constexpr unsigned char SC_ATTACK_2 = 23;
constexpr unsigned char CS_ATTACK_3 = 24;
constexpr unsigned char SC_ATTACK_3 = 25;

constexpr unsigned char SC_DAMAGE = 30;

constexpr unsigned char SC_SYNC = 251;
constexpr unsigned char CS_ECHO = 252;
constexpr unsigned char SC_ECHO = 253;

constexpr unsigned char HEADER_CODE = 0x89;

struct Header
{
	unsigned char code;
	unsigned char size;
	unsigned char type;
};

union Pos
{
	struct
	{
		short y;
		short x;
	};
	int k;
};

union SectorPos
{
	struct
	{
		short row;
		short col;
	};
	int k;
};

struct SectorAround
{
	SectorPos around[9];
	int cnt;
};

struct Session
{
	unsigned int id = 0;
	unsigned int is_valid = 0;
	SOCKET sock = INVALID_SOCKET;
	RingBuffer recv_q;
	RingBuffer send_q;
	DWORD last_recvd_time = 0;
};

struct Character
{
	Session* session = nullptr;
	unsigned int id = 0;
	Pos pos{};
	char direction = 0;
	char facing = 0;
	char hp = 0;
	bool is_valid = false;
	SectorPos sector_pos{};
};