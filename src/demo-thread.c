//------------------------------------------------------------------------------
// Demo for showing threads
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------
//> use: thread

#include <thread/thread.h>

//------------------------------------------------------------------------------

typedef struct {
    Thread thread;
    u32    number;
    u32    delay;
} WorkerArgs;

void* worker_thread(void* arg)
{
    WorkerArgs* args = arg;
    prn("Hello from the worker thread %u!", args->number);
    thread_sleep_ms(args->delay);
    prn("Worker thread %u done!", args->number);

    return nullptr;
}

int run(int argc, char** argv)
{
    UNUSED(argc);
    UNUSED(argv);

    WorkerArgs args[] = {
        {.thread = 0, .number = 1, .delay = 2000},
        {.thread = 0, .number = 2, .delay = 1500},
    };

    prn("Hello from the main thread!");

    for (size_t i = 0; i < sizeof(args) / sizeof(args[0]); i++) {
        thread_create(&args[i].thread, worker_thread, &args[i]);
    }

    for (size_t i = 0; i < sizeof(args) / sizeof(args[0]); i++) {
        thread_join(&args[i].thread);
    }

    return 0;
}
