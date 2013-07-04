#include <rdb_protocol/minidriver.hpp>

namespace ql {

const reql reql::r;

const reql& r = reql::r;

void test(){
  reql x = r.array(1,2,3,4).map([](reql a){ return a + 1; });
}

}
