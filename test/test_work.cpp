#include "nrvna/work.hpp"
#include <filesystem>

int main() {
    nrvna::Work work("./test_tmp");
    auto id1 = work.submit("this is my test content1\n");
    auto id2 = work.submit("this is my test content2\n");
    auto id3 = work.submit("this is my test content3\n");

    //std::filesystem::remove_all("./test_tmp");
    return (id1 != id2 && id1 != id3 && id2 != id3) ? 0 : 1;
}