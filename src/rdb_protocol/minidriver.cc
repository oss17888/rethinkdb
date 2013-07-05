#include "rdb_protocol/minidriver.hpp"

namespace ql {

const reql_t& r = reql_t::r;

reql_t reql_t::fun(reql_t&& body){
    return reql_t::r.call(Term::FUNC, r.array(), std::move(body));
}

reql_t reql_t::fun(const reql_t::pvar& a, reql_t&& body){
    std::vector<reql_t> v;
    v.emplace_back(static_cast<double>(a.id));
    return reql_t::r.call(Term::FUNC, std::move(v), std::move(body));
}

reql_t reql_t::fun(const reql_t::pvar& a, const reql_t::pvar& b, reql_t&& body){
    std::vector<reql_t> v;
    v.emplace_back(static_cast<double>(a.id));
    v.emplace_back(static_cast<double>(b.id));
    return reql_t::r.call(Term::FUNC, std::move(v), std::move(body));
}

const reql_t reql_t::r;

}
