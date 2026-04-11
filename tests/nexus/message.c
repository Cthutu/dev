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

typedef struct {
    char       url[64];
    Net_Result bind_result;
    Net_Result recv_result;
    Net_Result send_result;
    string     received_string;
} UdpMessageServerArgs;

typedef struct {
    char       url[64];
    string     request_text;
    u32        request_id;
    Net_Result connect_result;
    Net_Result send_result;
    Net_Result recv_result;
    char       reply_text_storage[32];
    usize      reply_text_len;
    u32        reply_id;
} MultiClientArgs;

typedef struct {
    char       url[64];
    Net_Result bind_result;
    Net_Result recv_results[2];
    Net_Result send_results[2];
    string     received_texts[2];
    u32        received_ids[2];
} MultiClientServerArgs;

typedef struct {
    char       url[64];
    u32        bind_delay_ms;
    Net_Result bind_result;
    Net_Result recv_result;
    string     received_string;
} DelayedBindServerArgs;

typedef struct {
    char       url[64];
    u32        recv_timeout_ms;
    u32        client_delay_ms;
    u32        client_hold_ms;
    Net_Result bind_result;
    Net_Result recv_result;
    usize      recv_elapsed_ms;
    string     received_string;
} RecvTimeoutArgs;

typedef struct {
    char       url[64];
    Net_Result bind_result;
    Net_Result recv_result;
    u32        recv_delay_ms;
} NonblockingServerArgs;

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

