#include <iostream>
#include <stdexcept>
#include <vector>
int main()
{
    std::vector<int> values{6, 7};
    try { throw std::runtime_error("ok"); }
    catch (const std::runtime_error&) {
        if (values[0] * values[1] != 42) return 1;
    }
    std::cout << "hello from g++ on Linux" << std::endl;
}
