#ifndef RDB_PROTOCOL_MINI_DRIVER_HPP_
#define RDB_PROTOCOL_MINI_DRIVER_HPP_

#if 0
#define MINIDRIVER_DEBUG(code) code
#include <iostream>
#else 
#define MINIDRIVER_DEBUG(ignore)
#endif

#include <string>
#include <vector>

#include "rdb_protocol/env.hpp"
#include "rdb_protocol/ql2.pb.h"
#include "rdb_protocol/datum.hpp"
#include "rdb_protocol/counted_term.hpp"

namespace ql {

/** fill_vector(vector, elements...)
 * 
 * push_back each element onto the vector
 *
 **/

template<typename E>
inline void fill_vector(std::vector<E>&) { }

template<typename E, typename H, typename ... T>
inline void fill_vector(std::vector<E>& v, H x, T ... xs){
    v.push_back(std::forward<H>(x));
    fill_vector(v, std::forward<T>(xs) ...);
}

/** reql_t
 * 
 * A thin wrapper around scoped_ptr_t<Term> that allows building Terms
 * using the ReQL syntax.
 *
 * Move semantics are used to avoid copying the inner Term.
 *
 * - Non-const *this is treated like an rvalue reference. Store reql_t
 *   values in const variables or use .copy() if you use the same
 *   reql_t more than once. GCC < 4.8 has no support for explicit
 *   rvalue references for *this, so they are commented out.
 *
 **/

class reql_t {
private:
    class pvar;
    friend class pvar;
public:
    
    typedef const pvar var_t;
    
    static const reql_t r;

    typedef std::pair<std::string, reql_t> key_value;

    reql_t(scoped_ptr_t<Term>&& term_) : term(std::move(term_)) { }

    reql_t(const double val) { set_datum(datum_t(val)); }
    reql_t(const std::string& val) { set_datum(datum_t(val)); }
    reql_t(const datum_t &d) { set_datum(d); }
    
    static reql_t boolean(bool b){
        return reql_t(datum_t(datum_t::R_BOOL, b));
    }

    static reql_t expr(const datum_t& d){
        return reql_t(d);
    }

    static reql_t expr(const Datum& d){
        reql_t ret = make_scoped<Term>(); 
        ret.term->set_type(Term::DATUM);
        *ret.term->mutable_datum() = d;
        return ret;
    }

    static reql_t expr(const Term& t){
        return reql_t(make_scoped<Term>(t));
    }

    reql_t(std::vector<reql_t>&& val) : term(make_scoped<Term>()) {
        term->set_type(Term::MAKE_ARRAY);
        for (auto i = val.begin(); i != val.end(); i++) {
            add_arg(std::move(*i));
        }
    }

    reql_t(const reql_t& other) = delete;

    reql_t(reql_t&& other) : term(std::move(other.term)) { }

    reql_t& operator= (const reql_t& other) = delete;

    reql_t& operator= (reql_t&& other){
        term.swap(other.term);
        return *this;
    }

    static reql_t fun(reql_t&& body);
    static reql_t fun(const pvar& a, reql_t&& body);
    static reql_t fun(const pvar& a, const pvar& b, reql_t&& body);

    template<typename ... T>
    static reql_t array(T ... a){
        std::vector<reql_t> v;
        fill_vector(v, std::forward<T>(a) ...);
        return std::move(v);
    }

    static reql_t null() {
        auto t = make_scoped<Term>();
        t->set_type(Term::DATUM);
        t->mutable_datum()->set_type(Datum::R_NULL);
        return reql_t(std::move(t));
    }

    template <typename ... T>
    reql_t call(Term_TermType type, T ... args) const /* & */ {
        MINIDRIVER_DEBUG(std::cout << "Copying *this for const call\n";)
        return copy().call(type, std::forward<T>(args) ...);
    }

