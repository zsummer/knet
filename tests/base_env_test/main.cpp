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
	KNetAssert(KNetEnv::count(KNT_STT_SKT_ALLOC_COUNT) == KNetEnv::count(KNT_STT_SKT_FREE_COUNT), "");
	KNetAssert(KNetEnv::count(KNT_STT_SES_CREATE_COUNT) == KNetEnv::count(KNT_STT_SES_DESTROY_COUNT), "");

	return 0;
}

