
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

void KNetController::OnSocketReadable(KNetSocket&, s64 now_ms)
{

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



s32 KNetController::StartServer(const MultiChannel& channels)
{
	bool has_error = false;
	for (auto& c : channels)
	{
		auto iter = nss_.emplace_back();
		s32 ret = iter->InitSocket(c.first.c_str(), c.second, NULL, 0);
		if (ret != 0)
		{
			LogError() << "init " << c.first << ":" << c.second << " has error";
			has_error = true;
			break;
		}
		LogInfo() << "init " << c.first << ":" << c.second << " success";
		iter->set_state(KNTS_ESTABLISHED);
	}

	if (has_error)
	{
		Destroy();
		return -1;
	}
	return 0;
}