internal void _nexus_make_udp_message_test_url(char* out_url,
                                               usize out_url_size)
{
    static u16 next_port = 19180;

    next_port++;
    snprintf(out_url,
             out_url_size,
             "udp://127.0.0.1:%u",
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
    args->recv_result = net_recv(&msg);
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
    args->send_result = net_send(&msg);

    net_message_done(&msg);
    net_close(&sock);
    return NULL;
}

internal void* _nexus_udp_message_server_round_trip(void* arg)
{
    UdpMessageServerArgs* args = arg;

    Net_Socket sock            = net_socket();
    args->bind_result          = net_bind(&sock, args->url);
    if (NET_FAILED(args->bind_result)) {
        return NULL;
    }

    Net_Message msg   = net_message_create(&sock);
    args->recv_result = net_recv(&msg);
    if (NET_FAILED(args->recv_result)) {
        net_message_done(&msg);
        net_close(&sock);
        return NULL;
    }

    TEST_ASSERT(net_message_read_string(&msg, &args->received_string));

    net_message_clear(&msg);
    net_message_append_string(&msg, S("udp-reply"));
    args->send_result = net_send(&msg);

    net_message_done(&msg);
    net_close(&sock);
    return NULL;
}

internal void* _nexus_multi_client_server_round_trip(void* arg)
{
    MultiClientServerArgs* args = arg;

    Net_Socket sock             = net_socket();
    args->bind_result           = net_bind(&sock, args->url);
    if (NET_FAILED(args->bind_result)) {
        return NULL;
    }

    Net_Message msg = net_message_create(&sock);
    for (usize i = 0; i < 2; ++i) {
        args->recv_results[i] = net_recv(&msg);
        if (NET_FAILED(args->recv_results[i])) {
            break;
        }

        TEST_ASSERT(net_message_read_string(&msg, &args->received_texts[i]));
        TEST_ASSERT(net_message_read_u32(&msg, &args->received_ids[i]));

        net_message_clear(&msg);
        net_message_append_string(&msg, args->received_texts[i]);
        net_message_append_u32(&msg, args->received_ids[i]);
        args->send_results[i] = net_send(&msg);
        if (NET_FAILED(args->send_results[i])) {
            break;
        }
    }

    net_message_done(&msg);
    net_close(&sock);
    return NULL;
}

internal void* _nexus_multi_client_round_trip(void* arg)
{
    MultiClientArgs* args = arg;

    Net_Socket sock       = net_socket();
    args->connect_result  = net_connect(&sock, args->url);
    if (NET_FAILED(args->connect_result)) {
        return NULL;
    }

    Net_Message outbound = net_message_create(&sock);
    net_message_append_string(&outbound, args->request_text);
    net_message_append_u32(&outbound, args->request_id);
    args->send_result = net_send(&outbound);
    if (NET_FAILED(args->send_result)) {
        net_message_done(&outbound);
        net_close(&sock);
        return NULL;
    }

    Net_Message inbound = net_message_create(&sock);
    args->recv_result   = net_recv(&inbound);
    if (args->recv_result == NET_OK) {
        string reply_text;
        TEST_ASSERT(net_message_read_string(&inbound, &reply_text));
        TEST_ASSERT(net_message_read_u32(&inbound, &args->reply_id));
        TEST_ASSERT_LT(reply_text.count, sizeof(args->reply_text_storage));
        memcpy(args->reply_text_storage, reply_text.data, reply_text.count);
        args->reply_text_storage[reply_text.count] = 0;
        args->reply_text_len                       = reply_text.count;
    }

    net_message_done(&outbound);
    net_message_done(&inbound);
    net_close(&sock);
    return NULL;
}

internal void* _nexus_delayed_bind_server(void* arg)
{
    DelayedBindServerArgs* args = arg;

    thread_sleep_ms(args->bind_delay_ms);

    Net_Socket sock   = net_socket();
    args->bind_result = net_bind(&sock, args->url);
    if (NET_FAILED(args->bind_result)) {
        return NULL;
    }

    Net_Message msg   = net_message_create(&sock);
    args->recv_result = net_recv(&msg);
    if (args->recv_result == NET_OK) {
        TEST_ASSERT(net_message_read_string(&msg, &args->received_string));
    }

    net_message_done(&msg);
    net_close(&sock);
    return NULL;
}

internal void* _nexus_recv_timeout_server(void* arg)
{
    RecvTimeoutArgs* args = arg;

    Net_Socket sock       = net_socket();
    args->bind_result     = net_bind(&sock, args->url);
    if (NET_FAILED(args->bind_result)) {
        return NULL;
    }

    TEST_ASSERT_EQ(
        net_set_option(&sock, NET_OPT_RECV_TIMEOUT_MS, args->recv_timeout_ms),
        NET_OK);

    Net_Message msg   = net_message_create(&sock);
    TimePoint   start = time_now();
    args->recv_result = net_recv(&msg);
    args->recv_elapsed_ms =
        (usize)time_duration_to_ms(time_elapsed(start, time_now()));
    if (args->recv_result == NET_OK) {
        TEST_ASSERT(net_message_read_string(&msg, &args->received_string));
    }

    net_message_done(&msg);
    net_close(&sock);
    return NULL;
}

internal void* _nexus_delayed_client_send(void* arg)
{
    RecvTimeoutArgs* args = arg;

    thread_sleep_ms(args->client_delay_ms);

    Net_Socket sock = net_socket();
    if (NET_FAILED(net_connect(&sock, args->url))) {
        return NULL;
    }

    if (args->client_hold_ms > 0) {
        thread_sleep_ms(args->client_hold_ms);
    }

    Net_Message msg = net_message_create(&sock);
    net_message_append_string(&msg, S("delayed-send"));
    (void)net_send(&msg);

    net_message_done(&msg);
    net_close(&sock);
    return NULL;
}

internal void* _nexus_nonblocking_server_recv_once(void* arg)
{
    NonblockingServerArgs* args = arg;

    Net_Socket sock             = net_socket();
    args->bind_result           = net_bind(&sock, args->url);
    if (NET_FAILED(args->bind_result)) {
        return NULL;
    }

    TEST_ASSERT_EQ(net_set_option(&sock, NET_OPT_NONBLOCKING, 1), NET_OK);

    if (args->recv_delay_ms > 0) {
        thread_sleep_ms(args->recv_delay_ms);
    }

    Net_Message msg   = net_message_create(&sock);
    args->recv_result = net_recv(&msg);

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
    TEST_ASSERT_EQ(net_send(&outbound), NET_OK);

    Net_Message inbound = net_message_create(&client_sock);
    TEST_ASSERT_EQ(net_recv(&inbound), NET_OK);

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

TEST_CASE(nexus, udp_recv_msg_preserves_reply_route_for_send_msg)
{
    UdpMessageServerArgs args = {0};
    _nexus_make_udp_message_test_url(args.url, sizeof(args.url));

    Thread server_thread;
    TEST_ASSERT(thread_create(
        &server_thread, _nexus_udp_message_server_round_trip, &args));

    _nexus_message_wait_for_server_start();

    Net_Socket client_sock = net_socket();
    TEST_ASSERT_EQ(net_connect(&client_sock, args.url), NET_OK);

    Net_Message outbound = net_message_create(&client_sock);
    net_message_append_string(&outbound, S("udp-request"));
    TEST_ASSERT_EQ(net_send(&outbound), NET_OK);

    Net_Message inbound = net_message_create(&client_sock);
    TEST_ASSERT_EQ(net_recv(&inbound), NET_OK);

    string reply_text;
    TEST_ASSERT(net_message_read_string(&inbound, &reply_text));

    thread_join(&server_thread);

    TEST_ASSERT_EQ(args.bind_result, NET_OK);
    TEST_ASSERT_EQ(args.recv_result, NET_OK);
    TEST_ASSERT_EQ(args.send_result, NET_OK);
    TEST_ASSERT_EQ(args.received_string.count, (usize)11);
    TEST_ASSERT_MEM_EQ(args.received_string.data, "udp-request", 11);
    TEST_ASSERT_EQ(reply_text.count, (usize)9);
    TEST_ASSERT_MEM_EQ(reply_text.data, "udp-reply", 9);

    net_message_done(&outbound);
    net_message_done(&inbound);
    net_close(&client_sock);
}

TEST_CASE(nexus, tcp_server_can_reply_to_multiple_clients_via_message_pipe)
{
    MultiClientServerArgs server_args = {0};
    _nexus_make_message_test_url(server_args.url, sizeof(server_args.url));

    Thread server_thread;
    TEST_ASSERT(thread_create(
        &server_thread, _nexus_multi_client_server_round_trip, &server_args));

    _nexus_message_wait_for_server_start();

    MultiClientArgs client_a = {
        .request_text = S("client-a"),
        .request_id   = 101,
    };
    MultiClientArgs client_b = {
        .request_text = S("client-b"),
        .request_id   = 202,
    };

    snprintf(client_a.url, sizeof(client_a.url), "%s", server_args.url);
    snprintf(client_b.url, sizeof(client_b.url), "%s", server_args.url);

    Thread client_thread_a;
    Thread client_thread_b;
    TEST_ASSERT(thread_create(
        &client_thread_a, _nexus_multi_client_round_trip, &client_a));
    TEST_ASSERT(thread_create(
        &client_thread_b, _nexus_multi_client_round_trip, &client_b));

    thread_join(&client_thread_a);
    thread_join(&client_thread_b);
    thread_join(&server_thread);

    TEST_ASSERT_EQ(server_args.bind_result, NET_OK);
    TEST_ASSERT_EQ(server_args.recv_results[0], NET_OK);
    TEST_ASSERT_EQ(server_args.recv_results[1], NET_OK);
    TEST_ASSERT_EQ(server_args.send_results[0], NET_OK);
    TEST_ASSERT_EQ(server_args.send_results[1], NET_OK);

    TEST_ASSERT_EQ(client_a.connect_result, NET_OK);
    TEST_ASSERT_EQ(client_a.send_result, NET_OK);
    TEST_ASSERT_EQ(client_a.recv_result, NET_OK);
    TEST_ASSERT_EQ(client_a.reply_text_len, client_a.request_text.count);
    TEST_ASSERT_MEM_EQ(client_a.reply_text_storage,
                       client_a.request_text.data,
                       client_a.request_text.count);
    TEST_ASSERT_EQ(client_a.reply_id, client_a.request_id);

    TEST_ASSERT_EQ(client_b.connect_result, NET_OK);
    TEST_ASSERT_EQ(client_b.send_result, NET_OK);
    TEST_ASSERT_EQ(client_b.recv_result, NET_OK);
    TEST_ASSERT_EQ(client_b.reply_text_len, client_b.request_text.count);
    TEST_ASSERT_MEM_EQ(client_b.reply_text_storage,
                       client_b.request_text.data,
                       client_b.request_text.count);
    TEST_ASSERT_EQ(client_b.reply_id, client_b.request_id);
}

TEST_CASE(nexus, socket_options_can_be_set_and_read_back)
{
    Net_Socket sock  = net_socket();
    u64        value = 0;

    TEST_ASSERT_EQ(net_set_option(&sock, NET_OPT_CONNECT_TIMEOUT_MS, 250),
                   NET_OK);
    TEST_ASSERT_EQ(net_set_option(&sock, NET_OPT_RECONNECT_INTERVAL_MS, 25),
                   NET_OK);
    TEST_ASSERT_EQ(net_set_option(&sock, NET_OPT_SEND_TIMEOUT_MS, 500), NET_OK);
    TEST_ASSERT_EQ(net_set_option(&sock, NET_OPT_RECV_TIMEOUT_MS, 750), NET_OK);
    TEST_ASSERT_EQ(net_set_option(&sock, NET_OPT_NONBLOCKING, 1), NET_OK);

    TEST_ASSERT_EQ(net_get_option(&sock, NET_OPT_CONNECT_TIMEOUT_MS, &value),
                   NET_OK);
    TEST_ASSERT_EQ(value, 250u);
    TEST_ASSERT_EQ(net_get_option(&sock, NET_OPT_RECONNECT_INTERVAL_MS, &value),
                   NET_OK);
    TEST_ASSERT_EQ(value, 25u);
    TEST_ASSERT_EQ(net_get_option(&sock, NET_OPT_SEND_TIMEOUT_MS, &value),
                   NET_OK);
    TEST_ASSERT_EQ(value, 500u);
    TEST_ASSERT_EQ(net_get_option(&sock, NET_OPT_RECV_TIMEOUT_MS, &value),
                   NET_OK);
    TEST_ASSERT_EQ(value, 750u);
    TEST_ASSERT_EQ(net_get_option(&sock, NET_OPT_NONBLOCKING, &value), NET_OK);
    TEST_ASSERT_EQ(value, 1u);

    net_close(&sock);
}

TEST_CASE(nexus, connect_can_retry_until_server_binds)
{
    DelayedBindServerArgs server_args = {
        .bind_delay_ms = 120,
    };
    _nexus_make_message_test_url(server_args.url, sizeof(server_args.url));

    Thread server_thread;
    TEST_ASSERT(thread_create(
        &server_thread, _nexus_delayed_bind_server, &server_args));

    Net_Socket client_sock = net_socket();
    TEST_ASSERT_EQ(
        net_set_option(&client_sock, NET_OPT_CONNECT_TIMEOUT_MS, 1000), NET_OK);
    TEST_ASSERT_EQ(
        net_set_option(&client_sock, NET_OPT_RECONNECT_INTERVAL_MS, 25),
        NET_OK);
    TEST_ASSERT_EQ(net_connect(&client_sock, server_args.url), NET_OK);

    Net_Message msg = net_message_create(&client_sock);
    net_message_append_string(&msg, S("delayed-connect"));
    TEST_ASSERT_EQ(net_send(&msg), NET_OK);

    net_message_done(&msg);
    net_close(&client_sock);
    thread_join(&server_thread);

    TEST_ASSERT_EQ(server_args.bind_result, NET_OK);
    TEST_ASSERT_EQ(server_args.recv_result, NET_OK);
    TEST_ASSERT_EQ(server_args.received_string.count, (usize)15);
    TEST_ASSERT_MEM_EQ(server_args.received_string.data, "delayed-connect", 15);
}

TEST_CASE(nexus, connect_times_out_when_server_never_binds)
{
    char url[64];
    _nexus_make_message_test_url(url, sizeof(url));

    Net_Socket client_sock = net_socket();
    TEST_ASSERT_EQ(
        net_set_option(&client_sock, NET_OPT_CONNECT_TIMEOUT_MS, 150), NET_OK);
    TEST_ASSERT_EQ(
        net_set_option(&client_sock, NET_OPT_RECONNECT_INTERVAL_MS, 25),
        NET_OK);

    TimePoint  start  = time_now();
    Net_Result result = net_connect(&client_sock, url);
    u64 elapsed_ms    = time_duration_to_ms(time_elapsed(start, time_now()));

    TEST_ASSERT_EQ(result, NET_TIMEOUT);
    TEST_ASSERT_GE(elapsed_ms, 100u);

    net_close(&client_sock);
}

TEST_CASE(nexus, recv_times_out_when_no_client_arrives)
{
    RecvTimeoutArgs args = {
        .recv_timeout_ms = 120,
    };
    _nexus_make_message_test_url(args.url, sizeof(args.url));

    Thread server_thread;
    TEST_ASSERT(
        thread_create(&server_thread, _nexus_recv_timeout_server, &args));

    thread_join(&server_thread);

    TEST_ASSERT_EQ(args.bind_result, NET_OK);
    TEST_ASSERT_EQ(args.recv_result, NET_TIMEOUT);
    TEST_ASSERT_GE(args.recv_elapsed_ms, (usize)80);
}

TEST_CASE(nexus, recv_times_out_when_client_connects_but_sends_nothing)
{
    RecvTimeoutArgs args = {
        .recv_timeout_ms = 120,
        .client_delay_ms = 20,
        .client_hold_ms  = 200,
    };
    _nexus_make_message_test_url(args.url, sizeof(args.url));

    Thread server_thread;
    Thread client_thread;
    TEST_ASSERT(
        thread_create(&server_thread, _nexus_recv_timeout_server, &args));
    TEST_ASSERT(
        thread_create(&client_thread, _nexus_delayed_client_send, &args));

    thread_join(&server_thread);
    thread_join(&client_thread);

    TEST_ASSERT_EQ(args.bind_result, NET_OK);
    TEST_ASSERT_EQ(args.recv_result, NET_TIMEOUT);
    TEST_ASSERT_GE(args.recv_elapsed_ms, (usize)80);
}

TEST_CASE(nexus, recv_succeeds_when_message_arrives_before_timeout)
{
    RecvTimeoutArgs args = {
        .recv_timeout_ms = 500,
        .client_delay_ms = 100,
    };
    _nexus_make_message_test_url(args.url, sizeof(args.url));

    Thread server_thread;
    Thread client_thread;
    TEST_ASSERT(
        thread_create(&server_thread, _nexus_recv_timeout_server, &args));
    TEST_ASSERT(
        thread_create(&client_thread, _nexus_delayed_client_send, &args));

    thread_join(&server_thread);
    thread_join(&client_thread);

    TEST_ASSERT_EQ(args.bind_result, NET_OK);
    TEST_ASSERT_EQ(args.recv_result, NET_OK);
    TEST_ASSERT_EQ(args.received_string.count, (usize)12);
    TEST_ASSERT_MEM_EQ(args.received_string.data, "delayed-send", 12);
}

TEST_CASE(nexus, nonblocking_connect_returns_would_block_when_server_is_absent)
{
    char url[64];
    _nexus_make_message_test_url(url, sizeof(url));

    Net_Socket sock = net_socket();
    TEST_ASSERT_EQ(net_set_option(&sock, NET_OPT_NONBLOCKING, 1), NET_OK);

    TEST_ASSERT_EQ(net_connect(&sock, url), NET_WOULD_BLOCK);

    net_close(&sock);
}

TEST_CASE(nexus, nonblocking_recv_returns_would_block_when_no_client_arrives)
{
    NonblockingServerArgs args = {0};
    _nexus_make_message_test_url(args.url, sizeof(args.url));

    Thread server_thread;
    TEST_ASSERT(thread_create(
        &server_thread, _nexus_nonblocking_server_recv_once, &args));

    thread_join(&server_thread);

    TEST_ASSERT_EQ(args.bind_result, NET_OK);
    TEST_ASSERT_EQ(args.recv_result, NET_WOULD_BLOCK);
}

TEST_CASE(nexus, nonblocking_recv_returns_would_block_when_client_sends_nothing)
{
    NonblockingServerArgs args = {
        .recv_delay_ms = 200,
    };
    _nexus_make_message_test_url(args.url, sizeof(args.url));

    Net_Socket client = net_socket();

    Thread server_thread;
    TEST_ASSERT(thread_create(
        &server_thread, _nexus_nonblocking_server_recv_once, &args));

    _nexus_message_wait_for_server_start();
    TEST_ASSERT_EQ(net_connect(&client, args.url), NET_OK);

    thread_join(&server_thread);

    TEST_ASSERT_EQ(args.bind_result, NET_OK);
    TEST_ASSERT_EQ(args.recv_result, NET_WOULD_BLOCK);

    net_close(&client);
}

TEST_CASE(nexus, nonblocking_udp_recv_returns_would_block_without_datagram)
{
    char url[64];
    _nexus_make_udp_message_test_url(url, sizeof(url));

    Net_Socket sock = net_socket();
    TEST_ASSERT_EQ(net_bind(&sock, url), NET_OK);
    TEST_ASSERT_EQ(net_set_option(&sock, NET_OPT_NONBLOCKING, 1), NET_OK);

    Net_Message msg = net_message_create(&sock);
    TEST_ASSERT_EQ(net_recv(&msg), NET_WOULD_BLOCK);

    net_message_done(&msg);
    net_close(&sock);
}
