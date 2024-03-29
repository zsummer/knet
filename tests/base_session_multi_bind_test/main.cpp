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

KNetTurbo turbo;

s32 test_session_bind()
{
	KNetConfigs mc;
	mc.emplace_back(KNetConfig{ "127.0.0.1", 19870, "", 0 ,0, 0 });
	mc.emplace_back(KNetConfig{ "127.0.0.2", 19870, "", 0  ,0, 0 });
	
	s32 ret = turbo.start_server(mc);
	KNetAssert(ret == 0, "");

	KNetSocket s1(1);
	KNetSocket s2(2);
	ret = s1.init("127.0.0.1", 19870, "127.0.0.1", 8080);
	KNetAssert(ret != 0, "");
	ret = s2.init("127.0.0.2", 19870, "127.0.0.1", 8080);
	KNetAssert(ret != 0, "");

	turbo.stop();
	ret = s1.init("127.0.0.1", 19870, "127.0.0.1", 8080);
	KNetAssert(ret == 0, "");
	ret = s2.init("127.0.0.2", 19870, "127.0.0.1", 8080);
	KNetAssert(ret == 0, "");
	s1.destroy();
	s2.destroy();
	KNetAssert(KNetEnv::error_count() == 0, "");
	return 0;
}

int main()
{
	FNLog::FastStartDebugLogger();
	LogInfo() << "start up";
	KNetAssert(test_session_bind() == 0, "");
	return 0;
}