    template <typename ... T>
    reql_t call(Term_TermType type, T ... args) /* && */ {
        MINIDRIVER_DEBUG({
                std::cout << "Calling " << type << " on *this = ";
                if (term.has()) {
                    term->PrintDebugString();
                } else {
                    std::cout << "NULL\n";
                }
            })
        reql_t ret(make_scoped<Term>());
        ret.term->set_type(type);
        if (term.has()) {
            ret.add_arg(std::move(*this));
        }
        ret.add_args(std::forward<T>(args) ...);
        MINIDRIVER_DEBUG({
                std::cout << "Result of call: ";
                ret.term->PrintDebugString();
            })
        return ret;
    }

    key_value optarg(const std::string& key, reql_t&& value){
        return key_value(key, std::move(value));
    }

    reql_t copy() const {
        reql_t ret;
        MINIDRIVER_DEBUG({
                std::cout << "copying this ";
                if (term.has()) {
                    term->PrintDebugString();
                } else {
                    std::cout << "NULL\n";
                }
            })
        if (term.has()) {
            ret.term = make_scoped<Term>(*term);
        }
        return ret;
    }

    Term* release(){
        return term.release();
    }

    Term& get(){
        return *term;
    }

    protob_t<Term> release_counted(){
        protob_t<Term> ret = make_counted_term();
        auto t = scoped_ptr_t<Term>(term.release());
        ret->Swap(t.get());
        return ret;
    }

    void swap(Term& t){
        t.Swap(term.get());
    }

#define REQL_METHOD(name, termtype)                             \
    template<typename ... T>                                    \
    reql_t name(T ... a)                                        \
    { return call(Term::termtype, std::forward<T>(a) ...); }    \
    template<typename ... T>                                    \
    reql_t name(T ... a) const                                  \
    { return call(Term::termtype, std::forward<T>(a) ...); }

    REQL_METHOD(operator+, ADD);
    REQL_METHOD(operator==, EQ);
    REQL_METHOD(operator(), FUNCALL);
    REQL_METHOD(operator>=, GE);
    REQL_METHOD(operator<=, LE);
    REQL_METHOD(operator&&, ALL);
    REQL_METHOD(count, COUNT);
    REQL_METHOD(map, MAP);

    reql_t operator[] (const std::string& key) {
        return call(Term::GET_FIELD, key);
    }

    reql_t operator[] (const std::string& key) const {
        return copy().call(Term::GET_FIELD, key);
    }

    reql_t operator[] (double key) {
        return call(Term::NTH, key);
    }

    reql_t operator[] (double key) const {
        return copy().call(Term::NTH, key);
    }


#undef REQL_METHOD

private:

    void set_datum(const datum_t &d) {
        term = make_scoped<Term>(); 
        term->set_type(Term::DATUM);
        d.write_to_protobuf(term->mutable_datum());
    }

    void add_args(){ };

    template <typename A, typename ... T>
    void add_args(A a, T ... args){
        add_arg(std::forward<A>(a));
        add_args(std::forward<T>(args) ...);
    }
     
    template<typename T>
    void add_arg(T a){
        reql_t it(std::forward<T>(a));
        MINIDRIVER_DEBUG({
                std::cout << "Adding arg ";
                it.term->PrintDebugString();
            })
        term->mutable_args()->AddAllocated(it.term.release()); 
    }

    reql_t() : term(NULL) { }

    scoped_ptr_t<Term> term;
};

class reql_t::pvar : public reql_t {
public:
    int id;
    pvar(env_t& env) : reql_t(), id(env.gensym()) {
       term = r.call(Term::VAR, static_cast<double>(id)).term;
    }
    pvar(int id_) : reql_t(), id(id_) {
       term = r.call(Term::VAR, static_cast<double>(id)).term;
    }
    pvar(const pvar& var) : reql_t(var.copy()), id(var.id) { }
};

template <>
inline void reql_t::add_arg(key_value&& kv){
    auto ap = make_scoped<Term_AssocPair>();        
    ap->set_key(kv.first);
    ap->mutable_val()->Swap(kv.second.term.get());
    MINIDRIVER_DEBUG({
       std::cout << "Adding optarg ";
       ap->PrintDebugString();
    })
    term->mutable_optargs()->AddAllocated(ap.release());
}

extern const reql_t& r;

} // namespace ql

#endif // RDB_PROTOCOL_MINI_DRIVER_HPP_
