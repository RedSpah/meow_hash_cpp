#include <iostream>
#include <intrin.h>
#include <immintrin.h>
//#define MEOW_HASH_512
//#define MEOW_HASH_256
#include "meow_hash.h"
#include "meow_hash.hpp"


int main()
{
	int dat[] = { 1, 2, 3, 4 };
	meow_lane hash1 = MeowHash1(0, 16, static_cast<void*>(dat));
	
	meowh::hash_t<64> hash2 = meowh::meow_hash<128, 64>(static_cast<void*>(dat), 16, 0);

	std::cout << hash1.Sub[0] << "\n" << hash2[0] << std::endl;

	__m128i h2 = hash2.as<128>(3);
	__m128i h1 = hash1.L3;
	
	std::cout << std::memcmp(&h1, &h2, 16) << std::endl;

	std::cin.get();


}