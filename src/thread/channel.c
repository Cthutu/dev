//------------------------------------------------------------------------------
// Channel API implementation
//
// Copyright (C)2026 Matt Davies, all rights reserved
//------------------------------------------------------------------------------

#include <thread/thread.h>

#include <stdatomic.h>

//------------------------------------------------------------------------------
// channel_init

void channel_init(Channel*    channel,
                  void*       buffer,
                  usize       buffer_size,
                  Channel_Tx* out_tx,
                  Channel_Rx* out_rx)
{
    channel->buffer      = (u8*)buffer;
    channel->buffer_size = buffer_size;

    atomic_store_explicit(&channel->read_pos, 0, memory_order_relaxed);
    atomic_store_explicit(&channel->write_pos, 0, memory_order_relaxed);

    atomic_store_explicit(&channel->tx_ref_count, 1, memory_order_relaxed);
    atomic_store_explicit(&channel->rx_ref_count, 1, memory_order_relaxed);

    atomic_store_explicit(&channel->tx_lock, false, memory_order_relaxed);

    out_tx->channel = channel;
    out_rx->channel = channel;
}

//------------------------------------------------------------------------------
// channel_done

void channel_done(Channel* channel)
{
#if DEBUG
    // In debug builds, verify that all TX and RX handles have been released
    u32 tx_refs =
        atomic_load_explicit(&channel->tx_ref_count, memory_order_relaxed);
    u32 rx_refs =
        atomic_load_explicit(&channel->rx_ref_count, memory_order_relaxed);

    ASSERT(tx_refs == 0 && rx_refs == 0,
           "Channel destroyed with active handles (TX: %u, RX: %u)",
           tx_refs,
           rx_refs);
#else
    UNUSED(channel);
#endif
}

//------------------------------------------------------------------------------
// channel_tx_clone

void channel_tx_clone(Channel_Tx* src, Channel_Tx* out_clone)
{
    atomic_fetch_add_explicit(
        &src->channel->tx_ref_count, 1, memory_order_relaxed);
    out_clone->channel = src->channel;
}

//------------------------------------------------------------------------------
// channel_tx_done

void channel_tx_done(Channel_Tx* tx)
{
    ASSERT(tx->channel != NULL, "TX handle already marked as done");
    atomic_fetch_sub_explicit(
        &tx->channel->tx_ref_count, 1, memory_order_relaxed);
    tx->channel = NULL;
}

//------------------------------------------------------------------------------
// channel_rx_done

void channel_rx_done(Channel_Rx* rx)
{
    ASSERT(rx->channel != NULL, "RX handle already marked as done");
    atomic_fetch_sub_explicit(
        &rx->channel->rx_ref_count, 1, memory_order_relaxed);
    rx->channel = NULL;
}

//------------------------------------------------------------------------------
// _channel_tx_lock

internal void cs_channel_tx_lock(Channel* channel)
{
    // Fast path: if ref_count == 1, we're still SPSC - no lock needed
    if (atomic_load_explicit(&channel->tx_ref_count, memory_order_relaxed) ==
        1) {
        return;
    }

    // MPSC mode: acquire spinlock
    // Short spin, then yield to avoid burning CPU
    u32  spin_count = 0;
    bool expected   = false;

    while (!atomic_compare_exchange_weak_explicit(&channel->tx_lock,
                                                  &expected,
                                                  true,
                                                  memory_order_acquire,
                                                  memory_order_relaxed)) {
        expected = false;

        if (++spin_count > 100) {
            thread_yield();
            spin_count = 0;
        }
    }
}

//------------------------------------------------------------------------------
// _channel_tx_unlock

internal void cs_channel_tx_unlock(Channel* channel)
{
    // Fast path: if ref_count == 1, we're still SPSC - no lock needed
    if (atomic_load_explicit(&channel->tx_ref_count, memory_order_relaxed) ==
        1) {
        return;
    }

    atomic_store_explicit(&channel->tx_lock, false, memory_order_release);
}

//------------------------------------------------------------------------------
// _channel_available_read

internal usize cs_channel_available_read(Channel* channel)
{
    usize read_pos =
        atomic_load_explicit(&channel->read_pos, memory_order_acquire);
    usize write_pos =
        atomic_load_explicit(&channel->write_pos, memory_order_acquire);

    if (write_pos >= read_pos) {
        return write_pos - read_pos;
    } else {
        return channel->buffer_size - (read_pos - write_pos);
    }
}

//------------------------------------------------------------------------------
// _channel_available_write

internal usize cs_channel_available_write(Channel* channel)
{
    usize read_pos =
        atomic_load_explicit(&channel->read_pos, memory_order_acquire);
    usize write_pos =
        atomic_load_explicit(&channel->write_pos, memory_order_acquire);

    if (write_pos >= read_pos) {
        return channel->buffer_size - (write_pos - read_pos) - 1;
    } else {
        return read_pos - write_pos - 1;
    }
}

//------------------------------------------------------------------------------
// channel_send_data

