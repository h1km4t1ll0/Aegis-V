// g++ -static -O2 (19-gcc464 README, rung gcc4): std::cout, std::string,
// std::vector, an exception thrown two calls down and caught by a base
// class reference. Prints `hello from g++ 4.6.4`, or `g++ test FAIL WHAT`.
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

static int depth2(int n)
{
    if (n > 3)
        throw std::runtime_error("too deep");
    return n;
}

static int depth1(int n)
{
    return depth2(n + 1) + 1;
}

int main()
{
    std::vector<int> v;
    for (int i = 1; i <= 100; i++)
        v.push_back(i);
    long sum = 0;
    for (std::vector<int>::const_iterator it = v.begin(); it != v.end(); ++it)
        sum += *it;

    std::string what;
    try {
        depth1(1);
        depth1(5);
    } catch (const std::exception &e) {
        what = e.what();
    }

    std::string s = "hello from g++";
    s += ' ';
    s += __VERSION__;
    if (sum != 5050) {
        std::cout << "g++ test FAIL vector " << sum << std::endl;
        return 1;
    }
    if (what != "too deep") {
        std::cout << "g++ test FAIL exception '" << what << "'" << std::endl;
        return 1;
    }
    std::cout << s << std::endl;
    return 0;
}
