// Copyright 2010-2012 RethinkDB, all rights reserved.
#include "unittest/mock_file.hpp"

#include <sys/uio.h>
#include <functional>

#include "arch/io/disk.hpp"
#include "arch/runtime/coroutines.hpp"
#include "unittest/gtest.hpp"

namespace unittest {

mock_file_t::mock_file_t(mode_t mode, std::vector<char> *data) : mode_(mode), data_(data) {
    guarantee(mode != 0);
    guarantee(data_ != NULL);
}
mock_file_t::~mock_file_t() { }

uint64_t mock_file_t::get_size() { return data_->size(); }
void mock_file_t::set_size(size_t size) { data_->resize(size, 0); }
void mock_file_t::set_size_at_least(size_t size) {
    if (data_->size() < size) {
        data_->resize(size, 0);
    }
}

void mock_file_t::read_async(size_t offset, size_t length, void *buf,
                             UNUSED file_account_t *account, linux_iocallback_t *cb) {
    read_blocking(offset, length, buf);
    // TODO: This is to silence the serializer disk_structure.cc reader_t
    // use-after-free bug: https://github.com/rethinkdb/rethinkdb/issues/738
    coro_t::spawn_sometime(std::bind(&linux_iocallback_t::on_io_complete, cb));
}

void mock_file_t::write_async(size_t offset, size_t length, const void *buf,
                              UNUSED file_account_t *account, linux_iocallback_t *cb,
                              UNUSED wrap_in_datasyncs_t wrap_in_datasyncs) {
    write_blocking(offset, length, buf);
    // TODO: This is to silence the serializer disk_structure.cc reader_t
    // use-after-free bug: https://github.com/rethinkdb/rethinkdb/issues/738
    coro_t::spawn_sometime(std::bind(&linux_iocallback_t::on_io_complete, cb));
}

void mock_file_t::writev_async(size_t offset, size_t length, scoped_array_t<iovec> &&bufs,
                               file_account_t *account, linux_iocallback_t *cb) {
    scoped_array_t<char> buf(length);

    iovec bufvec[1] = { { buf.data(), length } };
    fill_bufs_from_source(bufvec, 1, bufs.data(), bufs.size(), 0);

    write_async(offset, length, buf.data(), account, cb, NO_DATASYNCS);
}

void mock_file_t::read_blocking(size_t offset, size_t length, void *buf) {
    guarantee(mode_ & mode_read);
    verify_aligned_file_access(data_->size(), offset, length, buf);
    guarantee(!(offset > SIZE_MAX - length || offset + length > data_->size()));
    memcpy(buf, data_->data() + offset, length);
}

void mock_file_t::write_blocking(size_t offset, size_t length, const void *buf) {
    guarantee(mode_ & mode_write);
    verify_aligned_file_access(data_->size(), offset, length, buf);
    guarantee(!(offset > SIZE_MAX - length || offset + length > data_->size()));
    memcpy(data_->data() + offset, buf, length);
}

bool mock_file_t::coop_lock_and_check() {
    // We don't actually implement the locking behavior.
    return true;
}

size_t mock_semantic_checking_file_t::semantic_blocking_read(void *buf, size_t length) {
    size_t length_to_read = std::min(data_->size() - pos_, length);
    memcpy(buf, data_->data() + pos_, length_to_read);
    pos_ += length_to_read;
    return length_to_read;
}

size_t mock_semantic_checking_file_t::semantic_blocking_write(const void *buf, size_t length) {
    if (data_->size() < pos_ + length) {
        data_->resize(pos_ + length);
    }
    memcpy(data_->data() + pos_, buf, length);
    pos_ += length;
    return length;
}


std::string mock_file_opener_t::file_name() const {
    return "<mock file>";
}

void mock_file_opener_t::open_serializer_file_create_temporary(scoped_ptr_t<file_t> *file_out) {
    ASSERT_EQ(no_file, file_existence_state_);
    file_out->init(new mock_file_t(mock_file_t::mode_rw, &file_));
    file_existence_state_ = temporary_file;
}

void mock_file_opener_t::move_serializer_file_to_permanent_location() {
    ASSERT_EQ(temporary_file, file_existence_state_);
    file_existence_state_ = permanent_file;
}

void mock_file_opener_t::open_serializer_file_existing(scoped_ptr_t<file_t> *file_out) {
    ASSERT_TRUE(file_existence_state_ == temporary_file || file_existence_state_ == permanent_file);
    file_out->init(new mock_file_t(mock_file_t::mode_rw, &file_));
}

void mock_file_opener_t::unlink_serializer_file() {
    ASSERT_TRUE(file_existence_state_ == temporary_file || file_existence_state_ == permanent_file);
    file_existence_state_ = unlinked_file;
}

#ifdef SEMANTIC_SERIALIZER_CHECK
void mock_file_opener_t::open_semantic_checking_file(scoped_ptr_t<semantic_checking_file_t> *file_out) {
    file_out->init(new mock_semantic_checking_file_t(&semantic_checking_file_));
}
#endif

}  // namespace unittest
