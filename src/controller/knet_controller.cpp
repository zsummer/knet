
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
#include "knet_rudp.h"



KNetController::KNetController()
{
	controller_state_ = 0;
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
	char buf[KNT_UDATA_SIZE];
	int len = KNT_UDATA_SIZE;
	KNetAddress remote;
	s32 ret = s.RecvFrom(buf, len, remote, now_ms);
	if (ret != 0)
	{
		return ;
	}

	if (len < KNT_UHDR_SIZE)
	{
		return;
	}
	KNetUHDR uhdr;
	const char* p = buf;
	p = KNetDecodeUHDR(p, uhdr);
	u64 umac = KNetHSMac(p, p - buf, uhdr);
	if (umac != uhdr.mac)
	{
		LogDebug() << "umac error. real:" << (void*)umac << ", hdr umac:" << (void*)uhdr.mac;
		return;
	}
	if (uhdr.pkt_size != len)
	{
		LogDebug() << "pkt size error. real:" << len << ", hdr pkt_size:" << uhdr.pkt_size;
		return;
	}

	if (buf[len - 1] != '\0')
	{
		LogDebug() << "test error.";
		return;
	}
	LogDebug() << "PSH:" << p;
	uhdr.pkt_id++;
	uhdr.mac = KNetHSMac(p, p-buf, uhdr);
	KNetEncodeUHDR(buf, uhdr);
	s.SendTo(buf, len, remote);
	
}



s32 KNetController::Destroy()
{
	CleanSession();

	return 0;
}



s32 KNetController::StartConnect(KNetHandshakeKey hkey, const KNetConfigs& configs)
{
	u32 has_error = 0;
	KNetSession* session = NULL;
	if (!KNetHelper::CheckKey(hkey))
	{
		KNetEnv::Errors()++;
		return -1;
	}

	//if (sessions_.full())
	{
		//KNetEnv::Errors()++;
		//return -1;
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
		if (session == NULL)
		{
			KNetEnv::Status(KNT_STT_SES_CREATE_EVENTS)++;
			session = new KNetSession();
			session->hkey_ = hkey;
		}
		KNetSocketSlot addr;
		addr.skt_id_ = ns->skt_id_;
		addr.last_active_ = KNetEnv::Now();
		addr.remote_ = ns->remote_;
		session->slots_.push_back(addr);
		ns->state_ = KNTS_HANDSHAKE_CH;
		ns->refs_++;

		char	buf[1000];
		s32 len = 0;
		KNetUHDR uhdr;
		memset(&uhdr, 0, sizeof(uhdr));
		strcpy(buf + KNT_UHDR_SIZE, "abcde");
		uhdr.pkt_size = KNT_UHDR_SIZE + 6;
		uhdr.mac = KNetHSMac(buf + KNT_UHDR_SIZE, 6, uhdr);
		KNetEncodeUHDR(buf, uhdr);

		ret = ns->SendTo(buf, KNT_UHDR_SIZE + 6, addr.remote_);
		if (ret != 0)
		{
			LogError() << "SendTo " << ns->local_.debug_string() << " --> " << ns->remote_.debug_string() << " has error";
			has_error++;
			continue;
		}	
	}
	if (has_error == configs.size())
	{
		RemoveSession(session);
		return -1;
	}
	handshakes_.insert(std::make_pair(hkey, session));


	return 0;
}


s32 KNetController::CleanSession()
{
	//clean session
	while (!handshakes_.empty())
	{
		RemoveSession(handshakes_.begin()->second->hkey_, 0);
	}

	while (!establisheds_.empty())
	{
		RemoveSession(establisheds_.begin()->second->hkey_, establisheds_.begin()->second->session_id_);
	}
	for (auto& s : nss_)
	{
		s.DestroySocket();
	}
	nss_.clear();
	return 0;
}



s32 KNetController::RemoveSession(KNetHandshakeKey hkey, u64 session_id)
{
	KNetSession* session = NULL;
	if (true)
	{
		auto iter = handshakes_.find(hkey);
		if (iter != handshakes_.end())
		{
			session = iter->second;
			handshakes_.erase(iter);
		}
	}
	if (true)
	{
		auto iter = establisheds_.find(session_id);
		if (iter != establisheds_.end())
		{
			if (session != NULL)
			{
				KNetEnv::Errors()++;
			}
			session = iter->second;
			establisheds_.erase(iter);
		}
	}
	return RemoveSession(session);
}


s32 KNetController::RemoveSession(KNetSession* session)
{
	if (session == NULL)
	{
		return 0;
	}
	
	for (auto s: session->slots_)
	{
		if (s.skt_id_ < 0 || s.skt_id_ >= (u32)nss_.size())
		{
			KNetEnv::Errors()++;
			continue;
		}
		KNetSocket& skt = nss_[s.skt_id_];
		if (skt.skt_id_ != s.skt_id_)
		{
			KNetEnv::Errors()++;
			continue;
		}
		if (skt.refs_ <= 0)
		{
			KNetEnv::Errors()++;
			continue;
		}
		skt.refs_--;
	}
	KNetEnv::Status(KNT_STT_SES_DESTROY_EVENTS)++;
	delete session;
	return 0;
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
			KNetEnv::Status(KNT_STT_SKT_ALLOC_EVENTS)++;
			return &s;
		}
	}

	if (nss_.full())
	{
		return NULL;
	}
	nss_.emplace_back(nss_.size());
	KNetEnv::Status(KNT_STT_SKT_ALLOC_EVENTS)++;
	return &nss_.back();
}


void KNetController::PushFreeSocket(KNetSocket* s)
{
	if (s->state_ == KNTS_INVALID)
	{
		return;
	}
	KNetEnv::Status(KNT_STT_SKT_FREE_EVENTS)++;
	s->DestroySocket();

}













