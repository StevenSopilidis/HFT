#include "socket_utils.h"

int main() {
    using namespace Common;
    std::cout << getIfaceIP("lo") << std::endl;
}