
# KCP协议栈  

## 简介  
KCP是一个纯粹的ARQ协议 通过重传机制实现UDP数据包的可靠传输   
* 以比 TCP浪费10%-20%的带宽的代价 换取平均延迟降低 30%-40% 且最大延迟降低三倍的传输效果
* 纯算法实现 并不负责底层协议 (如UDP)的收发 
* 需要使用者自己定义下层数据包的发送方式 以 callback的方式提供给 KCP
* 连时钟都需要外部传递进来 内部不会有任何一次系统调用   

* 协议栈位置   
    | 协议栈位置  |
    | ----------- |
    | SESSION     |
    | KCP(ARG)    |
    | UDP(PACKET) |
    | IP          |
    | LINK        |
    | PHY         |



## 协议栈基础特性  

TCP是为流量设计的 (每秒内可以传输多少KB的数据) 讲究的是充分利用带宽 
KCP是为流速设计的 (单个数据包从一端发送到一端需要多少时间) 以10%-20%带宽浪费的代价换取了比 TCP快30%-40%的传输速度 

TCP信道是一条流速很慢 但每秒流量很大的大运河 
而KCP是水流湍急的小激流 KCP有正常模式和快速模式两种 通过以下策略达到提高流速的结果:  

* **RTO翻倍vs不翻倍:**  
  > TCP超时计算是RTOx2 这样连续丢三次包就变成RTOx8了 十分恐怖 而KCP启动快速模式后不x2 只是x1.5 (实验证明1.5这个值相对比较好) 提高了传输速度  

* **选择性重传 vs 全部重传:**  
  > TCP丢包时会全部重传从丢的那个包开始以后的数据 KCP是选择性重传 只重传真正丢失的数据包  (TCP同样有选择重传SACK 但有区别) 

* **快速重传:**  
  > 发送端发送了1 2 3 4 5几个包 然后收到远端的ACK: 1 3 4 5 当收到ACK3时 KCP知道2被跳过1次 收到ACK4时 知道2被跳过了2次 此时可以认为2号丢失 不用等超时 直接重传2号包 大大改善了丢包时的传输速度 
  > kcp实现是单独发送每个ACK信令 而TCP会合并  

* **延迟ACK vs 非延迟ACK:**  
  > TCP为了充分利用带宽 延迟发送ACK (NODELAY都没用) 这样超时计算会算出较大 RTT时间 延长了丢包时的判断过程 KCP的ACK是否延迟发送可以调节 

* **UNA vs ACK+UNA:**  
  > ARQ模型响应有两种 UNA (此编号前所有包已收到 如TCP)和ACK (该编号包已收到) 光用UNA将导致全部重传 光用ACK则丢失成本太高 以往协议都是二选其一 而 KCP协议中 除去单独的 ACK包外 所有包都有UNA信息 

* **非退让流控:**  
  > KCP正常模式同TCP一样使用公平退让法则 即发送窗口大小由:  发送缓存大小、接收端剩余接收缓存大小、丢包退让及慢启动这四要素决定 但传送及时性要求很高的小数据时 可选择通过配置跳过后两步 仅用前两项来控制发送频率 以牺牲部分公平性及带宽利用率之代价 换取了开着BT都能流畅传输的效果




## 基础流程  
单纯的ARQ在实际使用中并不能满足所有的网络场景 特别是网络拥塞时 大量的重传会导致更多的丢包   
增加FEC是一个明智的选择 在KCP协议中 也并不排斥在KCP上增加FEC  
但是需要注意的是 FEC加重传可能导致数据包的时延与抖动 同时 如果FEC解码得到的包经由重传或者网络延迟到达 需要在应用层进行检测 避免大量重复包影响KCP的传输效率  

流程图见rp文件  

KCP通过ikcp_create 创建一个KCP对象  
每个不同的会话将产生不同的对象  
因为KCP协议本身并没有提供网络部分的代码 所以需要将UDP发送函数的回调设置到KCP中 在有需要时 调用回调函数即可  

KCP也支持外部的内存分配与日志回调 为用户提供了非常充分的自由度    

整个KCP协议主要依靠一个循环ikcp_update来驱动整个算法的运转 所有的数据发送 接收 状态变化都依赖于此 所以如果有操作占用每一次update的周期过长 或者设置内部刷新的时间间隔过大 都会导致整个算法的效率降低 在ikcp_update中最终调用的是ikcp_flush 这是协议中的一个核心函数 将数据 确认包 以及窗口探测和应答发送到对端    

