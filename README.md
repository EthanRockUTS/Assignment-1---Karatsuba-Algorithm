This repository contains two executable files: karatsuba_algorithm.exe, and karatsuba_bench.exe

The **first program (karatsuba_algorithm.exe)** is a standard karatsuba calculator, which will prompt the user to provide two numbers, and then return the resulting multiplication.

If only blank spaces are provided, the algorithm will continue to wait for an input.

If any invalid characters/entries are input, the result will simply return 0.

The program will repeat and allow users to continually multiply numbers, until "0" is entered for the first number, after which the program will exit.

The **second program (karatsuba_bench.exe)** is benchmarking tool, which will compare four different approaches for multiplying two numbers (*, long multiplication (schoolbook), pure karatsuba, hybrid karatsuba).

When the program is run, it will first prompt the user to provide their chosen cut off for the hybrid algorithm. Entering nothing will default to 32 digits.

Then the user will be prompted to set the parameters for the benchmarking test, which will determine the digit counts to be tested. There are three types of valid input:

1. An explicit list of digit counts like: 10,100,1000 (will test 10, then 100, then 1000 digits)

2. A range with a lower and upper bound inclusive + step: 100:5000:100 (will test 100, 200, 300, ..., 4900, 5000)

3. A geometric sweep, multiplying by a chosen number at each step: 10:100000:*10 (will test 10, 100, 1000, 10000, 100000)

Finally, it will request a name for the output file, defaulting to karatsuba_results.csv.

After choosing the parameters the program will begin its benchmarking, represented by progress lines, and then output the results into a CSV file stored in the same directory.