
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

#include "knet_controller.h"
#include "knet_socket.h"

KNetController::KNetController()
{

}

KNetController::~KNetController()
{

}

void KNetController::OnSocketTick(KNetSocket&, s64 now_ms)
{

}

void KNetController::OnSocketReadable(KNetSocket& s, s64 now_ms)
{
	char buf[100];
	int len = 100;
	sockaddr_in6 in6;
	int addr_len = sizeof(in6);
	int ret = recvfrom(s.skt(), buf, 100, 0, (sockaddr*)&in6, &addr_len);
	if (ret < 0)
	{
		LogError() << "error";
		return;
	}
	buf[ret] = '\0';
	LogDebug() << "recv from:" << buf;
	s.SendTo();
	sendto(s.skt(), "result", 6, 0, (sockaddr*)&in6, addr_len);
}



s32 KNetController::Destroy()
{
	for (auto& s : nss_)
	{
		s.DestroySocket();
	}
	nss_.clear();
	return 0;
}



s32 KNetController::StartConnect(std::string uuid, const KNetConfigs& configs)
{
	u32 has_error = 0;
	for (auto& c : configs)
	{
		auto iter = nss_.emplace_back();
		s32 ret = iter->InitSocket(c.localhost.c_str(), c.localport, c.remote_ip.c_str(), c.remote_port);
		if (ret != 0)
		{
			LogError() << "init " << c.localhost << ":" << c.localport << " --> " << c.remote_ip << ":" << c.remote_port << " has error";
			has_error ++;
			continue;
		}

		LogInfo() << "init " << iter->local().debug_string() << " --> " << iter->remote().debug_string() << " success";
		iter->set_state(KNTS_HANDSHAKE_CH);
		ret = iter->SendTo();
		if (ret != 0)
		{
			LogError() << "SendTo " << iter->local().debug_string() << " --> " << iter->remote().debug_string() << " has error";
			has_error++;
			continue;
		}	
	}
	
	if (has_error < configs.size())
	{
		return 0;
	}
	return -1;
}




s32 KNetController::StartServer(const KNetConfigs& configs)
{
	bool has_error = false;
	for (auto& c : configs)
	{
		auto iter = nss_.emplace_back();
		s32 ret = iter->InitSocket(c.localhost.c_str(), c.localport, NULL, 0);
		if (ret != 0)
		{
			LogError() << "init " << c.localhost << ":" << c.localport << " has error";
			has_error = true;
			break;
		}
		LogInfo() << "init " << c.localhost << ":" << c.localport << " success";
		iter->set_state(KNTS_ESTABLISHED);
	}

	if (has_error)
	{
		Destroy();
		return -1;
	}
	return 0;
}



s32 KNetController::DoSelect()
{
	return Select(nss_, 0);
}
