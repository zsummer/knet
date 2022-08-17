
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
#ifndef _KNET_BASE_H_
#define _KNET_BASE_H_
#include "zallocator.h"
#include "zlist_ext.h"
#include "zarray.h"
#include "zlist.h"
#include "fn_log.h"
using namespace zsummer;


using s8 = char;
using u8 = unsigned char;
using s16 = short int;
using u16 = unsigned short int;
using s32 = int;
using u32 = unsigned int;
using s64 = long long;
using u64 = unsigned long long;
using f32 = float;
using f64 = double;


#include <stdint.h>
#include <string>
#include <map>
#include <algorithm>



#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "WinSock2.h"
#include "Ws2tcpip.h"
#include "Windows.h"
#pragma comment(lib, "ws2_32.lib") 

typedef int socklen_t;

#else // _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <openssl/aes.h>

typedef int SOCKET;
const int NO_ERROR = 0;
const int WSAECONNRESET = ECONNRESET;
const int WSAEWOULDBLOCK = EAGAIN;
const int SOCKET_ERROR = -1;

#ifndef INVALID_SOCKET
const int INVALID_SOCKET = -1;
#endif

#endif 

#ifndef ANSI_TO_TCHAR
#define ANSI_TO_TCHAR(x) (x)
#endif











#endif



