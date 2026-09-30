#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

//Using "Big" to represent vectors of ints
using Big = std::vector<int>;

// ---------------------------------------------------------------- helpers

//trim leading zeroes (digits are stored backwards in Big)
void trim(Big& a) 
{
    while (a.size() > 1 && a.back() == 0) a.pop_back();
}

//Performs long addition on two bigs (adding the columns and carrying over when necessary)
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

// Requires a >= b numerically. Performs long subtraction, reverse of above, with borrowing when necessary
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
    //If the result has less digits than before, the end of the vector will have leading zeroes, which need to be trimmed.
    trim(r);
    return r;
}

//Shifts positions of the vector contents by k, which effectively replicates multiplying by 10^k.
Big shift(const Big& a, size_t k) 
{
    if (a.size() == 1 && a[0] == 0) return a;
    Big r(a.size() + k, 0);
    std::copy(a.begin(), a.end(), r.begin() + k);
    return r;
}

//Gets the back half of the number (front of the vector), e.g. 1234, this gets 34
Big low(const Big& a, size_t m) 
{
    Big r(a.begin(), a.begin() + std::min(m, a.size()));
    if (r.empty()) r.push_back(0);
    trim(r);
    return r;
}

//Gets the front half of the number (back of the vector), e.g. 1234, this gets 12
Big high(const Big& a, size_t m) 
{
    Big r = a.size() > m ? Big(a.begin() + m, a.end()) : Big{0};
    trim(r);
    return r;
}

//Converts the input string into a vector of digits, whilst skipping whitespaces. If there are no digits, set to {0}
Big convertToBig(std::string a)
{
    Big r;
    for (auto it = a.rbegin(); it != a.rend(); ++it) 
    {
        if (!std::isdigit(static_cast<unsigned char>(*it))) continue;  // skip stray whitespace, etc.
        r.push_back(*it - '0');
    }
    if (r.empty()) r.push_back(0);
    trim(r);
    return r;
}

// ------------------------------------------------------------- algorithms

//Standard long multiplication where each column of the second number is multiplied with each column of the first, O(n^2)
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

//Recursive karatsuba algorithm. Modified so that if the vector is less than the threshold, use schoolbook instead of another recursion.
Big karatsuba(const Big& a, const Big& b) 
{
    size_t threshold = 32;
    size_t n = std::max(a.size(), b.size());
    if (n <= threshold) return schoolbook(a, b);

    size_t m = n / 2;
    Big a0 = low(a, m), a1 = high(a, m);
    Big b0 = low(b, m), b1 = high(b, m);

    Big z0 = karatsuba(a0, b0);
    Big z2 = karatsuba(a1, b1);

    //z1 = a0b0 + a0b1 + a1b0 + a1b1
    //But z0 = a0b0 and z2 = a1b1
    //Therefore just subract a0b0 and a1b1, to get a0b1 + a1b0 without having to multiply
    Big z1 = karatsuba(add(a0, a1), add(b0, b1));
    z1 = sub(sub(z1, z0), z2);

    Big r = add(add(shift(z2, 2 * m), shift(z1, m)), z0);
    trim(r);
    return r;
}

// ------------------------------------------------------------------- main

int main() 
{
    std::string x;
    std::string y;

    while (true)
    {
        std::cout << "Enter the first number, or 0 to exit: ";
        std::cin >> x;

        if (x == "0") 
        {
            break;
        }

        std::cout << "Enter the second number: ";
        std::cin >> y;

        std::cout << "The result is: ";

        Big result = karatsuba(convertToBig(x), convertToBig(y));

        //Read out the result digit by digit (back to front) to print answer
        for (auto it = result.rbegin(); it != result.rend(); it++)
        {
            std::cout << *it;
        }

        std:: cout << "\n";
    } 

    return 0;
}