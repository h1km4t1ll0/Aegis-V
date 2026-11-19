// g++ at -O2 with gcc9a's default -std=gnu++14 (20-gcc95 README, rung
// gcc9a): __cplusplus, auto and deduced return types, lambdas (a capturing
// one, a generic one, one as std::sort's comparator), std::unique_ptr with
// std::make_unique and a virtual call, std::map, range-for, a C++14
// constexpr function with a loop checked by static_assert, binary literals
// and digit separators, std::string, an exception. volatile inputs keep the
// work at run time. Prints `gcc9a sanity c++ ok`, or
// `gcc9a sanity FAIL WHAT` per failed check.
#include <algorithm>
#include <cstdio>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#if __cplusplus < 201402L
#error "not C++14"
#endif

static int fails;

static void check(bool ok, const char *what)
{
    if (!ok) {
        std::printf("gcc9a sanity FAIL %s\n", what);
        fails++;
    }
}

constexpr unsigned long long factorial(int n)
{
    unsigned long long r = 1;
    for (int i = 2; i <= n; ++i)
        r *= i;
    return r;
}
static_assert(factorial(20) == 2'432'902'008'176'640'000ULL, "constexpr loop");
static_assert(0b1010'0101 == 165, "binary literal");

struct shape {
    virtual ~shape() {}
    virtual long area() const = 0;
};

struct rect : shape {
    long w, h;
    rect(long w_, long h_) : w(w_), h(h_) {}
    long area() const override { return w * h; }
};

struct square : rect {
    explicit square(long s) : rect(s, s) {}
};

static auto twice(int x)
{
    return 2 * x;
}

int main()
{
    volatile int n = 50, k = 7;
    std::vector<int> v;
    for (int i = 0; i < n; ++i)
        v.push_back((i * 37 + k) % n);

    std::sort(v.begin(), v.end(), [](int a, int b) { return a > b; });
    check(std::is_sorted(v.rbegin(), v.rend()) && v.front() == n - 1 && v.back() == 0, "sort");

    int sum = 0;
    for (auto x : v)
        sum += x;
    check(sum == n * (n - 1) / 2, "range-for");

    int base = k;
    auto addk = [base](int x) { return x + base; };
    auto mul = [](auto a, auto b) { return a * b; };
    check(addk(35) == 42 && mul(6, 7) == 42 && mul(1.5, 4.0) == 6.0
          && mul(std::string("ab").size(), 3u) == 6u, "lambda");
    check(twice(21) == 42, "auto return");

    std::vector<std::unique_ptr<shape>> shapes;
    shapes.push_back(std::make_unique<rect>(3, k));
    shapes.push_back(std::make_unique<square>(n));
    long total = 0;
    for (const auto &s : shapes)
        total += s->area();
    check(total == 21 + 2500, "unique_ptr");

    std::map<std::string, int> count;
    const char *words[] = { "gcc", "tilck", "gcc", "musl", "gcc", "tilck" };
    for (auto w : words)
        ++count[w];
    std::string keys;
    for (const auto &p : count)
        keys += p.first + "=" + std::to_string(p.second) + ";";
    check(keys == "gcc=3;musl=1;tilck=2;", "map");

    volatile unsigned long long f = factorial(n / 5 + 10);
    check(f == 2432902008176640000ULL, "constexpr");

    std::string what;
    try {
        if (n > 10)
            throw std::out_of_range("n too big");
    } catch (const std::logic_error &e) {
        what = e.what();
    }
    check(what == "n too big", "exception");

    if (!fails)
        std::printf("gcc9a sanity c++ ok\n");
    return fails;
}
