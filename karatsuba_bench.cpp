// Empirical comparison of four multiplication strategies, written to a CSV file for Excel.
//
// Build: g++ -std=c++17 -O2 -o karatsuba_bench karatsuba_bench.cpp
// Run:   ./karatsuba_bench   (prompts for hybrid cutoff, digit counts and output file)
//
// 1. Native `*`           : built-in unsigned __int128 product (GCC/Clang extension).
//                           Only possible while both numbers fit in 64 bits (<= 19 digits).
// 2. Schoolbook           : hand-written O(n^2) digit-by-digit multiplication.
// 3. Pure Karatsuba       : recursion all the way down to single digits.
// 4. Hybrid Karatsuba     : Karatsuba above a cutoff, schoolbook at or below it.

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <random>
#include <string>
#include <utility>
#include <vector>

// Big non-negative integer: base-10 digits, least-significant digit first.
using Big = std::vector<int>;
using u128 = unsigned __int128;

constexpr size_t NATIVE_MAX_DIGITS = 19;  // 10^19 - 1 < 2^64, product < 10^38 < 2^128

// ---------------------------------------------------------------- helpers

void trim(Big& a) 
{
    while (a.size() > 1 && a.back() == 0) a.pop_back();
}

Big add(const Big& a, const Big& b) 
{
    Big r;
    int carry = 0;
    for (size_t i = 0; i < std::max(a.size(), b.size()) || carry; ++i) 
    {
        int s = carry + (i < a.size() ? a[i] : 0) + (i < b.size() ? b[i] : 0);
        r.push_back(s % 10);
        carry = s / 10;
    }
    if (r.empty()) r.push_back(0);
    return r;
}

// Requires a >= b numerically.
Big sub(const Big& a, const Big& b) 
{
    Big r = a;
    int borrow = 0;
    for (size_t i = 0; i < r.size(); ++i) 
    {
        int d = r[i] - borrow - (i < b.size() ? b[i] : 0);
        borrow = d < 0;
        r[i] = d + (borrow ? 10 : 0);
    }
    trim(r);
    return r;
}

Big shift(const Big& a, size_t k) // multiply by 10^k
{  
    if (a.size() == 1 && a[0] == 0) return a;
    Big r(a.size() + k, 0);
    std::copy(a.begin(), a.end(), r.begin() + k);
    return r;
}

Big low(const Big& a, size_t m) 
{
    Big r(a.begin(), a.begin() + std::min(m, a.size()));
    if (r.empty()) r.push_back(0);
    trim(r);
    return r;
}

Big high(const Big& a, size_t m) 
{
    Big r = a.size() > m ? Big(a.begin() + m, a.end()) : Big{0};
    trim(r);
    return r;
}

// Random number with exactly n digits (leading digit is never 0).
Big randomBig(size_t n, std::mt19937_64& rng) 
{
    Big r(n);
    for (size_t i = 0; i < n; ++i) r[i] = static_cast<int>(rng() % 10);
    if (r.back() == 0) r.back() = 1 + static_cast<int>(rng() % 9);
    return r;
}

uint64_t toU64(const Big& a) 
{
    uint64_t v = 0;
    for (auto it = a.rbegin(); it != a.rend(); ++it) v = v * 10 + *it;
    return v;
}

Big fromU128(u128 v) 
{
    Big r;
    while (v > 0) {
        r.push_back(static_cast<int>(v % 10));
        v /= 10;
    }
    if (r.empty()) r.push_back(0);
    return r;
}

// ------------------------------------------------------------- algorithms

// 2. Schoolbook: O(n^2).
Big schoolbook(const Big& a, const Big& b) 
{
    std::vector<long long> t(a.size() + b.size(), 0);
    for (size_t i = 0; i < a.size(); ++i)
        for (size_t j = 0; j < b.size(); ++j) t[i + j] += a[i] * b[j];
    Big r;
    long long carry = 0;
    for (long long v : t) 
    {
        v += carry;
        r.push_back(static_cast<int>(v % 10));
        carry = v / 10;
    }
    trim(r);
    return r;
}

// 3. Pure Karatsuba: the only base case is a single digit times a single digit.
Big karatsubaPure(const Big& a, const Big& b) 
{
    size_t n = std::max(a.size(), b.size());
    if (n == 1) 
    {
        int p = a[0] * b[0];
        return p < 10 ? Big{p} : Big{p % 10, p / 10};
    }

    size_t m = n / 2;
    Big a0 = low(a, m), a1 = high(a, m);
    Big b0 = low(b, m), b1 = high(b, m);

    Big z0 = karatsubaPure(a0, b0);
    Big z2 = karatsubaPure(a1, b1);
    Big z1 = karatsubaPure(add(a0, a1), add(b0, b1));
    z1 = sub(sub(z1, z0), z2);

    Big r = add(add(shift(z2, 2 * m), shift(z1, m)), z0);
    trim(r);
    return r;
}

