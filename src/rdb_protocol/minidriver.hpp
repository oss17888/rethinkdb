#ifndef RDB_PROTOCOL_MINI_DRIVER_HPP_
#define RDB_PROTOCOL_MINI_DRIVER_HPP_

#include <string>
#include <vector>

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

class reql {
public:
    static const reql r;

    typedef std::pair<std::string, reql> key_value;

    reql(scoped_ptr_t<Term>&& term_, size_t next_var_id_) :
        term(std::move(term_)), next_var_id(next_var_id_) { }
 
    reql(const double val) : next_var_id(0) { set_datum(datum_t(val)); }
    reql(const std::string& val) : next_var_id(0) { set_datum(datum_t(val)); }
    reql(const datum_t &d) : next_var_id(0) { set_datum(d); }
    
    static reql boolean(bool b){
        return reql(datum_t(datum_t::R_BOOL, b));
    }

    reql(std::vector<reql>&& val) : next_var_id(0) {
        for (auto i = val.begin(); i != val.end(); i++) {
            next_var_id = std::max(next_var_id, i->next_var_id);
            add_arg(std::move(*i));
        }
    }

    reql(const reql& other) = delete;
    // : term(new Term(*other.term)), next_var_id(other.next_var_id) { }

    reql(reql&& other) : term(std::move(other.term)), next_var_id(other.next_var_id) { }

    
    reql& operator= (const reql& other) = delete; /* {
        term = make_scoped<Term>(*other.term);
        next_var_id = other.next_var_id;
        return *this;
    } */

    reql& operator= (reql&& other){
        term.swap(other.term);
        next_var_id = other.next_var_id;
        return *this;
    }

    template <typename T>
    reql gensym(std::vector<reql>& args){
        next_var_id ++;
        args.emplace_back(next_var_id);
        return r.var(next_var_id);
    }

    static reql var(size_t var_id) {
        auto t = make_scoped<Term>();
        t->set_type(Term::VAR);
        t->mutable_args()->AddAllocated(reql(static_cast<double>(var_id)).term.release());
        return reql(std::move(t), var_id + 1);
    }

    template <typename ... T>
    reql(std::function<reql(T ...)> f){
        term = make_scoped<Term>();
        term->set_type(Term::FUNC);
        std::vector<reql> args;
        reql body = f(gensym<T>(args) ...);
        term->mutable_args()->AddAllocated(reql(std::move(args)).term.release());
        term->mutable_args()->AddAllocated(body.term.release());
        next_var_id = std::max(next_var_id, body.next_var_id);
    }

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
        return reql(std::move(t), 0);
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
        ret.next_var_id = next_var_id;
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

    reql map(std::function<reql(reql)> f){
        return call(Term::MAP, f);
    }

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
        next_var_id = std::max(next_var_id, it.next_var_id);
        term->mutable_args()->AddAllocated(it.term.release()); 
    }

    scoped_ptr_t<Term> term;
    reql() : term(nullptr), next_var_id(0) { }
    size_t next_var_id;
};

template <>
void reql::add_arg(key_value&& kv){
    next_var_id = std::max(next_var_id, kv.second.next_var_id);
    auto ap = make_scoped<Term_AssocPair>();        
    ap->set_key(kv.first);
    ap->mutable_val()->MergeFrom(*kv.second.term);
    term->mutable_optargs()->AddAllocated(ap.release());
}

} // namespace ql

#endif // RDB_PROTOCOL_MINI_DRIVER_HPP_
