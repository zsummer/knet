
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
#include "knet_turbo.h"

KNetSession::KNetSession(s32 inst_id)
{
    inst_id_ = inst_id;
    state_ = KNTS_INVALID;
	reset();
}




KNetSession::~KNetSession()
{
	if (kcp_ != NULL)
	{
		ikcp_release(kcp_);
		kcp_ = NULL;
	}
}

s32 KNetSession::reset()
{
	state_ = KNTS_INVALID;
	flag_ = 0;
	kcp_ = 0;
	session_id_ = 0;
	shake_id_ = 0;
	salt_id_ = 0;
	snd_pkt_id_ = 0;
	encrypt_key = "";
	configs_.clear();
	memset(sg_, 0, sizeof(sg_));
	memset(sg_, 0, sizeof(sg_));
	memset(box_, 0, sizeof(box_));
	for (auto& slot : slots_)
	{
		slot.inst_id_ = -1;
	}
	connect_time_ = 0;
	connect_expire_time_ = 0;
	on_connected_ = NULL;
	last_send_ts_ = 0;
	last_recv_ts_ = 0;
	connect_resends_ = 0;
	return 0;
}



s32 KNetSession::init(KNetTurbo& turbo, u16 flag)
{
	if (kcp_ != NULL)
	{
		return -1;
	}
	state_ = KNTS_CREATED;
	flag_ = flag;
	salt_id_ = ((u64)rand() << 32) | (u32)rand();

	kcp_ = ikcp_create(0, (void*)&turbo, inst_id_);
	ikcp_nodelay(kcp_, 1, KNET_UPDATE_INTERVAL, 1, 1);
	ikcp_setmtu(kcp_, KNT_UDAT_SIZE);
	ikcp_wndsize(kcp_, KNET_DEFAULT_SND_WND, KNET_DEFAULT_RCV_WND);
	kcp_->rx_minrto = KNET_FAST_MIN_RTO;
	kcp_->output = KNetTurbo::kcp_output;
	kcp_->writelog = KNetTurbo::kcp_writelog;
	kcp_->stream = 1;
	//kcp_->logmask = (IKCP_LOG_OUT_WINS << 1) - 1;
	KNetEnv::call_user(KNTP_SES_INIT);
	return 0;
}


s32 KNetSession::destory()
{
	state_ = KNTS_INVALID;
	if (kcp_ != NULL)
	{
		delete kcp_;
		kcp_ = NULL;
	}
	KNetEnv::call_user(KNTP_SES_DESTROY);
	return reset();
}





