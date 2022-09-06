
/*
* knet License
* Copyright (C) 2019 YaweiZhang <yawei.zhang@foxmail.com>.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#pragma once
#ifndef _KNET_ENV_H_
#define _KNET_ENV_H_
#include "knet_base.h"
#include <chrono>

enum KNET_STATUS
{
	KNT_STT_NONE,

	KNT_STT_SKT_INSTRUCT_COUNT,
	KNT_STT_SKT_DESTRUCT_COUNT,

	KNT_STT_SKT_INIT_COUNT,
	KNT_STT_SKT_DESTROY_COUNT,

	KNT_STT_SKT_ALLOC_COUNT,
	KNT_STT_SKT_FREE_COUNT,

	KNT_STT_SKT_SND_COUNT,
	KNT_STT_SKT_RCV_COUNT,
	KNT_STT_SKT_SND_BYTES,
	KNT_STT_SKT_RCV_BYTES,




	KNT_STT_SKT_CONNECT_COUNT,


	KNT_STT_SKT_HANDSHAKE_CH_SND_COUNT,
	KNT_STT_SKT_HANDSHAKE_CH_RCV_COUNT,
	KNT_STT_SKT_HANDSHAKE_SH_SND_COUNT,
	KNT_STT_SKT_HANDSHAKE_SH_RCV_COUNT,
	KNT_STT_SKT_ESTABLISHED_COUNT,
	KNT_STT_SKT_LINGER_COUNT,

	KNT_STT_SES_CREATE_COUNT,
	KNT_STT_SES_DESTROY_COUNT,
	KNT_STT_SES_ALLOC_COUNT,
	KNT_STT_SES_FREE_COUNT,


	KNT_STT_SES_ON_ACCEPT,
	KNT_STT_SES_ON_CONNECTED,
	KNT_STT_SES_CONNECTED,


	KNT_STT_CTL_START_COUNT,
	KNT_STT_CTL_DESTROY_COUNT,

	KNT_STT_MAX,
};


const static u32 KNET_DEVICE_NAME_LEN = 64;
struct KNetDeviceInfo
{
	static const u32 PKT_SIZE = KNET_DEVICE_NAME_LEN * 6;
	char device_name[KNET_DEVICE_NAME_LEN];
	char device_type[KNET_DEVICE_NAME_LEN];
	char device_mac[KNET_DEVICE_NAME_LEN];
	char sys_name[KNET_DEVICE_NAME_LEN];
	char sys_version[KNET_DEVICE_NAME_LEN];
	char os_name[KNET_DEVICE_NAME_LEN];
	bool check_str(char* src, s32 src_len)
	{
		for (s32 i = 0; i < src_len; i++)
		{
			if (src[i] == '\0')
			{
				return true;
			}
		}
		return false;
	}

	bool is_valid()
	{
		return check_str(device_name, KNET_DEVICE_NAME_LEN)
			&& check_str(device_type, KNET_DEVICE_NAME_LEN)
			&& check_str(device_mac, KNET_DEVICE_NAME_LEN)
			&& check_str(sys_name, KNET_DEVICE_NAME_LEN)
			&& check_str(sys_version, KNET_DEVICE_NAME_LEN)
			&& check_str(os_name, KNET_DEVICE_NAME_LEN);
	}
};
static_assert(sizeof(KNetDeviceInfo) == KNetDeviceInfo::PKT_SIZE, "");


class KNetEnv
{
public:
	KNetEnv()
	{
		init_env();
	}
	~KNetEnv()
	{
		clean_env();
	}
	static s32 init_env()
	{
#ifdef _WIN32
		WSADATA wsa_data;
		s32 ret = WSAStartup(MAKEWORD(2, 2), &wsa_data);
		if (ret != NO_ERROR)
		{
			return -1;
		}
#endif
		return 0;
	}

	static void clean_env()
	{
#ifdef _WIN32
		WSACleanup();
#endif
	}

	static s32 error_code()
	{
#ifdef _WIN32
		return WSAGetLastError();
#else
		return errno;
#endif
	}

	static s64 now_ms()
	{
		return  std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}


	static s32& error_count();

	static s64& count(KNET_STATUS id);
	static void clean_count();
	static void print_count();


	static u64 create_seq_id();
	static u64 create_session_id();


	static s32 fill_device_info(KNetDeviceInfo& dvi);
};




#endif


