// Copyright 2010-2013 RethinkDB, all rights reserved.
#ifndef EXTPROC_EXTPROC_JOB_HPP_
#define EXTPROC_EXTPROC_JOB_HPP_

#include <exception>

#include "utils.hpp"
#include "containers/archive/archive.hpp"
#include "concurrency/signal.hpp"

class extproc_pool_t;
class extproc_worker_t;

class extproc_job_t : public home_thread_mixin_t {
public:
    extproc_job_t(extproc_pool_t *_pool,
                  bool (*worker_fn) (read_stream_t *, write_stream_t *),
                  signal_t *interruptor);
    virtual ~extproc_job_t();

protected:
    // All data written and read by the user must be accounted for, or things will break
    read_stream_t *read_stream();
    write_stream_t *write_stream();

private:
    extproc_pool_t *pool;
    size_t worker_index;
    extproc_worker_t *worker;
};

#endif /* EXTPROC_EXTPROC_JOB_HPP_ */
