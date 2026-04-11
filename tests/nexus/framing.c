//> use: core nexus thread

#include <core/core.h>
#include <nexus/nexus.h>
#include <test.h>
#include <thread/thread.h>

typedef struct {
    cstr       url;
    string     first_message;
    string     second_message;
    Net_Result bind_result;
    Net_Result recv_result;
    Net_Result second_recv_result;
    Net_Result drop_result;
    usize      recv_len;
    usize      second_recv_len;
    usize      required_len;
    usize      dropped_len;
    u8         recv_buffer[256];
    u8         second_recv_buffer[256];
} FramingServerArgs;

internal void _nexus_wait_for_server_start(void) { thread_sleep_ms(50); }

internal void* _nexus_server_recv_once(void* arg)
{
    FramingServerArgs* args = arg;

    Net_Socket sock         = net_socket();
    args->bind_result       = net_bind(&sock, args->url);
    if (NET_FAILED(args->bind_result)) {
        return NULL;
    }

    args->recv_result = net_recv(
        &sock, args->recv_buffer, sizeof(args->recv_buffer), &args->recv_len);

    net_close(&sock);
    return NULL;
}

internal void* _nexus_server_drop_and_retry(void* arg)
{
    FramingServerArgs* args = arg;

    Net_Socket sock         = net_socket();
    args->bind_result       = net_bind(&sock, args->url);
    if (NET_FAILED(args->bind_result)) {
        return NULL;
    }

    u8 small_buffer[4];
    args->recv_result = net_recv(
        &sock, small_buffer, sizeof(small_buffer), &args->required_len);
    args->drop_result        = net_recv(&sock, NULL, 0, &args->dropped_len);
    args->second_recv_result = net_recv(&sock,
                                        args->second_recv_buffer,
                                        sizeof(args->second_recv_buffer),
                                        &args->second_recv_len);

    net_close(&sock);
    return NULL;
}

TEST_CASE(nexus, tcp_message_framing_round_trip)
{
    FramingServerArgs args = {
        .url           = "tcp://127.0.0.1:18081",
        .first_message = S("Hello from a framed TCP client"),
    };

    Thread server_thread;
    TEST_ASSERT(thread_create(&server_thread, _nexus_server_recv_once, &args));

    _nexus_wait_for_server_start();

    Net_Socket client = net_socket();
    TEST_ASSERT_EQ(net_connect(&client, args.url), NET_OK);
    TEST_ASSERT_EQ(
        net_send(&client, args.first_message.data, args.first_message.count),
        NET_OK);
    net_close(&client);

    thread_join(&server_thread);

    TEST_ASSERT_EQ(args.bind_result, NET_OK);
    TEST_ASSERT_EQ(args.recv_result, NET_OK);
    TEST_ASSERT_EQ(args.recv_len, args.first_message.count);
    TEST_ASSERT_MEM_EQ(
        args.recv_buffer, args.first_message.data, args.first_message.count);
}

TEST_CASE(nexus, recv_buffer_too_small_can_drop_pending_message)
{
    FramingServerArgs args = {
        .url            = "tcp://127.0.0.1:18082",
        .first_message  = S("message-one"),
        .second_message = S("two"),
    };

    Thread server_thread;
    TEST_ASSERT(
        thread_create(&server_thread, _nexus_server_drop_and_retry, &args));

    _nexus_wait_for_server_start();

    Net_Socket client = net_socket();
    TEST_ASSERT_EQ(net_connect(&client, args.url), NET_OK);
    TEST_ASSERT_EQ(
        net_send(&client, args.first_message.data, args.first_message.count),
        NET_OK);
    TEST_ASSERT_EQ(
        net_send(&client, args.second_message.data, args.second_message.count),
        NET_OK);
    net_close(&client);

    thread_join(&server_thread);

    TEST_ASSERT_EQ(args.bind_result, NET_OK);
    TEST_ASSERT_EQ(args.recv_result, NET_BUFFER_TOO_SMALL);
    TEST_ASSERT_EQ(args.required_len, args.first_message.count);
    TEST_ASSERT_EQ(args.drop_result, NET_OK);
    TEST_ASSERT_EQ(args.dropped_len, args.first_message.count);
    TEST_ASSERT_EQ(args.second_recv_result, NET_OK);
    TEST_ASSERT_EQ(args.second_recv_len, args.second_message.count);
    TEST_ASSERT_MEM_EQ(args.second_recv_buffer,
                       args.second_message.data,
                       args.second_message.count);
}

TEST_CASE(nexus, zero_length_messages_are_valid)
{
    FramingServerArgs args = {
        .url = "tcp://127.0.0.1:18083",
    };

    Thread server_thread;
    TEST_ASSERT(thread_create(&server_thread, _nexus_server_recv_once, &args));

    _nexus_wait_for_server_start();

    Net_Socket client = net_socket();
    TEST_ASSERT_EQ(net_connect(&client, args.url), NET_OK);
    TEST_ASSERT_EQ(net_send(&client, NULL, 0), NET_OK);
    net_close(&client);

    thread_join(&server_thread);

    TEST_ASSERT_EQ(args.bind_result, NET_OK);
    TEST_ASSERT_EQ(args.recv_result, NET_OK);
    TEST_ASSERT_EQ(args.recv_len, 0);
}

TEST_CASE(nexus, send_rejects_messages_larger_than_maximum)
{
    Net_Socket sock = net_socket();
    sock.state      = NET_STATE_CONNECTED;
    sock.proto      = NET_PROTO_TCP;

    u8* large_buffer =
        mem_realloc(NULL, NET_MAX_MESSAGE_SIZE + 1, __FILE__, __LINE__);

    TEST_ASSERT_EQ(net_send(&sock, large_buffer, NET_MAX_MESSAGE_SIZE + 1),
                   NET_BAD_MESSAGE);

    mem_free(large_buffer, __FILE__, __LINE__);
}
