#include "knet_base.h"
#include "knet_socket.h"

KNetEnv env_init_;


#define KNetAssert(x, desc)  \
if (!(x))  \
{   \
	LogError() << desc << "has error.";  \
	return -1;\
}


s32 test_socket_bind()
{
	KNetSocket s1;
	KNetSocket s2;
	s32 ret = s1.InitSocket("0.0.0.0", 0, "127.0.0.1", 8080);
	KNetAssert(ret == 0, "");
	ret = s2.InitSocket("0.0.0.0", s1.local().port(), "127.0.0.1", 8080);
	KNetAssert(ret != 0, "");
	s1.DestroySocket();
	ret = s2.InitSocket("0.0.0.0", s1.local().port(), "127.0.0.1", 8080);
	KNetAssert(ret == 0, "");
	s2.DestroySocket();
	return 0;
}


int main()
{
	FNLog::FastStartDebugLogger();
	LogInfo() << "start up";
	KNetAssert(test_socket_bind() == 0, "");

	KNetSocket s;
	s32 ret = s.InitSocket("0.0.0.0", 0, "127.0.0.1", 8080);
	if (ret != 0)
	{
		LogError() << "init error:" << ret;
		return -1;
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(10000000));

	return 0;
}