KCP使用 ikcp_send 发送数据 该函数调用ikcp_output发送数据 实际上最终调用事先注册的发送回调发送数据  
KCP通过 ikcp_recv 将数据接收出来 如果被分片发送 将在此自动重组 数据将与发送前保持一致   






## 数据结构      
基本结构如下:  
```
0               4   5   6       8 (BYTE)
+---------------+---+---+-------+
|     conv      |cmd|frg|  wnd  |
+---------------+---+---+-------+   8
|     ts        |     sn        |
+---------------+---------------+  16
|     una       |     len       |
+---------------+---------------+  24
|                               |
|        DATA (optional)        |
|                               |
+-------------------------------+
```   

code:   
``` C++ 
struct IKCPSEG
{
	struct IQUEUEHEAD node;
	IUINT32 conv;
	IUINT32 cmd;
	IUINT32 frg;
	IUINT32 wnd;
	IUINT32 ts;
	IUINT32 sn;
	IUINT32 una;
	IUINT32 len;
	IUINT32 resendts;
	IUINT32 rto;
	IUINT32 fastack;
	IUINT32 xmit;
	char data[1];
};
```


* Conv: 32bit 4Byte  
  > 为一个表示会话编号的整数 和TCP的 conv 一样 通信双方需保证 conv 相同 相互的数据包才能够被接受 
  > conv 唯一标识一个会话 但通信双方可以同时存在多个会话   

* cmd 8bit 1Byte   
   用来区分分片的作用 
  * IKCP_CMD_PUSH: 数据分片 
  * IKCP_CMD_ACK: ack分片  
  * IKCP_CMD_WASK: 请求告知窗口大小 
  * IKCP_CMD_WINS: 告知窗口大小 

* frag 8bit 1Byte   
  > 用户数据可能会被分成多个KCP包发送 frag标识segment分片ID（在message中的索引 由大到小 0表示最后一个分片） 

* wnd 16bit 2Byte   
  > 剩余接收窗口大小（接收窗口大小-接收队列大小） 发送方的发送窗口不能超过接收方给出的数值 

* ts 32bit 4Byte   
  > message发送时刻的时间戳

* sn 32bit 4Byte   
  > message分片segment的序号 按1累次递增 

* una 32bit 4Byte   
  > 待接收消息序号(接收滑动窗口左端) 对于未丢包的网络来说 una是下一个可接收的序号 如收到sn=10的包 una为11 

* len 32bit 4Byte   
  > 数据长度 


* resendts   
  > 下次超时重传的时间戳 

* rto  
  > 该分片的超时重传等待时间 其计算方法同TCP 

* fastack   
  > 收到ack时计算的该分片被跳过的累计次数 此字段用于快速重传 自定义需要几次确认开始快速重传 

* xmit   
  > 发送分片的次数 每发送一次加一 发送的次数对RTO的计算有影响 但是比TCP来说 影响会小一些 计算思想类似



## IKCPCB结构    
``` C++
struct IKCPCB
{
	IUINT32 conv, mtu, mss, state;
	IUINT32 snd_una, snd_nxt, rcv_nxt;
	IUINT32 ts_recent, ts_lastack, ssthresh;
	IINT32 rx_rttval, rx_srtt, rx_rto, rx_minrto;
	IUINT32 snd_wnd, rcv_wnd, rmt_wnd, cwnd, probe;
	IUINT32 current, interval, ts_flush, xmit;
	IUINT32 nrcv_buf, nsnd_buf;
	IUINT32 nrcv_que, nsnd_que;
	IUINT32 nodelay, updated;
	IUINT32 ts_probe, probe_wait;
	IUINT32 dead_link, incr;
	struct IQUEUEHEAD snd_queue;
	struct IQUEUEHEAD rcv_queue;
	struct IQUEUEHEAD snd_buf;
	struct IQUEUEHEAD rcv_buf;
	IUINT32 *acklist;
	IUINT32 ackcount;
	IUINT32 ackblock;
	void *user;
	char *buffer;
	int fastresend;
	int fastlimit;
	int nocwnd, stream;
	int logmask;
	int (*output)(const char *buf, int len, struct IKCPCB *kcp, void *user);
	void (*writelog)(const char *log, struct IKCPCB *kcp, void *user);
};
```  

