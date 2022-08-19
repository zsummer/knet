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
	s32 ret = s1.InitSocket("0.0.0.0", 0, "127.0.0.1", 8080);
	KNetAssert(ret == 0, "");
	ret = s2.InitSocket("0.0.0.0", s1.local_.port(), "127.0.0.1", 8080);
	KNetAssert(ret != 0, "");
	s1.DestroySocket();
	ret = s2.InitSocket("0.0.0.0", s1.local_.port(), "127.0.0.1", 8080);
	KNetAssert(ret == 0, "");
	s2.DestroySocket();
	KNetAssert(KNetEnv::Errors() == 0, "");
	return 0;
}

s32 test_socket_bind2()
{
	KNetConfigs mc;
	mc.emplace_back(KNetConfig{ "127.0.0.1", 19870, "", 0});
	mc.emplace_back(KNetConfig{ "127.0.0.2", 19870, "", 0 });
	KNetController controller;
	s32 ret = controller.StartServer(mc);
	KNetAssert(ret == 0, "");

	KNetSocket s1(1);
	KNetSocket s2(2);
	ret = s1.InitSocket("127.0.0.1", 19870, "127.0.0.1", 8080);
	KNetAssert(ret != 0, "");
	ret = s2.InitSocket("127.0.0.2", 19870, "127.0.0.1", 8080);
	KNetAssert(ret != 0, "");

	controller.Destroy();
	ret = s1.InitSocket("127.0.0.1", 19870, "127.0.0.1", 8080);
	KNetAssert(ret == 0, "");
	ret = s2.InitSocket("127.0.0.2", 19870, "127.0.0.1", 8080);
	KNetAssert(ret == 0, "");
	s1.DestroySocket();
	s2.DestroySocket();
	KNetAssert(KNetEnv::Errors() == 0, "");
	return 0;
}

int main()
{
	FNLog::FastStartDebugLogger();
	LogInfo() << "start up";
	KNetAssert(test_socket_bind() == 0, "");
	KNetAssert(test_socket_bind2() == 0, "");

	KNetEnv::CleanStatus();
	KNetEnv::Errors() = 0;


	KNetConfigs mc;
	mc.emplace_back(KNetConfig{ "127.0.0.1", 19870, "", 0 });
	mc.emplace_back(KNetConfig{ "127.0.0.2", 19870, "", 0 });
	KNetController controller;
	s32 ret = controller.StartServer(mc);
	KNetAssert(ret == 0, "");

	mc.clear();
	mc.emplace_back(KNetConfig{ "127.0.0.1", 0,"127.0.0.1", 19870 });
	mc.emplace_back(KNetConfig{ "127.0.0.2", 0, "127.0.0.2", 19870 });

	ret = controller.StartConnect("ddddddddddd", mc);
	KNetAssert(ret == 0, "");

	for (size_t i = 0; i < 100; i++)
	{
		ret = controller.DoSelect();
		if (ret != 0)
		{
			LogError() << "select error";
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	controller.Destroy();
	for (size_t i = 0; i < 10; i++)
	{
		ret = controller.DoSelect();
		if (ret != 0)
		{
			LogError() << "select error";
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	LogInfo() << "finish.";
	KNetAssert(KNetEnv::Errors() == 0, "");
	return 0;
}