void channel_send_data(Channel_Tx* tx, const u8* data, usize length)
{
    Channel* channel = tx->channel;

    ASSERT(length > 0, "Cannot send zero-length data");
    ASSERT(length < channel->buffer_size, "Data too large for channel buffer");

    cs_channel_tx_lock(channel);

    // Wait until we have enough contiguous space or can wrap around
    while (true) {
        usize available = cs_channel_available_write(channel);
        if (available < length) {
            thread_yield();
            continue;
        }

        // Check if we need to wrap around for contiguous write
        usize write_pos =
            atomic_load_explicit(&channel->write_pos, memory_order_relaxed);
        usize space_to_end = channel->buffer_size - write_pos;

        if (space_to_end < length) {
            // Need to wrap - wait until we can write at the beginning
            usize read_pos =
                atomic_load_explicit(&channel->read_pos, memory_order_acquire);

            if (read_pos <= length) {
                // Not enough space at the beginning yet
                thread_yield();
                continue;
            }

            // Write at the beginning of the buffer
            memcpy(channel->buffer, data, length);
            write_pos = length;
        } else {
            // Contiguous space available
            memcpy(channel->buffer + write_pos, data, length);
            write_pos += length;

            if (write_pos == channel->buffer_size) {
                write_pos = 0;
            }
        }

        // Publish the new write position
        atomic_store_explicit(
            &channel->write_pos, write_pos, memory_order_release);
        break;
    }

    cs_channel_tx_unlock(channel);
}

//------------------------------------------------------------------------------
// channel_send_string

void channel_send_string(Channel_Tx* tx, string str)
{
    // Send length first, then the string data
    channel_send_data(tx, (const u8*)&str.count, sizeof(str.count));
    if (str.count > 0) {
        channel_send_data(tx, str.data, str.count);
    }
}

//------------------------------------------------------------------------------
// channel_wait_data

bool channel_wait_data(Channel_Rx* rx, usize length, CancelToken* cancel_token)
{
    Channel* channel = rx->channel;

    while (cs_channel_available_read(channel) < length) {
        if (cancel_token && thread_is_cancelled(cancel_token)) {
            return false;
        }
        thread_yield();
    }

    return true;
}

//------------------------------------------------------------------------------
// channel_peek_data

u8* channel_peek_data(Channel_Rx* rx, usize length)
{
    Channel* channel = rx->channel;

    ASSERT(length > 0, "Cannot peek zero-length data");
    ASSERT(length < channel->buffer_size,
           "Peek size too large for channel buffer");

    // Wait until we have enough data available
    if (cs_channel_available_read(channel) < length) {
        return NULL;
    }

    usize read_pos =
        atomic_load_explicit(&channel->read_pos, memory_order_acquire);
    usize space_to_end = channel->buffer_size - read_pos;

    u8* data;

    if (space_to_end < length) {
        // Data wraps around, wait for it to be at the beginning. The sender
        // guarantees contiguous data, so if we don't have enough space to the
        // end, the data must be at the beginning.
        data = channel->buffer;
    } else {
        data = channel->buffer + read_pos;
    }

    return data;
}

//------------------------------------------------------------------------------
// channel_get_data

u8* channel_get_data(Channel_Rx* rx, usize length, CancelToken* cancel_token)
{
    if (!channel_wait_data(rx, length, cancel_token)) {
        return NULL;
    }

    u8* data = channel_peek_data(rx, length);
    VERIFY(data, "Data should be available after wait");
    return data;
}

//------------------------------------------------------------------------------
// channel_get_string

string channel_get_string(Channel_Rx* rx, CancelToken* cancel_token)
{
    string str;

    // Peek the length first
    u8* length_ptr = channel_get_data(rx, sizeof(str.count), cancel_token);
    if (!length_ptr) {
        return (string){0};
    }

    usize length = *(usize*)length_ptr;
    channel_consume_data(rx, sizeof(str.count));
    if (length == 0) {
        return (string){0};
    }

    // Peek the string data
    u8* data_ptr = channel_get_data(rx, length, cancel_token);
    if (!data_ptr) {
        return (string){0};
    }

    str.data  = (u8*)data_ptr;
    str.count = length;

    return str;
}

//------------------------------------------------------------------------------
// channel_consume_data

void channel_consume_data(Channel_Rx* rx, usize length)
{
    if (length == 0) {
        return;
    }

    Channel* channel = rx->channel;

    ASSERT(length < channel->buffer_size,
           "Consume size too large for channel buffer");

    usize read_pos =
        atomic_load_explicit(&channel->read_pos, memory_order_relaxed);
    usize space_to_end = channel->buffer_size - read_pos;

    if (space_to_end < length) {
        // Data was wrapped, advance from the beginning
        read_pos = length;
    } else {
        read_pos += length;
        if (read_pos == channel->buffer_size) {
            read_pos = 0;
        }
    }

    // Publish the new read position
    atomic_store_explicit(&channel->read_pos, read_pos, memory_order_release);
}

//------------------------------------------------------------------------------
// channel_consume_string

void channel_consume_string(Channel_Rx* rx, string str)
{
    // Consume the string data.  We don't consume the length field as that is
    // already consumed when we got the string.  Only the string part is not
    // consumed when we call `channel_get_string`.
    channel_consume_data(rx, str.count);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