IKCPCB是KCP中最重要的结构 也是在会话开始就创建的对象 代表着这次会话  

* conv: 标识这个会话 
* mtu: 最大传输单元 默认数据为1400 最小为50 
* mss: 最大分片大小 不大于mtu 
* state: 连接状态（0xFFFFFFFF表示断开连接） 
* snd_una: 第一个未确认的包 
* snd_nxt: 下一个待分配的包的序号 
* rcv_nxt: 待接收消息序号 为了保证包的顺序 接收方会维护一个接收窗口 接收窗口有一个起始序号rcv_nxt（待接收消息序号）以及尾序号 * rcv_nxt + rcv_wnd（接收窗口大小） 
* ssthresh: 拥塞窗口阈值 以包为单位（TCP以字节为单位） 
* rx_rttval: RTT的变化量 代表连接的抖动情况 
* rx_srtt: smoothed round trip time 平滑后的RTT 
* rx_rto: 由ACK接收延迟计算出来的重传超时时间 
* rx_minrto: 最小重传超时时间 
* snd_wnd: 发送窗口大小 
* rcv_wnd: 接收窗口大小 
* rmt_wnd: 远端接收窗口大小 
* cwnd: 拥塞窗口大小 
* probe: 探查变量 IKCP_ASK_TELL表示告知远端窗口大小 IKCP_ASK_SEND表示请求远端告知窗口大小 
* interval: 内部flush刷新间隔 对系统循环效率有非常重要影响 
* ts_flush: 下次flush刷新时间戳 
* xmit: 发送segment的次数 当segment的xmit增加时 xmit增加（第一次或重传除外） 
* rcv_buf: 接收消息的缓存 
* nrcv_buf: 接收缓存中消息数量 
* snd_buf: 发送消息的缓存 
* nsnd_buf: 发送缓存中消息数量 
* rcv_queue: 接收消息的队列
* nrcv_que: 接收队列中消息数量 
* snd_queue: 发送消息的队列 
* nsnd_que: 发送队列中消息数量 
* nodelay: 是否启动无延迟模式 无延迟模式rtomin将设置为0 拥塞控制不启动 
* updated: 是否调用过update函数的标识 
* ts_probe: 下次探查窗口的时间戳 
* probe_wait: 探查窗口需要等待的时间     
* dead_link: 最大重传次数 被认为连接中断 
* incr: 可发送的最大数据量 
* acklist: 待发送的ack列表 
* ackcount: acklist中ack的数量 每个ack在acklist中存储ts sn两个量 
* ackblock: 2的倍数 标识acklist最大可容纳的ack数量 
* user: 指针 可以任意放置代表用户的数据 也可以设置程序中需要传递的变量 
* buffer: 存储消息字节流 
* fastresend: 触发快速重传的重复ACK个数 
* nocwnd: 取消拥塞控制 
* stream: 是否采用流传输模式 
* logmask: 日志的类型 如IKCP_LOG_IN_DATA 方便调试 
* output udp: 发送消息的回调函数 
* writelog: 写日志的回调函数 



## 基本使用

1. 创建 KCP对象：

   ```cpp
   // 初始化 kcp对象 conv为一个表示会话编号的整数 和tcp的 conv一样 通信双
   // 方需保证 conv相同 相互的数据包才能够被认可 user是一个给回调函数的指针
   ikcpcb *kcp = ikcp_create(conv, user);
   ```

2. 设置回调函数：

   ```cpp
   // KCP的下层协议输出函数 KCP需要发送数据时会调用它
   // buf/len 表示缓存和长度
   // user指针为 kcp对象创建时传入的值 用于区别多个 KCP对象
   int udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
   {
     ....
   }
   // 设置回调函数
   kcp->output = udp_output;
   ```

3. 循环调用 update：

   ```cpp
   // 以一定频率调用 ikcp_update来更新 kcp状态 并且传入当前时钟（毫秒单位）
   // 如 10ms调用一次 或用 ikcp_check确定下次调用 update的时间不必每次调用
   ikcp_update(kcp, millisec);
   ```

