
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
	KNT_STT_SKT_INSTRUCT_EVENTS,
	KNT_STT_SKT_ALLOC_EVENTS,
	KNT_STT_SKT_INIT_EVENTS,
	KNT_STT_SKT_BIND_EVENTS,
	KNT_STT_SKT_CONNECT_EVENTS,
	KNT_STT_SKT_HANDSHAKE_CH_SND_EVENTS,
	KNT_STT_SKT_HANDSHAKE_CH_RCV_EVENTS,
	KNT_STT_SKT_HANDSHAKE_SH_SND_EVENTS,
	KNT_STT_SKT_HANDSHAKE_SH_RCV_EVENTS,
	KNT_STT_SKT_ESTABLISHED_EVENTS,
	KNT_STT_SKT_LINGER_EVENTS,
	KNT_STT_SKT_DESTROY_EVENTS,
	KNT_STT_SKT_DESTRUCT_EVENTS,
	KNT_STT_SKT_FREE_EVENTS,

	KNT_STT_SKT_SND_EVENTS,
	KNT_STT_SKT_RCV_EVENTS,
	KNT_STT_SKT_SND_BYTES,
	KNT_STT_SKT_RCV_BYTES,

	KNT_STT_SES_CREATE_EVENTS,
	KNT_STT_SES_DESTROY_EVENTS,
	KNT_STT_SES_ALLOC_EVENTS,
	KNT_STT_SES_FREE_EVENTS,

	KNT_STT_CTL_START_EVENTS,
	KNT_STT_CTL_DESTROY_EVENTS,

	KNT_STT_MAX,
};


struct KNetDeviceInfo
{
	static const u32 PKT_SIZE = 128 * 6;
	char device_name[128];
	char device_type[128];
	char device_mac[128];
	char sys_name[128];
	char sys_version[128];
	char os_name[128];
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
		return check_str(device_name, 128)
			&& check_str(device_type, 128)
			&& check_str(device_mac, 128)
			&& check_str(sys_name, 128)
			&& check_str(sys_version, 128)
			&& check_str(os_name, 128);
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

	static s64& prof(KNET_STATUS id);
	static void clean_prof();


	static u64 create_seq_id();
	static u64 create_session_id();


	static s32 fill_device_info(KNetDeviceInfo& dvi);
};




#endif


