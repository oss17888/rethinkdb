#include "rdb_protocol/minidriver.hpp"

namespace ql {

const reql reql::r;

void test(env_t& env){
  reql::var a(env);
  reql x = r.array(1,2,3,4).map(r.fun(a, a + 1)).count();
}

}
