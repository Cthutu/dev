//> use: core nexus thread

#include <core/core.h>
#include <nexus/nexus.h>
#include <test.h>
#include <thread/thread.h>
#include "test_net.h"

typedef struct {
    char       url[64];
    bool       echo_reply;
    Net_Result bind_result;
    Net_Result recv_result;
    Net_Result send_result;
    bool       has_bounds;
    u16        width;
    u16        height;
    char       received_line[128];
    usize      received_line_len;
} TelnetServerArgs;

typedef struct {
    u16   port;
    char  received_bytes[128];
    usize received_len;
    bool  ready;
} RawTcpServerArgs;

typedef struct {
    char       url[64];
    Net_Result bind_result;
    Net_Result recv_results[3];
    char       received_chars[3];
} TelnetCharServerArgs;

internal void _nexus_make_telnet_test_url(char* out_url, usize out_url_size)
{
    u16 port = _nexus_choose_test_port(SOCK_STREAM, IPPROTO_TCP);
    snprintf(out_url, out_url_size, "tcp://127.0.0.1:%u", (unsigned)port);
}

internal void _nexus_make_telnet_udp_test_url(char* out_url, usize out_url_size)
{
    u16 port = _nexus_choose_test_port(SOCK_DGRAM, IPPROTO_UDP);
    snprintf(out_url, out_url_size, "udp://127.0.0.1:%u", (unsigned)port);
}

internal void _nexus_telnet_wait_for_server_start(void) { thread_sleep_ms(50); }

internal void* _nexus_telnet_echo_server(void* arg)
{
    TelnetServerArgs* args = arg;

    Net_Socket sock        = net_telnet_socket();
    args->bind_result      = net_bind(&sock, args->url);
    if (NET_FAILED(args->bind_result)) {
        return NULL;
    }

    Net_Message msg   = net_message_create(&sock);
    args->recv_result = net_recv(&msg);
    if (args->recv_result == NET_OK) {
        args->has_bounds = net_telnet_bounds(&msg, &args->width, &args->height);
        TEST_ASSERT_LT(msg.length, sizeof(args->received_line));
        memcpy(args->received_line, msg.data, msg.length);
        args->received_line[msg.length] = 0;
        args->received_line_len         = msg.length;

        if (args->echo_reply) {
            net_message_clear(&msg);
            net_message_append(&msg, "echo: ", 6);
            net_message_append(
                &msg, args->received_line, args->received_line_len);
            args->send_result = net_send(&msg);
        }
    }

    net_message_done(&msg);
    net_close(&sock);
    return NULL;
}

internal void* _nexus_raw_tcp_line_server(void* arg)
{
    RawTcpServerArgs* args = arg;

    _nexus_test_net_init();

    nexus_test_fd_t server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT(server_fd != NEXUS_TEST_INVALID_FD,
           "Failed to create raw TCP server socket");

    ASSERT(_nexus_test_setsockopt_reuseaddr(server_fd) == 0,
           "Failed to set SO_REUSEADDR on raw TCP server");

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
        .sin_port        = htons(args->port),
    };

    ASSERT(bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0,
           "Failed to bind raw TCP server");
    ASSERT(listen(server_fd, 1) == 0, "Failed to listen on raw TCP server");
    args->ready   = true;

    nexus_test_fd_t client_fd = accept(server_fd, NULL, NULL);
    ASSERT(client_fd != NEXUS_TEST_INVALID_FD,
           "Failed to accept raw TCP client");

    while (args->received_len < sizeof(args->received_bytes)) {
        nexus_test_ssize_t result =
            recv(client_fd,
                 args->received_bytes + args->received_len,
                 sizeof(args->received_bytes) - args->received_len,
                 0);
        ASSERT(result >= 0, "Failed to receive from raw TCP client");
        if (result == 0) {
            break;
        }

        args->received_len += (usize)result;
        if (memchr(args->received_bytes, '\n', args->received_len)) {
            break;
        }
    }

    _nexus_test_close_fd(client_fd);
    _nexus_test_close_fd(server_fd);
    return NULL;
}

