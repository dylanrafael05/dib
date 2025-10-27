#include "resources.h"
#include <iostream>

using namespace dib::resources;
namespace fs = std::filesystem;

int main()
{
    auto fn = fs::current_path() / "resource_test.dbatch";
    // ResourceBatch::create_from_directory(fs::current_path(), fn);
    // return 0;

    auto batch = ResourceBatch::open(fn);
    auto hello_world = batch.get("resource_test/hello_world.txt");
    auto other = batch.get("resource_test/other_file.txt");

    std::cout << hello_world.as_string() << std::endl;
    std::cout << other.as_string() << std::endl;
}