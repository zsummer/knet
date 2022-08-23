
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

	KNT_STT_CTL_START_EVENTS,
	KNT_STT_CTL_DESTROY_EVENTS,

	KNT_STT_MAX,
};



class KNetEnv
{
public:
	KNetEnv()
	{
		Startup();
	}
	~KNetEnv()
	{
		CleanUp();
	}
	static s32 Startup()
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

	static void CleanUp()
	{
#ifdef _WIN32
		WSACleanup();
#endif
	}

	static s32 GetLastError()
	{
#ifdef _WIN32
		return WSAGetLastError();
#else
		return errno;
#endif
	}

	static s64 Now()
	{
		return  std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}


	static s32& Errors();

	static s64& Status(KNET_STATUS id);
	static void CleanStatus();


	static u32 CreateSequenceID();
	static u64 CreatePKGID();
	static u64 CreateSessionID();

};




#endif