// 4. Hybrid: identical recursion, but stops at `threshold` digits and hands off to schoolbook.
Big karatsubaHybrid(const Big& a, const Big& b, size_t threshold) 
{
    size_t n = std::max(a.size(), b.size());
    if (n <= threshold) return schoolbook(a, b);

    size_t m = n / 2;
    Big a0 = low(a, m), a1 = high(a, m);
    Big b0 = low(b, m), b1 = high(b, m);

    Big z0 = karatsubaHybrid(a0, b0, threshold);
    Big z2 = karatsubaHybrid(a1, b1, threshold);
    Big z1 = karatsubaHybrid(add(a0, a1), add(b0, b1), threshold);
    z1 = sub(sub(z1, z0), z2);

    Big r = add(add(shift(z2, 2 * m), shift(z1, m)), z0);
    trim(r);
    return r;
}

// ----------------------------------------------------------------- timing

using Clock = std::chrono::steady_clock;
static volatile unsigned long long sink;  // stops the optimizer from discarding results

struct Timing 
{
    double ns;                  // average nanoseconds per call
    unsigned long long iters;   // how many calls the average is over
};

// Calls f() repeatedly, doubling the count until the batch takes >= ~100 ms.
template <class F>
Timing timeIt(F&& f) 
{
    const double targetNs = 1e8;
    unsigned long long iters = 1;
    while (true) 
    {
        auto t0 = Clock::now();
        for (unsigned long long i = 0; i < iters; ++i) f();
        auto t1 = Clock::now();
        double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
        if (ns >= targetNs || iters >= (1ULL << 32)) return {ns / iters, iters};
        iters *= 2;
    }
}

// -------------------------------------------------------------- measuring

struct Measurement 
{
    size_t digits = 0;
    bool nativeRan = false;
    double nativeNs = 0, schoolNs = 0, pureNs = 0, hybridNs = 0;  // average ns per multiplication
    bool correct = true;
};

// Generates two random n-digit numbers and times all four algorithms on them.
Measurement measure(size_t n, size_t threshold, std::mt19937_64& rng) 
{
    Measurement m;
    m.digits = n;
    Big a = randomBig(n, rng), b = randomBig(n, rng);
    Big ref, res;

    // Schoolbook doubles as the reference result for correctness checks.
    Timing t = timeIt([&] { ref = schoolbook(a, b); sink = ref[0]; });
    m.schoolNs = t.ns;

    if (n <= NATIVE_MAX_DIGITS) 
    {
        uint64_t a64 = toU64(a), b64 = toU64(b);
        u128 p = 0;
        // volatile reads force the multiply to be redone on every call
        t = timeIt([&] 
            {
            volatile uint64_t va = a64, vb = b64;
            p = static_cast<u128>(va) * static_cast<u128>(vb);
            sink = static_cast<unsigned long long>(p);
        });
        m.nativeRan = true;
        m.nativeNs = t.ns;
        if (fromU128(p) != ref) m.correct = false;
    }

    t = timeIt([&] { res = karatsubaPure(a, b); sink = res[0]; });
    m.pureNs = t.ns;
    if (res != ref) m.correct = false;

    t = timeIt([&] { res = karatsubaHybrid(a, b, threshold); sink = res[0]; });
    m.hybridNs = t.ns;
    if (res != ref) m.correct = false;

    return m;
}

// ---------------------------------------------------------- input parsing

std::string stripSpaces(std::string s) 
{
    s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c); }), s.end());
    return s;
}

