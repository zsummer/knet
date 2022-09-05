#include "knet_base.h"
#include "knet_socket.h"
#include "knet_controller.h"
KNetEnv env_init_;


#define KNetAssert(x, desc)  \
if (!(x))  \
{   \
	LogError() << desc << "has error.";  \
	return -1;\
}


s32 test_socket_bind()
{
	KNetSocket s1(1);
	KNetSocket s2(2);
	s32 ret = s1.init("0.0.0.0", 0, "127.0.0.1", 8080);
	KNetAssert(ret == 0, "");
	KNetAssert(s1.local_.port() != 0, "");
	ret = s2.init("0.0.0.0", s1.local_.port(), "127.0.0.1", 8080);
	KNetAssert(ret != 0, "");
	s1.destroy();
	ret = s2.init("0.0.0.0", s1.local_.port(), "127.0.0.1", 8080);
	KNetAssert(ret == 0, "");
	s2.destroy();
	KNetAssert(KNetEnv::error_count() == 0, "");
	return 0;
}

s32 test_socket_snd_rcv()
{
	KNetSocket s1(1);
	KNetSocket s2(2);
	s32 ret = s1.init("0.0.0.0", 0, "127.0.0.1", 18080);
	KNetAssert(ret == 0, "");
	KNetAssert(s1.local_.port() != 0, "");

	ret = s2.init("0.0.0.0", 18080, "127.0.0.1", s1.local_.port());
	KNetAssert(ret == 0, "");
	KNetAssert(s2.local_.port() != 0, "");

	ret = s1.send_packet("12345", 6, s1.remote_);
	KNetAssert(ret == 0, "");
	LogDebug() << "s1 send 12345";

	char buf[10] = { 0 };
	s32 len = 10;
	KNetAddress addr;
	ret = s2.recv_packet(buf, len, addr, 0);
	KNetAssert(ret == 0, "");
	KNetAssert(len == 6, "");
	LogDebug() << "s2 recv :" << buf;
	s2.send_packet("12345", 6, addr);
	KNetAssert(ret == 0, "");

	len = 10;
	LogDebug() << "s2 snd back 12345";
	ret = s1.recv_packet(buf, len, addr, 0);
	KNetAssert(ret == 0, "");
	KNetAssert(len == 6, "");
	LogDebug() << "s1 recv echo :" << buf;


	s1.destroy();
	s2.destroy();
	KNetAssert(KNetEnv::error_count() == 0, "");
	return 0;
}



s32 test_socket_snd_no_port()
{
	KNetSocket s1(1);
	s32 ret = s1.init("0.0.0.0", 0, "127.0.0.1", 18080);
	KNetAssert(ret == 0, "");
	KNetAssert(s1.local_.port() != 0, "");

	ret = s1.send_packet("12345", 6, s1.remote_);
	KNetAssert(ret == 0, "");
	LogDebug() << "s1 send 12345";


	s1.destroy();
	KNetAssert(KNetEnv::error_count() == 0, "");
	return 0;
}


int main()
{
	FNLog::FastStartDebugLogger();
	LogInfo() << "start up";
	KNetAssert(test_socket_bind() == 0, "");
	KNetAssert(test_socket_snd_rcv() == 0, "");
	KNetAssert(test_socket_snd_no_port() == 0, "");


	KNetAssert(KNetEnv::error_count() == 0, "");
	KNetAssert(KNetEnv::count(KNT_STT_SKT_ALLOC_COUNT) == KNetEnv::count(KNT_STT_SKT_FREE_COUNT), "");
	KNetAssert(KNetEnv::count(KNT_STT_SES_CREATE_COUNT) == KNetEnv::count(KNT_STT_SES_DESTROY_COUNT), "");

	return 0;
}

