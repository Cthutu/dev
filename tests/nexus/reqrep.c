//> use: core nexus thread

#include <core/core.h>
#include <nexus/nexus.h>
#include <test.h>
#include <thread/thread.h>
#include "test_net.h"

typedef struct {
    char       url[64];
    string     expected_request_text;
    Net_Result bind_result;
    Net_Result recv_result;
    Net_Result send_result;
    u64        sender_id;
    char       sender_url[64];
    usize      sender_url_len;
    u64        sender_id_after_clear;
    char       sender_url_after_clear[64];
    usize      sender_url_after_clear_len;
    u32        request_id;
} ReqRepServerArgs;

internal void _nexus_make_reqrep_test_url(char* out_url, usize out_url_size)
{
    u16 port = _nexus_choose_test_port(SOCK_STREAM, IPPROTO_TCP);
    snprintf(out_url, out_url_size, "tcp://127.0.0.1:%u", (unsigned)port);
}

internal void _nexus_make_reqrep_udp_test_url(char* out_url, usize out_url_size)
{
    u16 port = _nexus_choose_test_port(SOCK_DGRAM, IPPROTO_UDP);
    snprintf(out_url, out_url_size, "udp://127.0.0.1:%u", (unsigned)port);
}

internal void _nexus_reqrep_wait_for_server_start(void) { thread_sleep_ms(50); }

internal void* _nexus_reply_server_round_trip(void* arg)
{
    ReqRepServerArgs* args = arg;

    Net_Socket sock        = net_reply_socket();
    args->bind_result      = net_bind(&sock, args->url);
    if (NET_FAILED(args->bind_result)) {
        return NULL;
    }

    Net_Message msg   = net_message_create(&sock);
    args->recv_result = net_recv(&msg);
    if (args->recv_result == NET_OK) {
        string sender_url;
        TEST_ASSERT(net_message_id(&msg, &args->sender_id));
        TEST_ASSERT(net_message_url(&msg, &sender_url));
        TEST_ASSERT_LT(sender_url.count, sizeof(args->sender_url));
        memcpy(args->sender_url, sender_url.data, sender_url.count);
        args->sender_url[sender_url.count] = 0;
        args->sender_url_len               = sender_url.count;

        string request_text;
        TEST_ASSERT(net_message_read_string(&msg, &request_text));
        TEST_ASSERT_EQ(request_text.count, args->expected_request_text.count);
        TEST_ASSERT_MEM_EQ(request_text.data,
                           args->expected_request_text.data,
                           args->expected_request_text.count);
        TEST_ASSERT(net_message_read_u32(&msg, &args->request_id));

        net_message_clear(&msg);
        TEST_ASSERT(net_message_id(&msg, &args->sender_id_after_clear));
        TEST_ASSERT(net_message_url(&msg, &sender_url));
        TEST_ASSERT_LT(sender_url.count, sizeof(args->sender_url_after_clear));
        memcpy(args->sender_url_after_clear, sender_url.data, sender_url.count);
        args->sender_url_after_clear[sender_url.count] = 0;
        args->sender_url_after_clear_len               = sender_url.count;
        net_message_append_string(&msg, S("reply"));
        net_message_append_u32(&msg, args->request_id + 1);
        args->send_result = net_send(&msg);
    }

    net_message_done(&msg);
    net_close(&sock);
    return NULL;
}

