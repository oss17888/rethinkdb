#ifndef RDB_PROTOCOL_MINI_DRIVER_HPP_
#define RDB_PROTOCOL_MINI_DRIVER_HPP_

#include <string>
#include <vector>

#include "rdb_protocol/env.hpp"
#include "rdb_protocol/ql2.pb.h"
#include "rdb_protocol/datum.hpp"

namespace ql {

template<typename E>
void fill_vector(std::vector<E>&) { }

template<typename E, typename H, typename ... T>
void fill_vector(std::vector<E>& v, H x, T ... xs){
    v.push_back(std::forward<H>(x));
    fill_vector(v, std::forward<T>(xs) ...);
}

class reql;

class reql {
private:

    class pvar;
    friend class pvar;

public:
    static const reql r;

    typedef std::pair<std::string, reql> key_value;

    reql(scoped_ptr_t<Term>&& term_) :
        term(std::move(term_)) { }
 
    reql(const double val) { set_datum(datum_t(val)); }
    reql(const std::string& val) { set_datum(datum_t(val)); }
    reql(const datum_t &d) { set_datum(d); }
    
    static reql boolean(bool b){
        return reql(datum_t(datum_t::R_BOOL, b));
    }

    reql(std::vector<reql>&& val){
        for (auto i = val.begin(); i != val.end(); i++) {
            add_arg(std::move(*i));
        }
    }

    reql(const reql& other) = delete;
    // : term(new Term(*other.term)) { }

    reql(reql&& other) : term(std::move(other.term)) { }

    reql& operator= (const reql& other) = delete; /* {
        term = make_scoped<Term>(*other.term);
        return *this;
    } */

    reql& operator= (reql&& other){
        term.swap(other.term);
        return *this;
    }

    typedef const pvar var;
    
    static reql fun(const var& a, reql&& body);
    static reql fun(const var& a, const var& b, reql&& body);

    template<typename ... T>
    static reql array(T ... a){
        std::vector<reql> v;
        fill_vector(v, std::forward<T>(a) ...);
        return std::move(v);
    }

    reql null() {
        auto t = make_scoped<Term>();
        t->set_type(Term::DATUM);
        t->mutable_datum()->set_type(Datum::R_NULL);
        return reql(std::move(t));
    }

    template <typename ... T>
    reql call(Term_TermType type, T ... args) const /* & */ {
        return copy().call(type, std::forward<T>(args) ...);
    }

    template <typename ... T>
    reql call(Term_TermType type, T ... args) /* && */ {
        reql ret;
        ret.term->set_type(type);
        if (term.has()) {
            ret.add_arg(std::move(*this));
        }
        ret.add_args(std::forward<T>(args) ...);
        return ret;
    }

    key_value optarg(const std::string& key, reql&& value){
        return key_value(key, std::move(value));
    }

    reql copy() const {
        reql ret;
        ret.term = make_scoped<Term>(*term);
        return ret;
    }

#define REQL_METHOD(name, termtype)                             \
    template<typename ... T>                                    \
    reql name(T ... a)                                          \
    { return call(Term::termtype, std::forward<T>(a) ...); }    \
    template<typename ... T>                                    \
    reql name(T ... a) const                                    \
    { return call(Term::termtype, std::forward<T>(a) ...); }

    REQL_METHOD(operator+, ADD);
    REQL_METHOD(count, COUNT);
    REQL_METHOD(map, MAP);

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
        reql it(std::forward<T>(a));
        term->mutable_args()->AddAllocated(it.term.release()); 
    }

    reql() : term(NULL) { }

    scoped_ptr_t<Term> term;
};

const reql& r = reql::r;


template <>
void reql::add_arg(key_value&& kv){
    auto ap = make_scoped<Term_AssocPair>();        
    ap->set_key(kv.first);
    ap->mutable_val()->CopyFrom(*kv.second.term);
    term->mutable_optargs()->AddAllocated(ap.release());
}

class reql::pvar : public reql {
public:
    int id;
    pvar(env_t& env) : reql(), id(env.gensym()) {
       term = r.call(Term::VAR, static_cast<double>(id)).term;
    }
    pvar(const pvar& var) : reql(var.copy()), id(var.id) { }
};

reql reql::fun(const reql::var& a, reql&& body){
    std::vector<reql> v;
    v.emplace_back(static_cast<double>(a.id));
    return reql::r.call(Term::FUNC, std::move(v), std::move(body));
}

reql reql::fun(const reql::var& a, const reql::var& b, reql&& body){
    std::vector<reql> v;
    v.emplace_back(static_cast<double>(a.id));
    v.emplace_back(static_cast<double>(b.id));
    return reql::r.call(Term::FUNC, std::move(v), std::move(body));
}

} // namespace ql

#endif // RDB_PROTOCOL_MINI_DRIVER_HPP_