internal void* _nexus_telnet_character_server(void* arg)
{
    TelnetCharServerArgs* args = arg;

    Net_Socket sock            = net_telnet_socket();
    TEST_ASSERT_EQ(
        net_set_option(&sock, NET_OPT_TELNET_MODE, NET_TELNET_CHARACTER_MODE),
        NET_OK);

    args->bind_result = net_bind(&sock, args->url);
    if (NET_FAILED(args->bind_result)) {
        return NULL;
    }

    Net_Message msg = net_message_create(&sock);
    for (usize i = 0; i < 3; ++i) {
        args->recv_results[i] = net_recv(&msg);
        if (NET_FAILED(args->recv_results[i])) {
            break;
        }

        TEST_ASSERT_EQ(msg.length, (usize)1);
        args->received_chars[i] = (char)msg.data[0];
    }

    net_message_done(&msg);
    net_close(&sock);
    return NULL;
}

internal void _nexus_telnet_expect_round_trip_after_immediate_close(void)
{
    TelnetServerArgs server_args = {
        .echo_reply = true,
    };
    _nexus_make_telnet_test_url(server_args.url, sizeof(server_args.url));

    Thread server_thread;
    TEST_ASSERT(
        thread_create(&server_thread, _nexus_telnet_echo_server, &server_args));

    _nexus_telnet_wait_for_server_start();

    Net_Socket client = net_telnet_socket();
    TEST_ASSERT_EQ(net_set_option(&client, NET_OPT_CONNECT_TIMEOUT_MS, 1000),
                   NET_OK);
    TEST_ASSERT_EQ(net_set_option(&client, NET_OPT_RECONNECT_INTERVAL_MS, 25),
                   NET_OK);
    TEST_ASSERT_EQ(net_connect(&client, server_args.url), NET_OK);

    Net_Message msg = net_message_create(&client);
    net_message_append(&msg, "hello from telnet", 17);
    TEST_ASSERT_EQ(net_send(&msg), NET_OK);
    TEST_ASSERT_EQ(net_recv(&msg), NET_OK);

    string reply = string_from(msg.data, msg.length);
    TEST_ASSERT(string_equals_cstr(reply, "echo: hello from telnet"));

    thread_join(&server_thread);

    TEST_ASSERT_EQ(server_args.bind_result, NET_OK);
    TEST_ASSERT_EQ(server_args.recv_result, NET_OK);
    TEST_ASSERT_EQ(server_args.send_result, NET_OK);
    TEST_ASSERT(string_equals(
        string_from(server_args.received_line, server_args.received_line_len),
        S("hello from telnet")));

    net_message_done(&msg);
    net_close(&client);
}

internal void _nexus_telnet_expect_close_after_final_reply(void)
{
    TelnetServerArgs server_args = {
        .echo_reply = true,
    };
    _nexus_make_telnet_test_url(server_args.url, sizeof(server_args.url));

    Thread server_thread;
    TEST_ASSERT(
        thread_create(&server_thread, _nexus_telnet_echo_server, &server_args));

    _nexus_telnet_wait_for_server_start();

    Net_Socket client = net_telnet_socket();
    TEST_ASSERT_EQ(net_set_option(&client, NET_OPT_CONNECT_TIMEOUT_MS, 1000),
                   NET_OK);
    TEST_ASSERT_EQ(net_set_option(&client, NET_OPT_RECONNECT_INTERVAL_MS, 25),
                   NET_OK);
    TEST_ASSERT_EQ(net_connect(&client, server_args.url), NET_OK);

    Net_Message msg = net_message_create(&client);
    net_message_append(&msg, "hello from telnet", 17);
    TEST_ASSERT_EQ(net_send(&msg), NET_OK);
    TEST_ASSERT_EQ(net_recv(&msg), NET_OK);

    string reply = string_from(msg.data, msg.length);
    TEST_ASSERT(string_equals_cstr(reply, "echo: hello from telnet"));

    thread_join(&server_thread);

    TEST_ASSERT_EQ(server_args.bind_result, NET_OK);
    TEST_ASSERT_EQ(server_args.recv_result, NET_OK);
    TEST_ASSERT_EQ(server_args.send_result, NET_OK);
    TEST_ASSERT_EQ(net_recv(&msg), NET_CLOSED);

    net_message_done(&msg);
    net_close(&client);
}

