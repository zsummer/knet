

//#define KNET_MAX_SOCKETS 1000
#include "knet_base.h"
#include "knet_socket.h"
#include "knet_controller.h"
#include <memory>
KNetEnv env_init_;


#define KNetAssert(x, desc)  \
if (!(x))  \
{   \
	LogError() << desc << "has error.";  \
	return -1;\
}


s32 test_session_connect_mix(s32 session_count, bool double_stream, s32 send_times, s32 interval, s32 data_len)
{
	if (send_times == 0)
	{
		send_times = 1;
	}
	KNetEnv::clean_count();
	KNetEnv::error_count() = 0;

	KNetConfigs mc;
	mc.emplace_back(KNetConfig{ "127.0.0.1", 19870, "", 0 });
	mc.emplace_back(KNetConfig{ "127.0.0.2", 19870, "", 0 });

	std::shared_ptr< KNetController> turbos[10];
	for (s32 i = 0; i < 10; i++)
	{
		turbos[i] = std::make_shared<KNetController>();
	}

	KNetController& server_c = *turbos[0];

	s32 ret = server_c.start_server(mc);
	KNetAssert(ret == 0, "");

	mc.clear();
	mc.emplace_back(KNetConfig{ "127.0.0.1", 0,"127.0.0.1", 19870 });
	if (double_stream)
	{
		mc.emplace_back(KNetConfig{ "127.0.0.2", 0, "127.0.0.2", 19870 });
	}


	KNetOnConnect on_connect = [&](KNetController& c, KNetSession& session, bool connected, u16 state, s64 time_out)
	{
		if (connected)
		{
			LogDebug() << "connected";
		}
		else
		{
			LogError() << "connect error: last state:" << state << ", time_out:" << time_out;
			return;
		}
		static char buf[KNT_KCP_DATA_SIZE];
		for (s32 i = 0; i < KNT_KCP_DATA_SIZE; i++)
		{
			buf[i] = 'a';
		}
		for (s32 i = 0; i < send_times; i++)
		{
			c.send_data(session, 0, buf, data_len, KNetEnv::now_ms());
		}

	};


	KNetOnData on_data = [&](KNetController& c, KNetSession& s, u8 chl, const char* data, s32 len, s64 now_ms)
	{
		c.send_data(s, 0, data, len, KNetEnv::now_ms());
	};

	KNetOnDisconnect on_disconnect = [&](KNetController& c, KNetSession& session, bool passive)
	{
		LogDebug() << "close session:" << session.session_id_ << ", is server:" << session.is_server() << ", is passive:" << passive;
	};


	for (s32 i = 0; i < 10; i++)
	{
		turbos[i]->set_on_data(on_data);
		turbos[i]->set_on_disconnect(on_disconnect);
	}

	for (s32 i = 0; i < session_count; i++)
	{
		KNetSession* session = NULL;
		std::shared_ptr< KNetController> pc = turbos[i%10];
		KNetController& c = *pc;
		ret = c.create_connect(mc, session);
		if (ret != 0)
		{
			//volatile int aa = 0;
		}
		KNetAssert(ret == 0, "");
		KNetAssert(session != NULL, "");

		ret = c.start_connect(*session, on_connect, 5000);
		if (ret != 0)
		{
			//volatile int aa = 0;
		}
		KNetAssert(ret == 0, "");
		KNetAssert(KNetEnv::error_count() == 0, "");
		for (s32 i = 0; i < 10; i++)
		{
			for (s32 j = 0; j < 10; j++)
			{
				ret = turbos[i]->do_tick();
				KNetAssert(ret == 0, "");
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(interval));
		if (i%(session_count/10) == 0)
		{
			LogInfo() << "try connect:" << i;
			for (s32 i = 0; i < 10; i++)
			{
				LogInfo() << "turbo[" << i << "] connected session:" << turbos[i]->get_session_count_by_state(KNTS_ESTABLISHED);
			}
		}
	}


	s32 loop_count = 10 * 1000 / interval;

	s64 now = KNetEnv::now_ms();
	for (s32 i = 0; i < loop_count; i++)
	{
		for (s32 id = 0; id < 10; id++)
		{
			ret = turbos[id]->do_tick();
			KNetAssert(ret == 0, "");
		}
		if (i % (loop_count / 10) == 0)
		{
			LogInfo() << "now loop:" << i << "/" << loop_count;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(interval));
	}
	s64 finish_now = KNetEnv::now_ms();
	LogInfo() << "";
	LogInfo() << "==========================================================";
	LogInfo() << "session_count:" << session_count << ", double_stream:" << double_stream << ", interval:" << interval;

	LogInfo() << "KNT_STT_SKT_SND_COUNT:" << KNetEnv::count(KNT_STT_SKT_SND_COUNT);
	LogInfo() << "KNT_STT_SKT_RCV_COUNT:" << KNetEnv::count(KNT_STT_SKT_RCV_COUNT);
	LogInfo() << "KNT_STT_SKT_SND_BYTES:" << KNetEnv::count(KNT_STT_SKT_SND_BYTES);
	LogInfo() << "KNT_STT_SKT_RCV_BYTES:" << KNetEnv::count(KNT_STT_SKT_RCV_BYTES);


	LogInfo() << "AVG SECOND KNT_STT_SKT_SND_COUNT:" << KNetEnv::count(KNT_STT_SKT_SND_COUNT) / ((finish_now - now) / 1000);
	LogInfo() << "AVG SECOND KNT_STT_SKT_RCV_COUNT:" << KNetEnv::count(KNT_STT_SKT_RCV_COUNT) / ((finish_now - now) / 1000);
	LogInfo() << "AVG SECOND KNT_STT_SKT_SND_BYTES:" << KNetEnv::count(KNT_STT_SKT_SND_BYTES) / ((finish_now - now) / 1000);
	LogInfo() << "AVG SECOND KNT_STT_SKT_RCV_BYTES:" << KNetEnv::count(KNT_STT_SKT_RCV_BYTES) / ((finish_now - now) / 1000);
	for (s32 i = 0; i < 10; i++)
	{
		LogInfo() << "turbo[" << i << "] established session:" << turbos[i]->get_session_count_by_state(KNTS_ESTABLISHED);
		LogInfo() << "turbo[" << i << "] established socket:" << turbos[i]->get_socket_count_by_state(KNTS_ESTABLISHED);
	}
	KNetEnv::print_count();

	for (s32 i = 0; i < 10; i++)
	{
		ret = turbos[i]->stop();
		KNetAssert(ret == 0, "");
	}

	for (s32 i = 0; i < 10; i++)
	{
		auto& controller = *turbos[i];

		for (auto& s : controller.sessions())
		{
			KNetAssert(s.state_ == KNTS_INVALID, "rst session ");
		}
		for (auto& s : controller.nss())
		{
			KNetAssert(s.state_ == KNTS_INVALID, "rst socket ");
		}
	}


	LogInfo() << "finish.";
	KNetAssert(KNetEnv::error_count() == 0, "");
	KNetAssert(KNetEnv::count(KNT_STT_SKT_ALLOC_COUNT) == KNetEnv::count(KNT_STT_SKT_FREE_COUNT), "");
	KNetAssert(KNetEnv::count(KNT_STT_SES_CREATE_COUNT) == KNetEnv::count(KNT_STT_SES_DESTROY_COUNT), "");
	return 0;
}


int main()
{
	FNLog::FastStartDebugLogger();
	FNLog::BatchSetChannelConfig(FNLog::GetDefaultLogger(), FNLog::CHANNEL_CFG_PRIORITY, FNLog::PRIORITY_INFO);
	LogInfo() << "start up";

	KNetAssert(test_session_connect_mix(400, true, 1, 10, 200) == 0, "");


	return 0;
}

