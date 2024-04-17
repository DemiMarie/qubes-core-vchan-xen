/*
 * The Qubes OS Project, http://www.qubes-os.org
 *
 * Copyright (C) 2010  Rafal Wojtczuk  <rafal@invisiblethingslab.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#ifndef LIBVCHAN_H
#define LIBVCHAN_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
typedef int EVTCHN;

/* config vchan features */
#ifdef CONFIG_STUBDOM
#define ASYNC_INIT
#endif /* CONFIG_STUBDOM */

/* return values from libvchan_is_open */
/* remote disconnected or remote domain dead */
#define VCHAN_DISCONNECTED 0
/* connected */
#define VCHAN_CONNECTED 1
/* vchan initialized, waiting for client to connect, or server to appear */
#define VCHAN_WAITING 2

/** Opaque type corresponding to a vchan */
struct libvchan;

/** Typedef for \ref libvchan */
typedef struct libvchan libvchan_t;

/** Listen for a vchan connection as a server.
 *
 * \param domain The peer's Xen domain ID.  Must not be negative.
 * \param port The port number.  Should not be negative.
 * \param read_min The minimum allowed read buffer size.  The actual buffer allocated might be bigger than this.
 * \parma write_min The minimum allowed write buffer size.  The actual buffer allocated might be bigger than this.
 * \return An allocated vchan on success, or NULL on failure.
 */
libvchan_t *libvchan_server_init(int domain, int port, size_t read_min, size_t write_min);

/**
 * Connect to a vchan.  The peer must have called libvchan_server_init() already.
 *
 * \param domain The peer's Xen domain ID.  Must not be negative.
 * \param port The port used in the libvchan_server_init() call.  Should not be negative.
 * \return An allocated vchan on success, or NULL on failure.
 */
libvchan_t *libvchan_client_init(int domain, int port);

/* An alternative path for client connection:
 * 1. Call libvchan_client_init_async().
 * 2. Wait for watch_fd to become readable.
 * 3. When readable, call libvchan_client_init_async_finish().
 *
 * Repeat steps 2-3 until libvchan_client_init_async_finish returns 0. Abort on
 * negative values (error).
 * If connection attempt failed or should be aborted, call libvchan_close() to
 * clean up.
 */
__attribute__((access(write_only, 3, 1)))
libvchan_t *libvchan_client_init_async(int domain, int port, EVTCHN *watch_fd);
int libvchan_client_init_async_finish(libvchan_t *ctrl, bool blocking);

/**
 * Write data to a vchan. Partial writes might happen.
 *
 * This function is realtime-safe provided that either:
 *
 * 1. The vchan is in non-blocking mode.
 * 2. There is enough space in the buffer (as reported by libvchan_buffer_space())
 *    and the peer is not malicious.
 *
 * \param ctrl The vchan to act on.
 * \param[in] data The data to write.
 * \param size The number of bytes to write.
 * \return The number of bytes written on success, or a negative number on error.
 * The number of bytes written might be less than \ref size.
 */
__attribute__((access(read_only, 2, 3)))
int libvchan_write(libvchan_t *ctrl, const void *data, size_t size);

/**
 * Write data to a vchan atomically. Partial writes are not allowed.
 *
 * This function is realtime-safe provided that either:
 *
 * 1. The vchan is in non-blocking mode.
 * 2. There is enough space in the buffer (as reported by libvchan_buffer_space())
 *    and the peer is not malicious.
 *
 * \param ctrl The vchan to act on.
 * \param[in] The data to write.
 * \param size The number of bytes to write.
 * \return size on success, 0 if the vchan is in
 *         non-blocking mode and there is not enough buffer space, or -1 on error.
 */
__attribute__((access(read_only, 2, 3)))
int libvchan_send(libvchan_t *ctrl, const void *data, size_t size);

/**
 * Write data to a vchan. Partial reads might happen.
 *
 * This function is realtime-safe provided that either:
 *
 * 1. The vchan is in non-blocking mode.
 * 2. There is enough data in the buffer (as reported by libvchan_data_ready())
 *    and the peer is not malicious.
 *
 * \param ctrl The vchan to act on.
 * \param[in] data The data to read.
 * \param size The number of bytes to read.
 * \return The number of bytes written on success, or a negative number on error.
 * The number of bytes written might be less than \ref size, and could even be 0.
 */
__attribute__((access(write_only, 2, 3)))
int libvchan_read(libvchan_t *ctrl, void *data, size_t size);

/**
 * Write data to a vchan atomically. Partial reads are not allowed.
 *
 * This function is realtime-safe provided that either:
 *
 * 1. The vchan is in non-blocking mode.
 * 2. There is enough data in the buffer (as reported by libvchan_data_ready())
 *    and the peer is not malicious.
 *
 * \param ctrl The vchan to act on.
 * \param[in] The data to read.
 * \param size The number of bytes to read.
 * \return size on success, 0 if the vchan is in
 *         non-blocking mode and there is not enough buffer space, or -1 on error.
 */
__attribute__((access(write_only, 2, 3)))
int libvchan_recv(libvchan_t *ctrl, void *data, size_t size);

/**
 * Acknowledge an event on the vchan.  This must be called *immediately* after
 * poll() or epoll() has reported that the fd returned by libvchan_fd_for_select()
 * is ready for reading, before performing any I/O on the vchan.  It must not be
 * called more than once in that situation, as it may block indefinitely
 *
 * This function is realtime-safe provided that readiness has been reported on the FD.
 *
 * \param ctrl The vchan to act on.
 */
int libvchan_wait(libvchan_t *ctrl);

/**
 * Close the vchan and free the underlying resources.
 *
 * \param[in] ctrl The vchan to close.  Must not be NULL.
 */
void libvchan_close(libvchan_t *ctrl);

/**
 * Obtain a file descriptor usable in poll() or epoll().
 * The returned FD must not be used for any purpose other than waiting for
 * it to be ready for reading. All events are reported via a readable
 * FD, even if the event is that data can be written to the vchan.
 *
 * This function is realtime-safe.
 *
 * \param ctrl The vchan to get the FD from.
 * \return A pollable FD, or -1 on failure.
 */
EVTCHN libvchan_fd_for_select(libvchan_t *ctrl);

/**
 * Check if a vchan is open.  The first call is not RT-safe as
 * xc_evtchn_status() allocates memory internally.  Subsequent
 * calls are RT-safe.
 *
 * \param ctrl The vchan.
 * \return \ref VCHAN_WAITING if we are waiting for the peer to connect,
 *         \ref VCHAN_CONNECTED if the vchan is connected, or
 *         \ref VCHAN_DISCONNECTED otherwise.
 */
int libvchan_is_open(libvchan_t *ctrl);

/**
 * Report the amount of data ready to be read.  Realtime-safe.
 *
 * \param ctrl The vchan.
 * \return The amount of data ready, which might be 0 but is never negative.
 */
int libvchan_data_ready(libvchan_t *ctrl);

/*
 * Report the amount of data that can be written right now.  Realtime-safe.
 *
 * \param ctrl The vchan.
 * \return The amount of data that can be written, which might be 0 but is never negative.
 */
int libvchan_buffer_space(libvchan_t *ctrl);

/**
 * Set the blocking status of a vchan. Must be called only after successful
 * libvchan_*_init(). When using libvchan_client_init_async(), prefer using
 * blocking parameter to libvchan_client_init_async_finish() instead.
 *
 * \param ctrl The vchan.
 * \param blocking True to place the vchan in blocking mode, false otherwise.
 */
void libvchan_set_blocking(libvchan_t *ctrl, bool blocking);

#endif /* LIBVCHAN_H */
