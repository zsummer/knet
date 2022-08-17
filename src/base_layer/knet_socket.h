
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
#ifndef _KNET_SOCKET_H_
#define _KNET_SOCKET_H_

#include "knet_base.h"
#include <chrono>

#include "knet_address.h"
#include "knet_env.h"


enum KNTS_STATE: u32
{
    KNTS_INVALID = 0,
    KNTS_LOCAL_INITED,
    KNTS_BINDED,
    KNTS_CONNECTED,
    KNTS_HANDSHAKE_CH,
    KNTS_HANDSHAKE_SH,
    KNTS_ESTABLISHED,
    KNTS_LINGER,
};

class KNetSocket
{
public:
    KNetSocket()
    {
        socket_ = INVALID_SOCKET;
        state_ = KNTS_INVALID;
        ref_count_ = 0;
    }
    ~KNetSocket()
    {
        
    }
    
    s32 InitSocket(const char* localhost, u16 localport, const char* remote_ip, u16 remote_port)
    {
        if (state_ != KNTS_INVALID)
        {
            return -1;
        }
        s32 ret = local_.reset(localhost, localport);
        if (ret != 0)
        {
            return -2;
        }
        if (remote_ip != NULL)
        {
            ret = remote_.reset(remote_ip, remote_port);
            if (ret != 0)
            {
                return -3;
            }
        }


        socket_ = socket(local_.family(), SOCK_DGRAM, 0);
        if (socket_ == INVALID_SOCKET)
        {
            return -4;
        }
        state_ = KNTS_LOCAL_INITED;
        ret = bind(socket_, local_.sockaddr_ptr(), local_.sockaddr_len());
        if (ret != 0)
        {
            DestroySocket();
            return -5;
        }
        
        local_.reset_from_socket(socket_);
        state_ = KNTS_BINDED;
        LogInfo() << "bind local:" << local_.debug_string();
        return 0;
    }

    /* // 
    s32 Connect()
    {
        if (state_ != KNTS_BINDED)
        {
            return -1;
        }
        s32 ret = connect(socket_, remote_.sockaddr_ptr(), remote_.sockaddr_len());
        if (ret != 0)
        {
            return -2;
        }
        state_ = KNTS_CONNECTED;
        return 0;
    }
    */


    s32 SendTo()
    {
        sendto(socket_, "ssss", 4, 0, remote_.sockaddr_ptr(), remote_.sockaddr_len());
        return 0;
    }



    s32 DestroySocket()
    {
        if (state_ == KNTS_INVALID)
        {
            return -1;
        }
        if (socket_ != INVALID_SOCKET)
        {
#ifdef _WIN32
            closesocket(socket_);
#else
            close(socket_);
#endif
            socket_ = INVALID_SOCKET;
        }
        state_ = KNTS_INVALID;
        return 0;
    }

    
public:
    KNTS_STATE state() { return state_; }
    KNTS_STATE set_state(KNTS_STATE s) { state_ = s; return state_; }
    SOCKET skt() { return socket_; }
    KNetAddress& local() { return local_; }
    KNetAddress& remote() { return remote_; }
    s32 ref_count()const { return ref_count_; }
    s32 & ref_count() { return ref_count_; }
private:
    s32 ref_count_;
    KNTS_STATE state_;
    SOCKET socket_;
    KNetAddress local_;
    KNetAddress remote_;
};






#endif   