4. 输入一个下层数据包：

   ```cpp
   // 收到一个下层数据包（比如UDP包）时需要调用：
   ikcp_input(kcp, received_udp_packet, received_udp_size);
   ```
   处理了下层协议的输出/输入后 KCP协议就可以正常工作了   
   使用 ikcp_send 来向远端发送数据    
   另一端使用 ikcp_recv(kcp, ptr, size)来接收数据   


## 协议配置

协议默认模式是一个标准的 ARQ 需要通过配置打开各项加速开关：

1. 工作模式：
   ```cpp
   int ikcp_nodelay(ikcpcb *kcp, int nodelay, int interval, int resend, int nc)
   ```

   - nodelay ：是否启用 nodelay模式 0不启用；1启用 
   - interval ：协议内部工作的 interval 单位毫秒 比如 10ms或者 20ms
   - resend ：快速重传模式 默认0关闭 可以设置2（2次ACK跨越将会直接重传）
   - nc ：是否关闭流控 默认是0代表不关闭 1代表关闭 
   - 普通模式： ikcp_nodelay(kcp, 0, 40, 0, 0);
   - 极速模式： ikcp_nodelay(kcp, 1, 10, 2, 1);

2. 最大窗口：
   ```cpp
   int ikcp_wndsize(ikcpcb *kcp, int sndwnd, int rcvwnd);
   ```
   该调用将会设置协议的最大发送窗口和最大接收窗口大小 默认为32. 这个可以理解为 TCP的 SND_BUF 和 RCV_BUF 只不过单位不一样 SND/RCV_BUF 单位是字节 这个单位是包 

3. 最大传输单元：

   纯算法协议并不负责探测 MTU 默认 mtu是1400字节 可以使用ikcp_setmtu来设置该值 该值将会影响数据包归并及分片时候的最大传输单元 

4. 最小RTO：

   不管是 TCP还是 KCP计算 RTO时都有最小 RTO的限制 即便计算出来RTO为40ms 由于默认的 RTO是100ms 协议只有在100ms后才能检测到丢包 快速模式下为30ms 可以手动更改该值：
   ```cpp
   kcp->rx_minrto = 10;
   ```



## 其他文档索引

协议的使用和配置都是很简单的 大部分情况看完上面的内容基本可以使用了 如果你需要进一步进行精细的控制 比如改变 KCP的内存分配器 或者你需要更有效的大规模调度 KCP链接（比如 3500个以上） 或者如何更好的同 TCP结合 那么可以继续延伸阅读：

