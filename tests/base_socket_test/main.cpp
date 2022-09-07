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
	KNetAssert(s1.local().port() != 0, "");
	ret = s2.init("0.0.0.0", s1.local().port(), "127.0.0.1", 8080);
	KNetAssert(ret != 0, "");
	s1.destroy();
	ret = s2.init("0.0.0.0", s1.local().port(), "127.0.0.1", 8080);
	KNetAssert(ret == 0, "");
	s2.destroy();
	KNetAssert(KNetEnv::error_count() == 0, "");
	return 0;
}


s32 test_socket_snd_no_port()
{
	KNetSocket s1(1);
	s32 ret = s1.init("0.0.0.0", 0, "127.0.0.1", 18080);
	KNetAssert(ret == 0, "");
	KNetAssert(s1.local().port() != 0, "");

	ret = s1.send_packet("12345", 6, s1.remote());
	KNetAssert(ret == 0, "");
	LogDebug() << "s1 send 12345";


	s1.destroy();
	KNetAssert(KNetEnv::error_count() == 0, "");
	return 0;
}

s32 test_socket_rcv_no_data()
{
	KNetSocket s1(1);
	s32 ret = s1.init("0.0.0.0", 0, "127.0.0.1", 18080);
	KNetAssert(ret == 0, "");
	KNetAssert(s1.local().port() != 0, "");

	char buf[10] = { 0 };
	s32 len = 10;
	KNetAddress addr;
	ret = s1.recv_packet(buf, len, addr, 0);
	KNetAssert(ret == 0, "");
	KNetAssert(len == 0, "");

	s1.destroy();
	KNetAssert(KNetEnv::error_count() == 0, "");
	return 0;
}




s32 test_socket_snd_rcv()
{
	KNetSocket s1(1);
	KNetSocket s2(2);
	s32 ret = s1.init("0.0.0.0", 0, "127.0.0.1", 18080);
	KNetAssert(ret == 0, "");
	KNetAssert(s1.local().port() != 0, "");

	ret = s2.init("0.0.0.0", 18080, "127.0.0.1", s1.local().port());
	KNetAssert(ret == 0, "");
	KNetAssert(s2.local().port() != 0, "");

	ret = s1.send_packet("12345", 6, s1.remote());
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



class	SelectTest : public KNetSelect
{
public:
	void on_readable(KNetSocket&s, s64 now_ms)
	{
		s.user_data_++;
	}
};

s32 test_socket_snd_rcv_with_select()
{
	KNetSockets skts;
	skts.emplace_back(0);
	skts.emplace_back(1);
	KNetSocket& s1 = skts[0];
	KNetSocket& s2 = skts[1];

	s32 ret = s1.init("0.0.0.0", 0, "127.0.0.1", 18080);
	KNetAssert(ret == 0, "");
	KNetAssert(s1.local().port() != 0, "");

	ret = s2.init("0.0.0.0", 18080, "127.0.0.1", s1.local().port());
	KNetAssert(ret == 0, "");
	KNetAssert(s2.local().port() != 0, "");

	SelectTest st;
	ret = st.do_select(skts, 0);
	KNetAssert(ret == 0, "");
	KNetAssert(s1.user_data_ == 0, "");
	KNetAssert(s2.user_data_ == 0, "");

	ret = s1.send_packet("12345", 6, s1.remote());
	KNetAssert(ret == 0, "");
	LogDebug() << "s1 send 12345";

	ret = st.do_select(skts, 0);
	KNetAssert(ret == 0, "");
	KNetAssert(s1.user_data_ == 0, "");
	KNetAssert(s2.user_data_ == 1, "");

	char buf[10] = { 0 };
	s32 len = 10;
	KNetAddress addr;
	ret = s2.recv_packet(buf, len, addr, 0);
	KNetAssert(ret == 0, "");
	KNetAssert(len == 6, "");
	LogDebug() << "s2 recv :" << buf;
	s2.send_packet("12345", 6, addr);
	KNetAssert(ret == 0, "");
	ret = st.do_select(skts, 0);
	KNetAssert(ret == 0, "");
	KNetAssert(s1.user_data_ == 1, "");
	KNetAssert(s2.user_data_ == 1, "");


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


s32 test_socket_select()
{
	KNetSockets skts;
	s32 ret = 0;
	SelectTest st;
	if (true)
	{
		s64 now = KNetEnv::now_ms();
		ret = st.do_select(skts, 0);
		KNetAssert(abs(now - KNetEnv::now_ms()) < 1, "");
		KNetAssert(ret == 0, "");
		KNetAssert(KNetEnv::error_count() == 0, "");
	}

	if (true)
	{
		s64 now = KNetEnv::now_ms();
		ret = st.do_select(skts, 100);
		KNetAssert(abs(now - KNetEnv::now_ms()) < 1000, "");
		KNetAssert(abs(now - KNetEnv::now_ms()) > 50, "");
		KNetAssert(ret == 0, "");
		KNetAssert(KNetEnv::error_count() == 0, "");
	}

	skts.emplace_back(0);
	skts.emplace_back(1);
	KNetSocket& s1 = skts[0];
	KNetSocket& s2 = skts[1];

	ret = s1.init("0.0.0.0", 0, "127.0.0.1", 18080);
	KNetAssert(ret == 0, "");
	KNetAssert(s1.local().port() != 0, "");

	ret = s2.init("0.0.0.0", 18080, "127.0.0.1", s1.local().port());
	KNetAssert(ret == 0, "");
	KNetAssert(s2.local().port() != 0, "");
	if (true)
	{
		s64 now = KNetEnv::now_ms();
		ret = st.do_select(skts, 0);
		KNetAssert(abs(now - KNetEnv::now_ms()) < 1, "");
		KNetAssert(ret == 0, "");
		KNetAssert(KNetEnv::error_count() == 0, "");
	}

	if (true)
	{
		s64 now = KNetEnv::now_ms();
		ret = st.do_select(skts, 100);
		KNetAssert(abs(now - KNetEnv::now_ms()) < 1000, "");
		KNetAssert(abs(now - KNetEnv::now_ms()) > 50, "");
		KNetAssert(ret == 0, "");
		KNetAssert(KNetEnv::error_count() == 0, "");
	}

	s1.destroy();
	s2.destroy();
	KNetAssert(KNetEnv::error_count() == 0, "");

	return 0;
}



int main()
{
	FNLog::FastStartDebugLogger();
	LogInfo() << "start up";
	KNetAssert(test_socket_bind() == 0, "");
	KNetAssert(test_socket_snd_no_port() == 0, "");
	KNetAssert(test_socket_rcv_no_data() == 0, "");
	KNetAssert(test_socket_snd_rcv() == 0, "");
	KNetAssert(test_socket_select() == 0, "");
	KNetAssert(test_socket_snd_rcv_with_select() == 0, "");



	KNetAssert(KNetEnv::error_count() == 0, "");
	KNetAssert(KNetEnv::user_count(KNTP_SKT_ALLOC) == KNetEnv::user_count(KNTP_SKT_FREE), "");
	KNetAssert(KNetEnv::user_count(KNTP_SES_ALLOC) == KNetEnv::user_count(KNTP_SES_FREE), "");


	LogInfo() << "all success ";
	return 0;
}

