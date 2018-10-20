# meow_hash_cpp
This is a rewrite of the Casey's Muratori Meow hash (https://github.com/cmuratori/meow_hash), utilizing modern C++ features to significantly tidy up the codebase and get rid of a large amount of undefined behaviour present in the original implementation.

Compatibility
----
| Compiler             | Min. Version        | 
|----------------------|:-------------------:|
| MSVC (Visual Studio) | 19.1 (VS 2017.3 P2) | 
| clang                | 3.9                 | 
| gcc                  | 7                   |
| EDG eccp             | 4.14                |

Example Usage
----

```cpp
// standalone hash
std::array<int, 4> input {322, 2137, 42069, 65536};
meowh::hash_t<32> hash = meowh::meow_hash<128, 32>(input); 

// extracting parts of the 512 bit long hash result
unsigned int hash_32 = hash[0];
unsigned long long hash_64 = hash.as<64>(0);
__m128i l0 = hash.as<128>(0);
__m128i l1 = hash.as<128>(1);
__m128i l2 = hash.as<128>(2);
__m128i l3 = hash.as<128>(3);

// converting hash result to use a different element type
meowh::hash_t<256> hash256 = hash;
__m256i h0 = hash256[0];
__m256i h1 = hash256[1];
```


Usage Instructions
----

The template arguments of the `meowh::meow_hash` function specify the following:
* The first argument, N, specifies the version of the hashing algorithm to use. `128` uses MeowHash1, `256` uses MeowHash2, and `512` uses MeowHash4. Nothing else will need to be changed to switch between the different versions when MeowHash2 and MeowHash4 become publically usible.
* The second argument, R, specifies the element type the returned 512 bit hash will use. It is set to the same value as N by default.

Due to undefined behavior problems involving switching between unions, the original hash return type, `meow_lane`, has been replaced by `meowh::hash_t`. `hash_t` has a single template argument, which determines the size in bits of the elements of the internal 512 bit long array to hold the hash result. `32`, `64`, `128`, `256` and `512` are valid values. These elements can be accessed by indexing the `hash_t` object with the array subscript operator (`[]`). Values of other types than the element type can be obtained with the use of the `.as` member function. It takes a single template argument, specifying the size in bits of the desired type, and a single function argument, specifying the desired array member of the internal 512 bit long array expressed as an array of the desired type.

`meowh::meow_hash` provides several convenient overloads, all accepting an optional `seed` argument:
* C-style `const void*` + `size_t` pair
* `const T*` + `size_t` pair, for passing C arrays
* `const std::vector<T>&`
* `const std::basic_string<T>&`
* A pair of templated iterators
* `const std::array<T, N>&`
* `const std::initializer_list<T>&`


Build Instructions
----

The library is provided as a single standalone header, for static linking only. No build instructions are nessessary.