- [Wiki Home](https://github.com/skywind3000/kcp/wiki)
- [KCP 最佳实践](https://github.com/skywind3000/kcp/wiki/KCP-Best-Practice)
- [同现有TCP服务器集成](https://github.com/skywind3000/kcp/wiki/Cooperate-With-Tcp-Server)
- [传输数据加密](https://github.com/skywind3000/kcp/wiki/Network-Encryption)
- [应用层流量控制](https://github.com/skywind3000/kcp/wiki/Flow-Control-for-Users)
- [性能评测](https://github.com/skywind3000/kcp/wiki/KCP-Benchmark)







# 避免缓存积累延迟

不管使用 TCP 还是 KCP，你都不可能超越信道限制的发送数据。TCP 的发送窗口 SNDBUF 决定了最多可以同时发送多少数据，KCP的也一样。

当前发送且没有得到 ACK/UNA确认的数据，都会滞留在发送缓存中，一旦滞留数据超过了发送窗口大小限制，则该链接的 tcp send 调用将会
被阻塞，或者返回：EAGAIN / EWOULDBLOCK，这时候说明当前 tcp 信道可用带宽已经赶不上你的发送速度了。

```
可用带宽 = min(本地可用发送窗口大小，远端可用接收窗口大小) * (1 - 丢包率) / RTT
```

当你持续调用 ikcp_send，首先会填满kcp的 snd_buf，如果 snd_buf 的大小超过发送窗口 snd_wnd 限制，则会停止向 snd_buf 里追加
数据包，只会放在 snd_queue 里面滞留着，等待 snd_buf 有新位置了（因为收到远端 ack/una而将历史包从 snd_buf中移除），才会从
snd_queue 转移到 snd_buf，等待发送。

TCP发送窗口满了不能发送了，会给你阻塞住或者 EAGAIN/EWOULDBLOCK；KCP发送窗口满了，ikcp_send 并不会给你返回 -1，而是让数据滞留
在 snd_queue 里等待有能力时再发送。

因此，千万不要以为 ikcp_send 可以无节制的调用，为什么 KCP在发送窗口满的时候不返回错误呢？这个问题当年设计时权衡过，如果返回希望发送时返回错误的 EAGAIN/EWOULDBLOCK 你势必外层还需要建立一个缓存，等到下次再测试是否可以 send。那么还不如 kcp直接把这一层缓存做了，让上层更简单些，而且具体要如何处理 EAGAIN，可以让上层通过检测 ikcp_waitsnd 函数来判断还有多少包没有发出去，灵活抉择是否向 snd_queue 缓存追加数据包还是其他。

## 重设窗口大小

要解决上面的问题首先对你的使用带宽有一个预计，并根据上面的公式重新设置发送窗口和接收窗口大小，你写后端，想追求tcp的性能，也会需要重新设置tcp的 sndbuf, rcvbuf 的大小，KCP 默认发送窗口和接收窗口大小都比较小而已（默认32个包），你可以朝着 64, 128, 256, 512, 1024 等档次往上调，kcptun默认发送窗口 1024，用来传高清视频已经足够，游戏的话，32-256 应该满足。

不设置的话，如果默认 snd_wnd 太小，网络不是那么顺畅，你越来越多的数据会滞留在 snd_queue里得不到发送，你的延迟会越来越大。

设定了 snd_wnd，远端的 rcv_wnd 也需要相应扩大，并且不小于发送端的 snd_wnd 大小，否则设置没意义。

其次对于成熟的后端业务，不管用 TCP还是 KCP，你都需要实现相关缓存控制策略：

## 缓存控制：传送文件

你用 tcp传文件的话，当网络没能力了，你的 send调用要不就是阻塞掉，要不就是 EAGAIN，然后需要通过 epoll 检查 EPOLL_OUT事件来决定下次什么时候可以继续发送。

KCP 也一样，如果 ikcp_waitsnd 超过阈值，比如2倍 snd_wnd，那么停止调用 ikcp_send，ikcp_waitsnd的值降下来，当然期间要保持 ikcp_update 调用。

## 缓存控制：实时视频直播

视频点播和传文件一样，而视频直播，一旦 ikcp_waitsnd 超过阈值了，除了不再往 kcp 里发送新的数据包，你的视频应该进入一个 “丢帧” 状态，直到 ikcp_waitsnd 降低到阈值的 1/2，这样你的视频才不会有积累延迟。

这和使用 TCP推流时碰到 EAGAIN 期间，要主动丢帧的逻辑时一样的。

同时，如果你能做的更好点，waitsnd 超过阈值了，代表一段时间内网络传输能力下降了，此时你应该动态降低视频质量，减少码率，等网络恢复了你再恢复。

## 缓存控制：游戏控制数据

大部分逻辑严密的 TCP游戏服务器，都是使用无阻塞的 tcp链接配套个 epoll之类的东西，当后端业务向用户发送数据时会追加到用户空间的一块发送缓存，比如 ring buffer 之类，当 epoll 到 EPOLL_OUT 事件时（其实也就是tcp发送缓存有空余了，不会EAGAIN/EWOULDBLOCK的时候），再把 ring buffer 里面暂存的数据使用 send 传递给系统的 SNDBUF，直到再次 EAGAIN。

那么 TCP SERVER的后端业务持续向客户端发送数据，而客户端又迟迟没能力接收怎么办呢？此时 epoll 会长期不返回 EPOLL_OUT事件，数据会堆积再该用户的 ring buffer 之中，如果堆积越来越多，ring buffer 会自增长的话就会把 server 的内存给耗尽。因此成熟的 tcp 游戏服务器的做法是：当客户端应用层发送缓存（非tcp的sndbuf）中待发送数据超过一定阈值，就断开 TCP链接，因为该用户没有接收能力了，无法持续接收游戏数据。

使用 KCP 发送游戏数据也一样，当 ikcp_waitsnd 返回值超过一定限度时，你应该断开远端链接，因为他们没有能力接收了。

但是需要注意的是，KCP的默认窗口都是32，比tcp的默认窗口低很多，实际使用时应提前调大窗口，但是为了公平性也不要无止尽放大（不要超过1024）。


## 总结

缓存积累这个问题，不管是 TCP还是 KCP你都要处理，因为TCP默认窗口比较大，因此可能很多人并没有处理的意识。

当你碰到缓存延迟时：

1. 检查 snd_wnd, rcv_wnd 的值是否满足你的要求，根据上面的公式换算，每秒钟要发多少包，当前 snd_wnd满足条件么？
2. 确认打开了 ikcp_nodelay，让各项加速特性得以运转，并确认 nc参数是否设置，以关闭默认的类 tcp保守流控方式。
3. 确认 ikcp_update 调用频率是否满足要求（比如10ms一次）。

如果你还想更激进：

1. 确认 minrto 是否设置，比如设置成 10ms, nodelay 只是设置成 30ms，更激进可以设置成 10ms 或者 5ms。
2. 确认 interval是否设置，可以更激进的设置成 5ms，让内部始终循环更快。
3. 每次发送完数据包后，手动调用 ikcp_flush
4. 降低 mtu 到 470，同样数据虽然会发更多的包，但是小包在路由层优先级更高。

如果你还想更快，可以在 KCP下层增加前向纠错协议。详细见：[协议分层](https://github.com/skywind3000/kcp/wiki/Network-Layer)，[最佳实践](https://github.com/skywind3000/kcp/wiki/KCP-Best-Practice)。

更多见讨论记录：

[https://github.com/skywind3000/kcp/issues/4](https://github.com/skywind3000/kcp/issues/4)

[https://github.com/skywind3000/kcp/issues/93](https://github.com/skywind3000/kcp/issues/93)

## 部分讨论记录: 

> 50%+75%的丢包率实在是太高了，信道几乎不可用了。因为高丢包，RTO会变的很大，这些行为和TCP也都是一致的，tcp在35%丢包时就断线了，没法工作了。其实处理“当前网络无法发送更多数据”这种事情，是上层传输逻辑的一个很重要的逻辑，不管下面是tcp还是kcp，再我们用kcp传送语音和视频，都会碰到网络震荡，这时候，语音或者视频一旦发现ikcp_waitsnd 的数据超过一个阀值就开始跳帧，不再传送新数据出去，直到网络恢复，或者超时不恢复就断线重连了。这是一个参考处理方法。还有一个参考处理方法就是给上层返回 EAGAIN，和TCP的方式一样，让用户去解决去。

> 当你需要发送“超过信道容量”的数据时，由易到烦，有三个处理方法。  

1. int ikcp_wndsize(ikcpcb *kcp, int sndwnd, int rcvwnd);
    > 扩大发送窗口和接收窗口，比如设置为64，相当于tcp的，SNDBUF, RCVBUF   

2. 连接管理层（即kcp的上一层管理连接，用于衔接用户和kcp的控制类），每次调用kcp_send前检查ikcp_waitsnd是否超过阀值，超过的话，不要调用kcp_send了，直接给用户 EAGAIN，和tcp行为保持一直。  
  
3. 数据传送层（处理kcp->output, 临近udp的那一层），发现丢包超过20%时，启动FEC，每发送三个包，紧跟一个冗余包（冗余包=P1 xor P2 xor P3），即3:1的冗余，如果丢包率上升，继续调整为2:1的冗余，发现一个包丢失的话，使用同组其他包xor后恢复出来，让传输层来负担一部分减少丢包率的任务。  
   
> 理论上来讲，方法3在大多数情况下很有效果，但是如果你真的达到了信道的物理带宽上限，那么增加冗余包只会进一步增加丢包率。方法1和3都是缓解，归根结底是需要处理“每秒待发数据超过信道容量”这个问题，不管下层是tcp还是kcp，这个问题都必须要仔细处理。


##  丢包率和FEC   

* 冗余量考虑: 
  平均所需:  
  $Sum[0.1^i, {i, 1, infinite}]$   
  $\displaystyle \sum^{infty}_{i = 1}{i^k}$     
  > i=0.1  sum=0.11111111   
  > i=0.35 sum=0.538462  
  > i=0.5  sum=1  
  > i=0.7  sum=2.33333   
  > i=0.9  sum=9   

  考虑到连续丢包概率 例如丢包率50%, 则至少需要平均1倍的包量

* 恢复能力考虑(延迟):  
  丢失1个包可以在下一个包恢复  * 2  
  丢失1个包可以在收到下N个包全部收到后恢复 * 2
  丢失1个包可以在任意后续两个包恢复 * 3