TEST_CASE(nexus, telnet_socket_round_trip_over_tcp)
{
    _nexus_telnet_expect_round_trip_after_immediate_close();
}

TEST_CASE(nexus, telnet_socket_round_trip_survives_immediate_close_repeatedly)
{
    for (usize i = 0; i < 20; ++i) {
        _nexus_telnet_expect_round_trip_after_immediate_close();
    }
}

TEST_CASE(nexus, telnet_socket_reports_closed_after_final_reply)
{
    _nexus_telnet_expect_close_after_final_reply();
}

TEST_CASE(nexus, telnet_socket_sends_crlf_terminated_lines)
{
    RawTcpServerArgs server_args = {
        .port = _nexus_choose_test_port(SOCK_STREAM, IPPROTO_TCP),
    };
    char url[64];
    snprintf(
        url, sizeof(url), "tcp://127.0.0.1:%u", (unsigned)server_args.port);

    Thread server_thread;
    TEST_ASSERT(thread_create(
        &server_thread, _nexus_raw_tcp_line_server, &server_args));

    while (!server_args.ready) {
        thread_sleep_ms(1);
    }

    Net_Socket client = net_telnet_socket();
    TEST_ASSERT_EQ(net_set_option(&client, NET_OPT_CONNECT_TIMEOUT_MS, 1000),
                   NET_OK);
    TEST_ASSERT_EQ(net_set_option(&client, NET_OPT_RECONNECT_INTERVAL_MS, 25),
                   NET_OK);
    TEST_ASSERT_EQ(net_connect(&client, url), NET_OK);

    Net_Message msg = net_message_create(&client);
    net_message_append(&msg, "plain line", 10);
    TEST_ASSERT_EQ(net_send(&msg), NET_OK);

    net_message_done(&msg);
    net_close(&client);
    thread_join(&server_thread);

    TEST_ASSERT_EQ(server_args.received_len, (usize)12);
    TEST_ASSERT_MEM_EQ(server_args.received_bytes, "plain line\r\n", 12);
}

TEST_CASE(nexus, telnet_character_mode_receives_one_character_per_message)
{
    TelnetCharServerArgs server_args = {0};
    _nexus_make_telnet_test_url(server_args.url, sizeof(server_args.url));

    Thread server_thread;
    TEST_ASSERT(thread_create(
        &server_thread, _nexus_telnet_character_server, &server_args));

    _nexus_telnet_wait_for_server_start();

    _nexus_test_net_init();
    nexus_test_fd_t client_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    TEST_ASSERT(client_fd != NEXUS_TEST_INVALID_FD);

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons((u16)atoi(strrchr(server_args.url, ':') + 1)),
    };
    TEST_ASSERT(_nexus_test_parse_loopback(&addr));
    TEST_ASSERT(connect(client_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0);

    u8    negotiation[12];
    usize received = 0;
    while (received < sizeof(negotiation)) {
        nexus_test_ssize_t result =
            recv(client_fd, negotiation + received, sizeof(negotiation) - received, 0);
        TEST_ASSERT(result > 0);
        received += (usize)result;
    }
    TEST_ASSERT_MEM_EQ(negotiation,
                       ((u8[]){
                           255,
                           253,
                           31,
                           255,
                           251,
                           1,
                           255,
                           251,
                           3,
                           255,
                           253,
                           3,
                       }),
                       sizeof(negotiation));

    const u8 payload[] = {'a', 'b', '\r', '\n'};
    TEST_ASSERT_EQ(send(client_fd, payload, sizeof(payload), 0),
                   (nexus_test_ssize_t)sizeof(payload));

    _nexus_test_close_fd(client_fd);
    thread_join(&server_thread);

    TEST_ASSERT_EQ(server_args.bind_result, NET_OK);
    TEST_ASSERT_EQ(server_args.recv_results[0], NET_OK);
    TEST_ASSERT_EQ(server_args.recv_results[1], NET_OK);
    TEST_ASSERT_EQ(server_args.recv_results[2], NET_OK);
    TEST_ASSERT_MEM_EQ(server_args.received_chars, "ab\n", 3);
}