std::string trimEnds(const std::string& s) 
{
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

std::vector<std::string> split(const std::string& s, char delim) 
{
    std::vector<std::string> parts;
    size_t start = 0;
    while (true) 
    {
        size_t pos = s.find(delim, start);
        parts.push_back(s.substr(start, pos == std::string::npos ? std::string::npos : pos - start));
        if (pos == std::string::npos) break;
        start = pos + 1;
    }
    return parts;
}

bool parseSize(const std::string& s, size_t& out) 
{
    if (s.empty() || !std::isdigit(static_cast<unsigned char>(s[0]))) return false;
    try 
    {
        size_t pos = 0;
        unsigned long long v = std::stoull(s, &pos);
        if (pos != s.size()) return false;
        out = static_cast<size_t>(v);
        return true;
    } 
    catch (...) 
    {
        return false;
    }
}

// Accepts "10,100,1000", "100:5000:100" (start:end:step) or "10:100000:*10" (start:end:*factor).
// Result is sorted ascending with duplicates removed. Every digit count must be >= 1.
bool parseSizes(const std::string& rawSpec, std::vector<size_t>& out) 
{
    std::string spec = stripSpaces(rawSpec);
    out.clear();
    if (spec.empty()) return false;

    if (spec.find(':') != std::string::npos) 
    {
        std::vector<std::string> parts = split(spec, ':');
        if (parts.size() != 3) return false;
        bool geometric = !parts[2].empty() && parts[2][0] == '*';
        size_t start, end, step;
        if (!parseSize(parts[0], start) || !parseSize(parts[1], end)) return false;
        if (!parseSize(geometric ? parts[2].substr(1) : parts[2], step)) return false;
        if (start == 0 || end < start || step == 0 || (geometric && step < 2)) return false;
        size_t n = start;
        while (true) {
            out.push_back(n);
            if (geometric) 
            {
                if (n > end / step) break;
                n *= step;
            } else 
            {
                if (step > end - n) break;
                n += step;
            }
        }
    } 
    else 
    {
        for (const std::string& part : split(spec, ',')) 
        {
            size_t n;
            if (!parseSize(part, n) || n == 0) return false;
            out.push_back(n);
        }
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return true;
}

std::string ask(const std::string& prompt) 
{
    std::cout << prompt << std::flush;
    std::string line;
    std::getline(std::cin, line);
    return line;
}

// ------------------------------------------------------------------- main

int main() 
{
    std::mt19937_64 rng(std::random_device{}());

    std::cout << "Karatsuba benchmark -> CSV file\n\n";

    // 1. Hybrid cutoff
    size_t threshold = 32;
    std::string s = stripSpaces(ask("Hybrid cutoff in digits (schoolbook at or below this) [default 32]: "));
    if (!s.empty() && (!parseSize(s, threshold) || threshold == 0)) 
    {
        std::cout << "Invalid cutoff, using 32.\n";
        threshold = 32;
    }

    // 2. Digit counts
    std::cout << "\nDigit counts to test (each of the two random numbers has this many digits):\n"
                 "  list:       10,100,1000\n"
                 "  range:      100:5000:100     (start:end:step)\n"
                 "  geometric:  10:100000:*10    (start:end:*factor)\n";
    std::vector<size_t> sizes;
    while (true) 
    {
        std::string spec = ask("Digit counts: ");
        if (!std::cin) 
        {
            std::cout << "\nNo input received.\n";
            return 1;
        }
        if (parseSizes(spec, sizes)) break;
        std::cout << "Couldn't parse that, please try again.\n";
    }

    // 3. Output file
    std::string path = trimEnds(ask("\nOutput file [karatsuba_results.csv]: "));
    if (path.empty()) path = "karatsuba_results.csv";

    std::ofstream out(path);
    if (!out) 
    {
        std::cerr << "Could not open '" << path << "' for writing.\n";
        return 1;
    }
    out.imbue(std::locale::classic());  // '.' decimals, no thousands separators
    out << std::fixed << std::setprecision(3);
    //Row headers
    out << "Digits,Hybrid_Cutoff,Native_ns,Schoolbook_ns,Pure_Karatsuba_ns,Hybrid_Karatsuba_ns,Check\n";

    std::cout << "\nRunning " << sizes.size() << " size(s), hybrid cutoff " << threshold << "...\n";
    bool allCorrect = true;
    for (size_t i = 0; i < sizes.size(); ++i) 
    {
        std::cout << "  [" << i + 1 << "/" << sizes.size() << "] " << sizes[i] << " digits... " << std::flush;
        Measurement m = measure(sizes[i], threshold, rng);

        out << m.digits << ',' << threshold << ',';
        if (m.nativeRan) out << m.nativeNs;  // left blank when native * can't hold the numbers
        out << ',' << m.schoolNs << ',' << m.pureNs << ',' << m.hybridNs << ','
            << (m.correct ? "OK" : "MISMATCH") << '\n';
        out.flush();  // keep partial results if a long run is interrupted

        std::cout << (m.correct ? "done\n" : "MISMATCH\n");
        if (!m.correct) allCorrect = false;
    }

    std::cout << "\nWrote " << sizes.size() << " row(s) to " << path
              << (allCorrect ? "" : "  (WARNING: at least one result did not match the reference)") << "\n";
    return 0;
}