
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


#include "knet_select.h"
#include "knet_env.h"
#include "knet_socket.h"

static const u32 knet_fixed_size_ = sizeof(KNetSocket);
static const u32 knet_array_fixed_size_ = sizeof(KNetSockets);

s32 KNetSelect::Select(KNetSockets& sets, s64 wait_ms)
{
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = wait_ms * 1000;

	s64 enter_now_ms = KNetEnv::GetNowMS();

	fd_set rdfds;
	FD_ZERO(&rdfds);
	SOCKET max_fd = -1; //windows is error but will ignore
	for (auto& s : sets)
	{
		OnSocketTick(s, enter_now_ms);
		if (s.state() < KNTS_BINDED || s.state() >= KNTS_LINGER)
		{
			continue;
		}
		if (s.skt() == INVALID_SOCKET)
		{
			KNetEnv::Errors()++;
			LogError() << "error";
			continue;
		}
		if (s.skt() > max_fd)
		{
			max_fd = s.skt();
		}
		FD_SET(s.skt(), &rdfds);
	}



	s32 ret = select(max_fd + 1, &rdfds, NULL, NULL, &tv);
	if (ret < 0)
	{
		s32 error = KNetEnv::GetLastError();
		if (error == EINTR)
		{
			return 0;
		}
		KNetEnv::Errors()++;
		LogError() << " error";
		return -2;
	}
	s64 post_now_ms = KNetEnv::GetNowMS();
	for (auto& s : sets)
	{
		if (s.state() < KNTS_BINDED || s.state() >= KNTS_LINGER)
		{
			continue;
		}
		if (s.skt() == INVALID_SOCKET)
		{
			KNetEnv::Errors()++;
			LogError() << "error";
			continue;
		}

		if (FD_ISSET(s.skt(), &rdfds))
		{
			OnSocketReadable(s, post_now_ms);
		}
	}
	return 0;
}




