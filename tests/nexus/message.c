//> use: core nexus thread

#include <core/core.h>
#include <nexus/nexus.h>
#include <test.h>
#include <thread/thread.h>
#include <unistd.h>

typedef struct {
    char       url[64];
    Net_Result bind_result;
    Net_Result recv_result;
    Net_Result send_result;
    string     received_string;
    u16        received_u16;
    u32        received_u32;
    u64        received_u64;
    u8         received_u8;
    u8         received_tail[16];
    usize      received_tail_len;
} MessageServerArgs;

internal void _nexus_message_wait_for_server_start(void)
{
    thread_sleep_ms(50);
}

internal void _nexus_make_message_test_url(char* out_url, usize out_url_size)
{
    static u16 next_port = 18180;

    next_port++;
    snprintf(out_url,
             out_url_size,
             "tcp://127.0.0.1:%u",
             (unsigned)(next_port + (u16)(getpid() % 1000) * 10));
}

internal void* _nexus_message_server_round_trip(void* arg)
{
    MessageServerArgs* args = arg;

    Net_Socket sock         = net_socket();
    args->bind_result       = net_bind(&sock, args->url);
    if (NET_FAILED(args->bind_result)) {
        return NULL;
    }

    Net_Message msg   = net_message_create(&sock);
    args->recv_result = net_recv_msg(&msg);
    if (NET_FAILED(args->recv_result)) {
        net_message_done(&msg);
        net_close(&sock);
        return NULL;
    }

    TEST_ASSERT(net_message_read_string(&msg, &args->received_string));
    TEST_ASSERT(net_message_read_u16(&msg, &args->received_u16));
    TEST_ASSERT(net_message_read_u32(&msg, &args->received_u32));
    TEST_ASSERT(net_message_read_u64(&msg, &args->received_u64));
    TEST_ASSERT(net_message_read_u8(&msg, &args->received_u8));

    args->received_tail_len = msg.length;
    if (msg.length > 0) {
        TEST_ASSERT(net_message_read(
            &msg, args->received_tail, args->received_tail_len));
    }

    net_message_clear(&msg);
    net_message_append_string(&msg, S("reply"));
    net_message_append_u32(&msg, 0x11223344u);
    net_message_append(&msg, "ok", 2);
    args->send_result = net_send_msg(&msg);

    net_message_done(&msg);
    net_close(&sock);
    return NULL;
}

TEST_CASE(nexus, message_append_and_read_primitives)
{
    Net_Message msg = net_message_create(NULL);

    net_message_append_u8(&msg, 0x11);
    net_message_append_string(&msg, S("hello"));
    net_message_append_u16(&msg, 0x2233);
    net_message_append_u32(&msg, 0x44556677u);
    net_message_append_u64(&msg, 0x8899aabbccddeeffull);
    net_message_append(&msg, "xyz", 3);

    TEST_ASSERT_EQ(msg.length, (usize)(1 + 4 + 5 + 2 + 4 + 8 + 3));

    u8     v8 = 0;
    string text;
    u16    v16 = 0;
    u32    v32 = 0;
    u64    v64 = 0;
    u8     tail[3];

    TEST_ASSERT(net_message_read_u8(&msg, &v8));
    TEST_ASSERT(net_message_read_string(&msg, &text));
    TEST_ASSERT(net_message_read_u16(&msg, &v16));
    TEST_ASSERT(net_message_read_u32(&msg, &v32));
    TEST_ASSERT(net_message_read_u64(&msg, &v64));
    TEST_ASSERT(net_message_read(&msg, tail, sizeof(tail)));

    TEST_ASSERT_EQ(v8, 0x11);
    TEST_ASSERT_EQ(text.count, (usize)5);
    TEST_ASSERT_MEM_EQ(text.data, "hello", 5);
    TEST_ASSERT_EQ(v16, 0x2233);
    TEST_ASSERT_EQ(v32, 0x44556677u);
    TEST_ASSERT_EQ(v64, 0x8899aabbccddeeffull);
    TEST_ASSERT_MEM_EQ(tail, "xyz", sizeof(tail));
    TEST_ASSERT_EQ(msg.length, 0);

    net_message_done(&msg);
}

TEST_CASE(nexus, recv_msg_and_send_msg_round_trip)
{
    MessageServerArgs args = {0};
    _nexus_make_message_test_url(args.url, sizeof(args.url));

    Thread server_thread;
    TEST_ASSERT(
        thread_create(&server_thread, _nexus_message_server_round_trip, &args));

    _nexus_message_wait_for_server_start();

    Net_Socket client_sock = net_socket();
    TEST_ASSERT_EQ(net_connect(&client_sock, args.url), NET_OK);

    Net_Message outbound = net_message_create(&client_sock);
    net_message_append_string(&outbound, S("request"));
    net_message_append_u16(&outbound, 0x1234);
    net_message_append_u32(&outbound, 0x55667788u);
    net_message_append_u64(&outbound, 0x0102030405060708ull);
    net_message_append_u8(&outbound, 0x99);
    net_message_append(&outbound, "tail", 4);
    TEST_ASSERT_EQ(net_send_msg(&outbound), NET_OK);

    Net_Message inbound = net_message_create(&client_sock);
    TEST_ASSERT_EQ(net_recv_msg(&inbound), NET_OK);

    string reply_text;
    u32    reply_code = 0;
    u8     reply_tail[2];
    TEST_ASSERT(net_message_read_string(&inbound, &reply_text));
    TEST_ASSERT(net_message_read_u32(&inbound, &reply_code));
    TEST_ASSERT(net_message_read(&inbound, reply_tail, sizeof(reply_tail)));

    thread_join(&server_thread);

    TEST_ASSERT_EQ(args.bind_result, NET_OK);
    TEST_ASSERT_EQ(args.recv_result, NET_OK);
    TEST_ASSERT_EQ(args.send_result, NET_OK);
    TEST_ASSERT_EQ(args.received_string.count, (usize)7);
    TEST_ASSERT_MEM_EQ(args.received_string.data, "request", 7);
    TEST_ASSERT_EQ(args.received_u16, 0x1234);
    TEST_ASSERT_EQ(args.received_u32, 0x55667788u);
    TEST_ASSERT_EQ(args.received_u64, 0x0102030405060708ull);
    TEST_ASSERT_EQ(args.received_u8, 0x99);
    TEST_ASSERT_EQ(args.received_tail_len, (usize)4);
    TEST_ASSERT_MEM_EQ(args.received_tail, "tail", 4);
    TEST_ASSERT_EQ(reply_text.count, (usize)5);
    TEST_ASSERT_MEM_EQ(reply_text.data, "reply", 5);
    TEST_ASSERT_EQ(reply_code, 0x11223344u);
    TEST_ASSERT_MEM_EQ(reply_tail, "ok", sizeof(reply_tail));

    net_message_done(&outbound);
    net_message_done(&inbound);
    net_close(&client_sock);
}
