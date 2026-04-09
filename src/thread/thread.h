//------------------------------------------------------------------------------
// Threads and channels module
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------
//> use: core
//> def: _POSIX_C_SOURCE=199309L

#pragma once

#include <core/core.h>

//------------------------------------------------------------------------------

#if OS_WINDOWS
typedef HANDLE Thread;
#else
typedef pthread_t Thread;
#endif

typedef _Atomic bool CancelToken;

//------------------------------------------------------------------------------
// Thread API

bool thread_create(Thread* thread, void* (*func)(void*), void* arg);
void thread_join(Thread* thread);
void thread_yield();
void thread_sleep_ms(u32 milliseconds);

Thread thread_this();
void   thread_cancel(CancelToken* token);
bool   thread_is_cancelled(CancelToken* token);

//------------------------------------------------------------------------------
// Channel API
//
// A channel allows asynchronous communication between threads. It supports both
// SPSC (Single Producer Single Consumer) and MPSC (Multiple Producer Single
// Consumer) modes. The channel starts in SPSC mode and automatically switches
// to MPSC mode when the TX handle is cloned.
//
// Channels use a ring buffer internally to store data. One byte is reserved so
// that read_pos == write_pos always means "empty", so the usable capacity is
// buffer_size - 1 bytes.
//
// Despite being a ring-buffer, when data of a certain size is sent and peeked,
// it is guaranteed that the data will be contiguous in memory.
//
// The channel uses reference counting for TX and RX handles. All handles must
// be marked as done before the channel itself can be destroyed.
//

typedef struct {
    u8*   buffer;
    usize buffer_size;

    _Atomic usize read_pos;
    _Atomic usize write_pos;

    // Reference counting for safe cleanup
    _Atomic u32 tx_ref_count;
    _Atomic u32 rx_ref_count;

    // MPSC support (minimal overhead when tx_ref_count == 1)
    _Atomic bool tx_lock;
} Channel;

typedef struct {
    Channel* channel;
} Channel_Tx;

typedef struct {
    Channel* channel;
} Channel_Rx;

//
// Initialisation and clean-up
//

void channel_init(Channel*    channel,
                  void*       buffer,
                  usize       buffer_size,
                  Channel_Tx* out_tx,
                  Channel_Rx* out_rx);
void channel_done(Channel* channel);

void channel_tx_clone(Channel_Tx* src, Channel_Tx* out_clone);

void channel_tx_done(Channel_Tx* tx);
void channel_rx_done(Channel_Rx* rx);

//
// Transmit operations (producer side)
//
// Data is copied into the channel's internal buffer, blocking if there is not
// enough room.
//

void channel_send_data(Channel_Tx* tx, const u8* data, usize length);
void channel_send_string(Channel_Tx* tx, string str);

//
// Receive operations (consumer side)
//
// Peeked data is a reference into the channel's internal buffer, returning
// false if there is not enough data.
//
// Consume advances the read position in the channel's internal buffer. If the
// size of the data to consume is bigger than the channel's buffer, the function
// will abort. Consumption invalidates any previously peeked data as there is a
// possibility of it being overridden.
//
// The `get` functions will wait until the data is available, but still will not
// consume the data.
//

// Wait for data of the given length to arrive.  Exit with true when data has
// arrived, or false if cancelled.
bool channel_wait_data(Channel_Rx* rx, usize length, CancelToken* cancel_token);

// Check to see if enough data to fill a buffer has arrived, and if so, return a
// pointer to the data, or NULL if it hasn't.
u8* channel_peek_data(Channel_Rx* rx, usize length);

// Wait for data to arrive of a given size and return a pointer to it when it
// does.  However, exit with NULL if the given cancel token (optional) is
// triggered.
u8* channel_get_data(Channel_Rx* rx, usize length, CancelToken* cancel_token);

// Fetch a string from a channel passed to it via `channel_send_string`.  If
// cancelled, returns a string whose data field is null.
string channel_get_string(Channel_Rx* rx, CancelToken* cancel_token);

// Mark the following number of bytes in the channel as not required any more.
void channel_consume_data(Channel_Rx* rx, usize length);

// Mark the following string in the channel as not required any more.  Pass the
// string returned from `channel_peek_string` or `channel_get_string` to it.
void channel_consume_string(Channel_Rx* rx, string str);

#define CHANNEL_GET(rx, type, cancel_token)                                    \
    channel_get_data(rx, sizeof(type), cancel_token)
#define CHANNEL_CONSUME(rx, type) channel_consume_data(rx, sizeof(type))

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
