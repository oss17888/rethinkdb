// Copyright 2010-2013 RethinkDB, all rights reserved.
#ifndef EXTPROC_EXTPROC_SPAWNER_HPP_
#define EXTPROC_EXTPROC_SPAWNER_HPP_

#include <sys/types.h>
#include "arch/io/io_utils.hpp"
#include "arch/types.hpp"

// The extproc_spawner_t controls an external process which launches workers
// This is necessary to avoid some forking problems with tcmalloc, and
//  should be constructed before the thread pool and extproc pool
class extproc_spawner_t {
public:
    extproc_spawner_t();
    ~extproc_spawner_t();

    // Spawns a new worker, and returns the socket file descriptor for communication
    //  with the worker process
    fd_t spawn();

    static extproc_spawner_t *get_instance();

private:
    void fork_spawner();

    static extproc_spawner_t *instance;

    pid_t spawner_pid;
    scoped_fd_t spawner_socket;
};

#endif /* EXTPROC_EXTPROC_SPAWNER_HPP_ */