TEST_CASE(nexus, request_reply_round_trip_over_tcp)
{
    ReqRepServerArgs server_args = {
        .expected_request_text = S("request"),
    };
    _nexus_make_reqrep_test_url(server_args.url, sizeof(server_args.url));

    Thread server_thread;
    TEST_ASSERT(thread_create(
        &server_thread, _nexus_reply_server_round_trip, &server_args));

    _nexus_reqrep_wait_for_server_start();

    Net_Socket client = net_request_socket();
    TEST_ASSERT_EQ(net_connect(&client, server_args.url), NET_OK);

    Net_Message msg = net_message_create(&client);
    net_message_append_string(&msg, S("request"));
    net_message_append_u32(&msg, 41);
    TEST_ASSERT_EQ(net_send(&msg), NET_OK);

    TEST_ASSERT_EQ(net_recv(&msg), NET_OK);

    string reply_text;
    u32    reply_id = 0;
    TEST_ASSERT(net_message_read_string(&msg, &reply_text));
    TEST_ASSERT(net_message_read_u32(&msg, &reply_id));

    thread_join(&server_thread);

    TEST_ASSERT_EQ(server_args.bind_result, NET_OK);
    TEST_ASSERT_EQ(server_args.recv_result, NET_OK);
    TEST_ASSERT_EQ(server_args.send_result, NET_OK);
    TEST_ASSERT_EQ(server_args.request_id, 41u);
    TEST_ASSERT_EQ(server_args.sender_id_after_clear, server_args.sender_id);
    TEST_ASSERT_EQ(server_args.sender_url_after_clear_len,
                   server_args.sender_url_len);
    TEST_ASSERT_MEM_EQ(server_args.sender_url_after_clear,
                       server_args.sender_url,
                       server_args.sender_url_len);
    TEST_ASSERT_GT(server_args.sender_id, 0u);
    TEST_ASSERT(server_args.sender_url_len > 0);
    TEST_ASSERT_EQ(reply_text.count, (usize)5);
    TEST_ASSERT_MEM_EQ(reply_text.data, "reply", 5);
    TEST_ASSERT_EQ(reply_id, 42u);

    net_message_done(&msg);
    net_close(&client);
}

TEST_CASE(nexus, request_socket_cannot_receive_before_sending)
{
    char url[64];
    _nexus_make_reqrep_udp_test_url(url, sizeof(url));

    Net_Socket server = net_reply_socket();
    TEST_ASSERT_EQ(net_bind(&server, url), NET_OK);

    Net_Socket client = net_request_socket();
    TEST_ASSERT_EQ(net_connect(&client, url), NET_OK);

    Net_Message msg = net_message_create(&client);

    TEST_ASSERT_EQ(net_recv(&msg), NET_WRONG_STATE);

    net_message_done(&msg);
    net_close(&client);
    net_close(&server);
}

TEST_CASE(nexus, request_socket_cannot_send_twice_without_reply)
{
    Net_Socket sock              = net_request_socket();

    ReqRepServerArgs server_args = {
        .expected_request_text = S("one"),
    };
    _nexus_make_reqrep_test_url(server_args.url, sizeof(server_args.url));

    Thread server_thread;
    TEST_ASSERT(thread_create(
        &server_thread, _nexus_reply_server_round_trip, &server_args));

    _nexus_reqrep_wait_for_server_start();
    TEST_ASSERT_EQ(net_connect(&sock, server_args.url), NET_OK);

    Net_Message msg = net_message_create(&sock);
    net_message_append_string(&msg, S("one"));
    net_message_append_u32(&msg, 7);
    TEST_ASSERT_EQ(net_send(&msg), NET_OK);

    net_message_clear(&msg);
    net_message_append_string(&msg, S("two"));
    TEST_ASSERT_EQ(net_send(&msg), NET_WRONG_STATE);

    TEST_ASSERT_EQ(net_recv(&msg), NET_OK);

    net_message_done(&msg);
    net_close(&sock);
    thread_join(&server_thread);
}

TEST_CASE(nexus, reply_socket_cannot_send_before_receiving)
{
    char url[64];
    _nexus_make_reqrep_udp_test_url(url, sizeof(url));

    Net_Socket sock = net_reply_socket();
    TEST_ASSERT_EQ(net_bind(&sock, url), NET_OK);

    Net_Message msg = net_message_create(&sock);

    TEST_ASSERT_EQ(net_send(&msg), NET_WRONG_STATE);

    net_message_done(&msg);
    net_close(&sock);
}
