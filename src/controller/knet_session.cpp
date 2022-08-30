
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

#include "knet_session.h"

KNetSession::KNetSession(s32 inst_id)
{
    inst_id_ = inst_id;
    state_ = KNTS_INVALID;
	init();
}




KNetSession::~KNetSession()
{

}

s32 KNetSession::init()
{
	if (state_ != KNTS_INVALID)
	{
		return -1;
	}
	state_ = KNTS_LOCAL_INITED;
	session_id_ = 0;
	shake_id_ = 0;
	snd_pkt_id_ = 0;
	encrypt_key = "";
	configs_.clear();
	memset(sg_, 0, sizeof(sg_));
	memset(sg_, 0, sizeof(sg_));
	memset(box_, 0, sizeof(box_));
	for (auto& slot : slots_)
	{
		slot.inst_id_ = -1;
		slot.last_active_ = 0;
	}
	return 0;
}




s32 KNetSession::destroy()
{
	for (auto& slot : slots_)
	{
		if (slot.inst_id_ != -1)
		{
			return -1;
		}
	}
    state_ = KNTS_INVALID;
	return 0;
}


