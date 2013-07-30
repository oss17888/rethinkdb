// Copyright 2010-2013 RethinkDB, all rights reserved.
#ifndef EXTPROC_EXTPROC_WORKER_HPP_
#define EXTPROC_EXTPROC_WORKER_HPP_

#include <sys/types.h>
#include "arch/io/io_utils.hpp"
#include "concurrency/wait_any.hpp"
#include "concurrency/cross_thread_signal.hpp"
#include "containers/object_buffer.hpp"
#include "containers/scoped.hpp"
#include "containers/archive/socket_stream.hpp"

class extproc_spawner_t;

class extproc_worker_t {
public:
    extproc_worker_t(scoped_array_t<scoped_ptr_t<cross_thread_signal_t> > *_interruptors,
                     extproc_spawner_t *_spawner);
    ~extproc_worker_t();

    // Called whenever the worker changes hands (system -> user -> system)
    void acquired(signal_t *_user_interruptor);
    void released(); // Returns true if the worker process has failed

    // We accept jobs as functions that take a read stream and write stream
    //  so that they can communicate back to the job in the main process
    void run_job(bool (*fn) (read_stream_t *, write_stream_t *));

    read_stream_t *get_read_stream();
    write_stream_t *get_write_stream();

    static const uint64_t parent_to_worker_magic;
    static const uint64_t worker_to_parent_magic;

private:
    void spawn();
    void kill_process();

    // This will run inside the blocker pool so the worker process doesn't inherit any
    //  of our coroutine stuff
    void spawn_internal();

    extproc_spawner_t *spawner;
    pid_t worker_pid;
    scoped_fd_t socket;

    object_buffer_t<socket_stream_t> socket_stream;

    // Interruptors for the parent side
    scoped_array_t<scoped_ptr_t<cross_thread_signal_t> > *interruptors;
    signal_t *user_interruptor;

    // Used to combine the interruptors from the pool and the user
    object_buffer_t<wait_any_t> combined_interruptor;
};

#endif /* EXTPROC_EXTPROC_WORKER_HPP_ */
