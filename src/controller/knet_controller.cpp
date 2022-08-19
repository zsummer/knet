
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

void KNetController::OnSocketTick(KNetSocket&s, s64 now_ms)
{
	if (s.state_ == KNTS_INVALID)
	{
		return;
	}
	if (s.refs_ == 0)
	{
		LogDebug() << s;
		//s.DestroySocket();
		return;
	}
}

void KNetController::OnSocketReadable(KNetSocket& s, s64 now_ms)
{
	LogDebug() << s;
	char buf[100];
	int len = 100;
	sockaddr_in6 in6;
	int addr_len = sizeof(in6);
	int ret = recvfrom(s.skt_, buf, 100, 0, (sockaddr*)&in6, &addr_len);
	if (ret < 0)
	{
		LogError() << "error:" << KNetEnv::GetLastError();
		return;
	}
	buf[ret] = '\0';
	LogDebug() << "recv from:" << buf;
	s.SendTo();
	sendto(s.skt_, "result", 6, 0, (sockaddr*)&in6, addr_len);
}



s32 KNetController::Destroy()
{
	//clean session

	//clean sockets
	for (auto& s : nss_)
	{
		s.DestroySocket();
	}
	nss_.clear();
	return 0;
}



s32 KNetController::StartConnect(std::string uuid, const KNetConfigs& configs, s32&session_id)
{
	u32 has_error = 0;
	session_id = -1;
	if (sessions_.full())
	{
		KNetEnv::Errors()++;
		return -1;
	}


	for (auto& c : configs)
	{
		KNetSocket* ns = PopFreeSocket();
		if (ns == NULL)
		{
			KNetEnv::Errors()++;
			continue;
		}

		s32 ret = ns->InitSocket(c.localhost.c_str(), c.localport, c.remote_ip.c_str(), c.remote_port);
		if (ret != 0)
		{
			LogError() << "init " << c.localhost << ":" << c.localport << " --> " << c.remote_ip << ":" << c.remote_port << " has error";
			has_error ++;
			KNetEnv::Errors()++;
			continue;
		}

		LogInfo() << "init " << ns->local_.debug_string() << " --> " << ns->remote_.debug_string() << " success";
		ns->state_ = KNTS_BINDED;
		if (session_id == -1)
		{

		}


		ns->state_ = KNTS_HANDSHAKE_CH;

		ret = ns->SendTo();
		if (ret != 0)
		{
			LogError() << "SendTo " << ns->local_.debug_string() << " --> " << ns->remote_.debug_string() << " has error";
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
		KNetSocket* ns = PopFreeSocket();
		if (ns == NULL)
		{
			KNetEnv::Errors()++;
			continue;
		}

		s32 ret = ns->InitSocket(c.localhost.c_str(), c.localport, NULL, 0);
		if (ret != 0)
		{
			LogError() << "init " << c.localhost << ":" << c.localport << " has error";
			has_error = true;
			KNetEnv::Errors()++;
			break;
		}

		LogInfo() << "init " << c.localhost << ":" << c.localport << " success";
		ns->state_ = KNTS_ESTABLISHED;
		ns->refs_++;
		ns->flag_ |= KNTS_SERVER;
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


KNetSocket* KNetController::PopFreeSocket()
{
	for (u32 i = 0; i < nss_.size(); i++)
	{
		KNetSocket& s = nss_[i];
		if (s.state_ == KNTS_INVALID)
		{
			return &s;
		}
	}

	if (nss_.full())
	{
		return NULL;
	}
	nss_.emplace_back(nss_.size());
	return &nss_.back();
}


void KNetController::PushFreeSocket(KNetSocket* s)
{
	if (s->state_ == KNTS_INVALID)
	{
		return;
	}
	s->DestroySocket();
}













