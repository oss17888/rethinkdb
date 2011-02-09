#include "btree/slice.hpp"
#include "btree/node.hpp"
#include "buffer_cache/transactor.hpp"
#include "buffer_cache/buf_lock.hpp"
#include "concurrency/cond_var.hpp"
#include "btree/get.hpp"
#include "btree/rget.hpp"
#include "btree/set.hpp"
#include "btree/incr_decr.hpp"
#include "btree/append_prepend.hpp"
#include "btree/delete.hpp"
#include "btree/get_cas.hpp"
#include "replication/masterstore.hpp"
#include <boost/scoped_ptr.hpp>

void btree_slice_t::create(translator_serializer_t *serializer,
                           mirrored_cache_config_t *config) {
    /* Put slice in a scoped pointer because it's way to big to allocate on a coroutine stack */
    boost::scoped_ptr<btree_slice_t> slice(new btree_slice_t(serializer, config));

    /* Initialize the root block */
    transactor_t transactor(&slice->cache_, rwi_write);
    buf_lock_t superblock(transactor, SUPERBLOCK_ID, rwi_write);
    btree_superblock_t *sb = (btree_superblock_t*)(superblock.buf()->get_data_write());
    sb->magic = btree_superblock_t::expected_magic;
    sb->root_block = NULL_BLOCK_ID;

    // Destructors handle cleanup and stuff
}

btree_slice_t::btree_slice_t(translator_serializer_t *serializer,
                             mirrored_cache_config_t *config)
    : cache_(serializer, config) {
    // Start up cache
    struct : public cache_t::ready_callback_t, public cond_t {
        void on_cache_ready() { pulse(); }
    } ready_cb;
    if (!cache_.start(&ready_cb)) ready_cb.wait();
}

btree_slice_t::~btree_slice_t() {
    // Shut down cache
    struct : public cache_t::shutdown_callback_t, public cond_t {
        void on_cache_shutdown() { pulse(); }
    } shutdown_cb;
    if (!cache_.shutdown(&shutdown_cb)) shutdown_cb.wait();
}

store_t::get_result_t btree_slice_t::get(store_key_t *key) {
    return btree_get(key, this);
}

store_t::get_result_t btree_slice_t::get_cas(store_key_t *key, castime_t castime) {
    return btree_get_cas(key, this, castime);
}

store_t::rget_result_ptr_t btree_slice_t::rget(store_key_t *start, store_key_t *end, bool left_open, bool right_open) {
    return btree_rget_slice(this, start, end, left_open, right_open);
}

store_t::set_result_t btree_slice_t::sarc(store_key_t *key, data_provider_t *data, mcflags_t flags, exptime_t exptime, castime_t castime,
                                          add_policy_t add_policy, replace_policy_t replace_policy, cas_t old_cas) {
    return btree_set(key, this, data, flags, exptime, add_policy, replace_policy, old_cas, castime);
}

store_t::incr_decr_result_t btree_slice_t::incr_decr(incr_decr_kind_t kind, store_key_t *key, uint64_t amount, castime_t castime) {
    return btree_incr_decr(key, this, kind == incr_decr_INCR, amount, castime);
}

store_t::append_prepend_result_t btree_slice_t::append_prepend(append_prepend_kind_t kind, store_key_t *key, data_provider_t *data, castime_t castime) {
    return btree_append_prepend(key, this, data, kind == append_prepend_APPEND, castime);
}

store_t::delete_result_t btree_slice_t::delete_key(store_key_t *key, repli_timestamp timestamp) {
    return btree_delete(key, this, timestamp);
}
