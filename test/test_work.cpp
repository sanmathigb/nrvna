#include "nrvna/work.hpp"
#include <filesystem>

int main() {
    nrvna::Work wrk("./test_tmp");
    auto id1 = wrk.submit("this is my test content1\n");
    auto id2 = wrk.submit("this is my test content2\n");
    auto id3 = wrk.submit("this is my test content3\n");
    //return (id1 != id2 && id1 != id3 && id2 != id3) ? 0 : 1;
    return 1;
}