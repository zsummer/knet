
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

s32 KNetSelect::do_select(KNetSockets& sets, s64 wait_ms)
{
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = (int)wait_ms * 1000;

	fd_set rdfds;
	FD_ZERO(&rdfds);
	SOCKET max_fd = -1; //windows is error but will ignore
	s32 set_cnt = 0;
	for (auto& s : sets)
	{
		if (s.skt() == INVALID_SOCKET)
		{
			continue;
		}
		if (s.skt() > max_fd)
		{
			max_fd = s.skt();
		}
		FD_SET(s.skt(), &rdfds);
		set_cnt++;
	}

	if (set_cnt == 0)
	{
		if (wait_ms > 0)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
		}
		return 0;
	}

	s32 ret = select(max_fd + 1, &rdfds, NULL, NULL, &tv);
	if (ret < 0)
	{
		s32 error = KNetEnv::error_code();
		if (error == EINTR)
		{
			return 0;
		}
		KNetEnv::error_count()++;
		LogError() << " error";
		return -2;
	}
	s64 post_now_ms = KNetEnv::now_ms();
	for (auto& s : sets)
	{
		if (s.skt() == INVALID_SOCKET)
		{
			continue;
		}

		if (FD_ISSET(s.skt(), &rdfds))
		{
			on_readable(s, post_now_ms);
		}
	}
	return 0;
}