TEST_CASE(nexus, telnet_character_mode_sends_raw_bytes_without_crlf)
{
    RawTcpServerArgs server_args = {
        .port = _nexus_choose_test_port(SOCK_STREAM, IPPROTO_TCP),
    };
    char url[64];
    snprintf(
        url, sizeof(url), "tcp://127.0.0.1:%u", (unsigned)server_args.port);

    Thread server_thread;
    TEST_ASSERT(thread_create(
        &server_thread, _nexus_raw_tcp_line_server, &server_args));

    while (!server_args.ready) {
        thread_sleep_ms(1);
    }

    Net_Socket client = net_telnet_socket();
    TEST_ASSERT_EQ(
        net_set_option(&client, NET_OPT_TELNET_MODE, NET_TELNET_CHARACTER_MODE),
        NET_OK);
    TEST_ASSERT_EQ(net_set_option(&client, NET_OPT_CONNECT_TIMEOUT_MS, 1000),
                   NET_OK);
    TEST_ASSERT_EQ(net_set_option(&client, NET_OPT_RECONNECT_INTERVAL_MS, 25),
                   NET_OK);
    TEST_ASSERT_EQ(net_connect(&client, url), NET_OK);

    Net_Message msg = net_message_create(&client);
    net_message_append(&msg, "xy", 2);
    TEST_ASSERT_EQ(net_send(&msg), NET_OK);

    net_message_done(&msg);
    net_close(&client);
    thread_join(&server_thread);

    TEST_ASSERT_EQ(server_args.received_len, (usize)2);
    TEST_ASSERT_MEM_EQ(server_args.received_bytes, "xy", 2);
}

TEST_CASE(nexus, telnet_socket_ignores_telnet_negotiation_bytes)
{
    TelnetServerArgs server_args = {0};
    _nexus_make_telnet_test_url(server_args.url, sizeof(server_args.url));

    Thread server_thread;
    TEST_ASSERT(
        thread_create(&server_thread, _nexus_telnet_echo_server, &server_args));

    _nexus_telnet_wait_for_server_start();

    _nexus_test_net_init();
    nexus_test_fd_t client_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    TEST_ASSERT(client_fd != NEXUS_TEST_INVALID_FD);

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons((u16)atoi(strrchr(server_args.url, ':') + 1)),
    };
    TEST_ASSERT(_nexus_test_parse_loopback(&addr));
    TEST_ASSERT(connect(client_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0);

    u8    do_naws[3];
    usize received = 0;
    while (received < sizeof(do_naws)) {
        nexus_test_ssize_t result =
            recv(client_fd, do_naws + received, sizeof(do_naws) - received, 0);
        TEST_ASSERT(result > 0);
        received += (usize)result;
    }
    TEST_ASSERT_MEM_EQ(do_naws, ((u8[]){255, 253, 31}), sizeof(do_naws));

    const u8 payload[] = {
        255,
        251,
        1,
        255,
        253,
        3,
        'h',
        'i',
        '\r',
        '\n',
    };
    TEST_ASSERT_EQ(send(client_fd, payload, sizeof(payload), 0),
                   (nexus_test_ssize_t)sizeof(payload));

    u8 response[6];
    received = 0;
    while (received < sizeof(response)) {
        nexus_test_ssize_t result = recv(
            client_fd, response + received, sizeof(response) - received, 0);
        TEST_ASSERT(result > 0);
        received += (usize)result;
    }

    _nexus_test_close_fd(client_fd);
    thread_join(&server_thread);

    TEST_ASSERT_EQ(server_args.bind_result, NET_OK);
    TEST_ASSERT_EQ(server_args.recv_result, NET_OK);
    TEST_ASSERT(string_equals(
        string_from(server_args.received_line, server_args.received_line_len),
        S("hi")));
    TEST_ASSERT_MEM_EQ(
        response, ((u8[]){255, 254, 1, 255, 252, 3}), sizeof(response));
}

