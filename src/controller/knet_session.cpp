
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
#include "knet_controller.h"

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
	if (state_ != KNTS_INVALID)
	{
		return -1;
	}
	state_ = KNTS_INIT;
	flag_ = 0;
	kcp_ = 0;
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


#define KCP_UPDATE_INTERVAL 10
#define KCP_DEFAULT_SND_WND (512)
#define KCP_DEFAULT_RECV_WND (512)
#define KCP_MAX_SND_WND (5120)
#define KCP_MAX_RECV_WND (5120)
#define KCP_FAST_MIN_RTO (50)
#define KCP_SVR_DEFAULT_RECV_BUFF_SIZE (2048*1024)

s32 KNetSession::init(KNetController& c, u16 flag)
{
	if (kcp_ != NULL)
	{
		return -1;
	}
	state_ = KNTS_INIT;
	flag_ = flag;
	kcp_ = ikcp_create(0, (void*)&c, inst_id_);
	ikcp_nodelay(kcp_, 1, 0, 2, 1);
	ikcp_setmtu(kcp_, KNT_UDAT_SIZE);
	ikcp_wndsize(kcp_, KCP_DEFAULT_SND_WND, KCP_DEFAULT_RECV_WND);
	kcp_->rx_minrto = KCP_FAST_MIN_RTO;
	kcp_->output = KNetController::kcp_output;
	kcp_->writelog = KNetController::kcp_writelog;
	kcp_->stream = 1;
	//kcp_->logmask = (IKCP_LOG_OUT_WINS << 1) - 1;
	return 0;
}


s32 KNetSession::destory()
{
	state_ = KNTS_INVALID;
	return reset();
}


s32 KNetSession::on_tick(s64 now_ms)
{
	return 0;
}



