# GPengine_eval_SSE256
Example of using Magpie to speedup the performance critical interpret of GPengine
(Peter Nordin's) linear genetic programming system. Here set up with just the usual four
arithmetic operations (+ - times and protected division) operating on unsigned
char data types for extended evolution on the Mackey-Glass chaotic time series.

The C++ interpreter's eval function has been extracted from GPengine and modified
to operator on 16 test cases simultaneously using Intel's 256 bit SSE instruction set.

Speedup is measured by Linux' perf instruction count.
Which Magpie's local search tries to minify.

Assumes new GCC g++ compiler (here 11.5.0).
Assumes new Python (here 3.11.5).
SSE instruction set (your hardware must support at least -march=skylake).
If you have AVX 512 (-march=skylake-avx512) you might consider https://github.com/wblangdon/GPengine_eval_AVX512
Assumes Magpie (30 Sep 2025) https://github.com/bloa/magpie

It is recommened that you create examples/avx_extra to hold the development versions
of the files in the examples/avx directory.

This work will be presented at GI@ICSE 2026, 13 April, Rio (Brazil) https://geneticimprovementofsoftware.com/events/icse2026
```
@InProceedings{Langdon:2026:GI,
    author = "William B. Langdon and Carol Hanna",
    title = "Improving a Parallel {C++ Intel SSE SIMD} Linear Genetic Programming Interpreter",
    booktitle = "15th International Workshop on Genetic Improvement @ICSE 2026",
    year = "2026",
    editor = "Aymeric Blot and Oliver Krauss",
    address = "Rio de Janeiro",
    month = "13 April",
    url = "http://gpbib.cs.ucl.ac.uk/gi2026/langdon_2026_GI.pdf",
    code_url = "https://github.com/wblangdon/rand_malloc",
    size = "8 pages",
}
```