TEST_CASE(nexus, telnet_socket_reports_negotiated_bounds_from_naws)
{
    TelnetServerArgs server_args = {0};
    _nexus_make_telnet_test_url(server_args.url, sizeof(server_args.url));

    Thread server_thread;
    TEST_ASSERT(
        thread_create(&server_thread, _nexus_telnet_echo_server, &server_args));

    _nexus_telnet_wait_for_server_start();

    _nexus_test_net_init();
    nexus_test_fd_t client_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    TEST_ASSERT(client_fd != NEXUS_TEST_INVALID_FD);

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons((u16)atoi(strrchr(server_args.url, ':') + 1)),
    };
    TEST_ASSERT(_nexus_test_parse_loopback(&addr));
    TEST_ASSERT(connect(client_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0);

    u8    do_naws[3];
    usize received = 0;
    while (received < sizeof(do_naws)) {
        nexus_test_ssize_t result =
            recv(client_fd, do_naws + received, sizeof(do_naws) - received, 0);
        TEST_ASSERT(result > 0);
        received += (usize)result;
    }
    TEST_ASSERT_MEM_EQ(do_naws, ((u8[]){255, 253, 31}), sizeof(do_naws));

    const u8 payload[] = {
        255,
        251,
        31,
        255,
        250,
        31,
        0,
        80,
        0,
        25,
        255,
        240,
        'o',
        'k',
        '\r',
        '\n',
    };
    TEST_ASSERT_EQ(send(client_fd, payload, sizeof(payload), 0),
                   (nexus_test_ssize_t)sizeof(payload));

    _nexus_test_close_fd(client_fd);
    thread_join(&server_thread);

    TEST_ASSERT_EQ(server_args.bind_result, NET_OK);
    TEST_ASSERT_EQ(server_args.recv_result, NET_OK);
    TEST_ASSERT(server_args.has_bounds);
    TEST_ASSERT_EQ(server_args.width, (u16)80);
    TEST_ASSERT_EQ(server_args.height, (u16)25);
    TEST_ASSERT(string_equals(
        string_from(server_args.received_line, server_args.received_line_len),
        S("ok")));
}

TEST_CASE(nexus, telnet_socket_rejects_udp_urls)
{
    char url[64];
    _nexus_make_telnet_udp_test_url(url, sizeof(url));

    Net_Socket sock = net_telnet_socket();
    TEST_ASSERT_EQ(net_bind(&sock, url), NET_PROTOCOL_NOT_SUPPORTED);
    TEST_ASSERT_EQ(net_connect(&sock, url), NET_PROTOCOL_NOT_SUPPORTED);
    net_close(&sock);
}

TEST_CASE(nexus, telnet_bounds_are_unavailable_on_non_telnet_sockets)
{
    u16 width         = 0;
    u16 height        = 0;

    Net_Socket  basic = net_socket();
    Net_Message msg   = net_message_create(&basic);

    TEST_ASSERT(!net_telnet_bounds(&msg, &width, &height));

    net_message_done(&msg);
    net_close(&basic);

    Net_Socket request = net_request_socket();
    msg                = net_message_create(&request);

    TEST_ASSERT(!net_telnet_bounds(&msg, &width, &height));

    net_message_done(&msg);
    net_close(&request);
}

TEST_CASE(nexus, telnet_mode_option_round_trips_and_rejects_non_telnet_sockets)
{
    u64 value         = 99;

    Net_Socket telnet = net_telnet_socket();
    TEST_ASSERT_EQ(net_get_option(&telnet, NET_OPT_TELNET_MODE, &value),
                   NET_OK);
    TEST_ASSERT_EQ(value, (u64)NET_TELNET_LINE_MODE);
    TEST_ASSERT_EQ(
        net_set_option(&telnet, NET_OPT_TELNET_MODE, NET_TELNET_CHARACTER_MODE),
        NET_OK);
    TEST_ASSERT_EQ(net_get_option(&telnet, NET_OPT_TELNET_MODE, &value),
                   NET_OK);
    TEST_ASSERT_EQ(value, (u64)NET_TELNET_CHARACTER_MODE);
    net_close(&telnet);

    Net_Socket basic = net_socket();
    TEST_ASSERT_EQ(
        net_set_option(&basic, NET_OPT_TELNET_MODE, NET_TELNET_CHARACTER_MODE),
        NET_PROTOCOL_NOT_SUPPORTED);
    TEST_ASSERT_EQ(net_get_option(&basic, NET_OPT_TELNET_MODE, &value),
                   NET_PROTOCOL_NOT_SUPPORTED);
    net_close(&basic);
}
