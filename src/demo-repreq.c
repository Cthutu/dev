//
// demo-repreq.c
//
// Single-process request/reply demo
//
//> use: core nexus thread

#include <nexus/nexus.h>
#include <thread/thread.h>

typedef struct {
    cstr       url;
    Net_Result bind_result;
    Net_Result recv_result;
    Net_Result send_result;
} Demo_RepReq_Server;

internal void* _demo_repreq_server(void* arg)
{
    Demo_RepReq_Server* server = arg;

    //
    // Create a reply socket to receive requests
    //

    Net_Socket sock            = net_reply_socket();
    server->bind_result        = net_bind(&sock, server->url);
    if (NET_FAILED(server->bind_result)) {
        return NULL;
    }

    prn("Reply socket bound to %s", server->url);

    //
    // Create a message to hold the request and receive it
    //

    Net_Message msg     = net_message_create(&sock);
    server->recv_result = net_recv(&msg);
    if (NET_FAILED(server->recv_result)) {
        net_message_done(&msg);
        net_close(&sock);
        return NULL;
    }

    //
    // Decode the message
    //

    string request_text;
    u32    request_id = 0;
    if (!net_message_read_string(&msg, &request_text) ||
        !net_message_read_u32(&msg, &request_id)) {
        kill("Reply socket failed to decode request");
    }

    prn("Reply socket received request: " STRINGP " (%u)",
        STRINGV(request_text),
        request_id);

    //
    // Reuse the message for the pong reply
    //

    net_message_clear(&msg);
    net_message_append_string(&msg, S("pong"));
    net_message_append_u32(&msg, request_id + 1);

    server->send_result = net_send(&msg);

    //
    // Clean up and exit thread
    //

    net_message_done(&msg);
    net_close(&sock);
    return NULL;
}

int run(int argc, char* argv[])
{
    UNUSED(argc);
    UNUSED(argv);

    prn("Request/reply demo running...");

    Demo_RepReq_Server server = {
        .url = "tcp://127.0.0.1:8081",
    };

    //
    // Create a thread for the "server" that will open a reply socket
    //

    Thread server_thread;
    if (!thread_create(&server_thread, _demo_repreq_server, &server)) {
        kill("Failed to create reply thread");
    }

    //
    // Create a request socket to test the req/reply connection
    //
    // We set the timeout to 2 seconds and reconnect interval to 25ms so that we
    // can reconnect client if the server isn't ready yet.
    //

    Net_Socket client = net_request_socket();
    Net_Result result =
        net_set_option(&client, NET_OPT_CONNECT_TIMEOUT_MS, 2000);
    if (NET_FAILED(result)) {
        kill("Failed to set connect timeout: %s", net_result_string(result));
    }

    result = net_set_option(&client, NET_OPT_RECONNECT_INTERVAL_MS, 25);
    if (NET_FAILED(result)) {
        kill("Failed to set reconnect interval: %s", net_result_string(result));
    }

    //
    // Attempt to connect to the server
    //

    result = net_connect(&client, server.url);
    if (NET_FAILED(result)) {
        kill("Request socket failed to connect: %s", net_result_string(result));
    }

    prn("Request socket connected to %s", server.url);

    //
    // Create a ping request message and send it
    //

    Net_Message msg = net_message_create(&client);
    net_message_append_string(&msg, S("ping"));
    net_message_append_u32(&msg, 1);

    result = net_send(&msg);
    if (NET_FAILED(result)) {
        kill("Request socket failed to send: %s", net_result_string(result));
    }

    prn("Request socket sent request");

    //
    // Wait for and receive the pong reply
    //

    result = net_recv(&msg);
    if (NET_FAILED(result)) {
        kill("Request socket failed to receive reply: %s",
             net_result_string(result));
    }

    string reply_text;
    u32    reply_id = 0;
    if (!net_message_read_string(&msg, &reply_text) ||
        !net_message_read_u32(&msg, &reply_id)) {
        kill("Request socket failed to decode reply");
    }

    prn("Request socket received reply: " STRINGP " (%u)",
        STRINGV(reply_text),
        reply_id);

    //
    // Clean up and wait for server thread to finish
    //

    net_message_done(&msg);
    net_close(&client);

    thread_join(&server_thread);

    //
    // Report any server problems
    //

    if (NET_FAILED(server.bind_result)) {
        kill("Reply socket failed to bind: %s",
             net_result_string(server.bind_result));
    }
    if (NET_FAILED(server.recv_result)) {
        kill("Reply socket failed to receive: %s",
             net_result_string(server.recv_result));
    }
    if (NET_FAILED(server.send_result)) {
        kill("Reply socket failed to send: %s",
             net_result_string(server.send_result));
    }

    prn("Request/reply demo complete.");
    return 0;
}
