#include "knet_base.h"
#include "knet_socket.h"
#include "knet_turbo.h"
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
	ret = s2.init("0.0.0.0", s1.local().port(), "127.0.0.1", 8080);
	KNetAssert(ret != 0, "");
	s1.destroy();
	ret = s2.init("0.0.0.0", s1.local().port(), "127.0.0.1", 8080);
	KNetAssert(ret == 0, "");
	s2.destroy();
	KNetAssert(KNetEnv::error_count() == 0, "");
	return 0;
}



int main()
{
	FNLog::FastStartDebugLogger();
	LogInfo() << "start up";
	KNetAssert(test_socket_bind() == 0, "");
	KNetAssert(KNetEnv::error_count() == 0, "");
	KNetAssert(KNetEnv::user_count(KNTP_SKT_ALLOC) == KNetEnv::user_count(KNTP_SKT_FREE), "");
	KNetAssert(KNetEnv::user_count(KNTP_SES_ALLOC) == KNetEnv::user_count(KNTP_SES_FREE), "");

	return 0;
}

