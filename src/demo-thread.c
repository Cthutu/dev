//------------------------------------------------------------------------------
// Demo for showing threads
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------
//> use: thread

#include <thread/thread.h>

//------------------------------------------------------------------------------

typedef struct {
    Channel_Rx  channel;
    CancelToken cancel_token;
} WorkerArgs;

void* worker_thread(void* arg)
{
    WorkerArgs* args  = arg;
    usize       count = 0;

    while (!thread_is_cancelled(&args->cancel_token)) {
        string msg = channel_get_string(&args->channel, &args->cancel_token);
        if (msg.data) {
            prn("[%u] Worker received message: " STRINGP,
                ++count,
                STRINGV(msg));
            channel_consume_string(&args->channel, msg);
        }
    }
}

int run(int argc, char** argv)
{
    UNUSED(argc);
    UNUSED(argv);

    WorkerArgs args = {0};
    Channel    channel;
    Channel_Tx tx;
    u8         channel_buffer[1024];

    channel_init(
        &channel, channel_buffer, sizeof(channel_buffer), &tx, &args.channel);

    Thread thread;
    if (!thread_create(&thread, worker_thread, &args)) {
        kill("Failed to create worker thread");
    }

    int count = 5000;
    for (int i = 0; i < count; i++) {
        channel_send_string(&tx, S("Hello from the main thread!"));
        channel_send_string(
            &tx, S("This is a demo of thread communication using channels."));
    }

    thread_sleep_ms(1000); // Give the worker some time to process the message
    thread_cancel(&args.cancel_token);
    thread_join(&thread);

    channel_tx_done(&tx);
    channel_rx_done(&args.channel);
    channel_done(&channel);

    return 0;
}
