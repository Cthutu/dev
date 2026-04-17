//> use: core nexus thread

#include <core/core.h>
#include <nexus/nexus.h>
#include <test.h>
#include <thread/thread.h>
#include "test_net.h"

typedef struct {
    char       url[64];
    string     first_message;
    Net_Result bind_result;
    Net_Result recv_result;
    usize      recv_len;
    u8         recv_buffer[256];
} FramingServerArgs;

internal void _nexus_wait_for_server_start(void) { thread_sleep_ms(50); }

internal void _nexus_make_test_url(char* out_url, usize out_url_size)
{
    u16 port = _nexus_choose_test_port(SOCK_STREAM, IPPROTO_TCP);
    snprintf(out_url, out_url_size, "tcp://127.0.0.1:%u", (unsigned)port);
}

//------------------------------------------------------------------------------
// _nexus_server_recv_once
//
// Receives a single Nexus message and copies its body into the test-owned
// buffer for later assertions.
//------------------------------------------------------------------------------

internal void* _nexus_server_recv_once(void* arg)
{
    FramingServerArgs* args = arg;

    Net_Socket sock         = net_socket();
    args->bind_result       = net_bind(&sock, args->url);
    if (NET_FAILED(args->bind_result)) {
        return NULL;
    }

    Net_Message msg   = net_message_create(&sock);
    args->recv_result = net_recv(&msg);
    if (args->recv_result == NET_OK) {
        args->recv_len = msg.length;
        if (msg.length > 0) {
            TEST_ASSERT_LE(msg.length, sizeof(args->recv_buffer));
            memcpy(args->recv_buffer, msg.data, msg.length);
        }
    }

    net_message_done(&msg);
    net_close(&sock);
    return NULL;
}

TEST_CASE(nexus, tcp_message_framing_round_trip)
{
    FramingServerArgs args = {
        .first_message = S("Hello from a framed TCP client"),
    };
    _nexus_make_test_url(args.url, sizeof(args.url));

    Thread server_thread;
    TEST_ASSERT(thread_create(&server_thread, _nexus_server_recv_once, &args));

    _nexus_wait_for_server_start();

    Net_Socket client = net_socket();
    TEST_ASSERT_EQ(net_connect(&client, args.url), NET_OK);

    Net_Message msg = net_message_create(&client);
    net_message_append(&msg, args.first_message.data, args.first_message.count);
    TEST_ASSERT_EQ(net_send(&msg), NET_OK);

    net_message_done(&msg);
    net_close(&client);
    thread_join(&server_thread);

    TEST_ASSERT_EQ(args.bind_result, NET_OK);
    TEST_ASSERT_EQ(args.recv_result, NET_OK);
    TEST_ASSERT_EQ(args.recv_len, args.first_message.count);
    TEST_ASSERT_MEM_EQ(
        args.recv_buffer, args.first_message.data, args.first_message.count);
}

TEST_CASE(nexus, default_socket_constructor_works_for_message_sockets)
{
    FramingServerArgs args = {
        .first_message = S("Hello from net_socket"),
    };
    _nexus_make_test_url(args.url, sizeof(args.url));

    Thread server_thread;
    TEST_ASSERT(thread_create(&server_thread, _nexus_server_recv_once, &args));

    _nexus_wait_for_server_start();

    Net_Socket client = net_socket();
    TEST_ASSERT_EQ(net_connect(&client, args.url), NET_OK);

    Net_Message msg = net_message_create(&client);
    net_message_append(&msg, args.first_message.data, args.first_message.count);
    TEST_ASSERT_EQ(net_send(&msg), NET_OK);

    net_message_done(&msg);
    net_close(&client);
    thread_join(&server_thread);

    TEST_ASSERT_EQ(args.bind_result, NET_OK);
    TEST_ASSERT_EQ(args.recv_result, NET_OK);
    TEST_ASSERT_EQ(args.recv_len, args.first_message.count);
    TEST_ASSERT_MEM_EQ(
        args.recv_buffer, args.first_message.data, args.first_message.count);
}

TEST_CASE(nexus, zero_length_messages_are_valid)
{
    FramingServerArgs args = {};
    _nexus_make_test_url(args.url, sizeof(args.url));

    Thread server_thread;
    TEST_ASSERT(thread_create(&server_thread, _nexus_server_recv_once, &args));

    _nexus_wait_for_server_start();

    Net_Socket client = net_socket();
    TEST_ASSERT_EQ(net_connect(&client, args.url), NET_OK);

    Net_Message msg = net_message_create(&client);
    TEST_ASSERT_EQ(net_send(&msg), NET_OK);

    net_message_done(&msg);
    net_close(&client);
    thread_join(&server_thread);

    TEST_ASSERT_EQ(args.bind_result, NET_OK);
    TEST_ASSERT_EQ(args.recv_result, NET_OK);
    TEST_ASSERT_EQ(args.recv_len, 0);
}

TEST_CASE(nexus, send_rejects_messages_larger_than_maximum)
{
    FramingServerArgs args = {
        .first_message = S("ok"),
    };
    _nexus_make_test_url(args.url, sizeof(args.url));

    Thread server_thread;
    TEST_ASSERT(thread_create(&server_thread, _nexus_server_recv_once, &args));

    _nexus_wait_for_server_start();

    Net_Socket client = net_socket();
    TEST_ASSERT_EQ(net_connect(&client, args.url), NET_OK);

    Net_Message msg = net_message_create(&client);
    msg.data = mem_realloc(NULL, NET_MAX_MESSAGE_SIZE + 1, __FILE__, __LINE__);
    msg.length   = NET_MAX_MESSAGE_SIZE + 1;
    msg.capacity = NET_MAX_MESSAGE_SIZE + 1;

    TEST_ASSERT_EQ(net_send(&msg), NET_BAD_MESSAGE);

    net_message_clear(&msg);
    net_message_append(&msg, args.first_message.data, args.first_message.count);
    TEST_ASSERT_EQ(net_send(&msg), NET_OK);

    net_message_done(&msg);
    net_close(&client);
    thread_join(&server_thread);

    TEST_ASSERT_EQ(args.bind_result, NET_OK);
    TEST_ASSERT_EQ(args.recv_result, NET_OK);
    TEST_ASSERT_EQ(args.recv_len, args.first_message.count);
    TEST_ASSERT_MEM_EQ(
        args.recv_buffer, args.first_message.data, args.first_message.count);
}
