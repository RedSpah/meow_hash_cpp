#include <iostream>
#include <chrono>
#include <random>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <stdlib.h>

#ifdef _MSC_VER
#define _MEOWH_256
#endif

#include "meow_hash.hpp"
#include "meow_hash.h"


#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

#if defined(_MSC_VER)
#define ALIGN_MALLOC(N, A) _aligned_malloc(N, A)
#define ALIGN_FREE(P) _aligned_free(P)
#elif defined(__GNUC__)
#define ALIGN_MALLOC(N, A) aligned_alloc(A, N)
#define ALIGN_FREE(P) free(P)
#endif



template <typename T>
bool cmp(T a, T b)
{
	return !(std::memcmp(&a, &b, sizeof(T)));
}

std::string pretty_time(uint64_t ns)
{
	std::string out = "";

	if (ns > 1000000000)
	{
		int32_t sec = ns / 1000000000;
		ns %= 1000000000;
		out += std::to_string(sec) + "s ";
	}
	if (ns > 1000000)
	{
		int32_t ms = ns / 1000000;
		ns %= 1000000;
		out += std::to_string(ms) + "ms ";
	}
	if (ns > 1000)
	{
		int32_t us = ns / 1000;
		ns %= 1000;
		out += std::to_string(us) + "us ";
	}
	if (ns > 0)
	{
		out += std::to_string(ns) + "ns";
	}

	return out;
}

int main(int argc, char** argv)
{
	int res = Catch::Session().run(argc, argv);

	std::cout << "Naive Benchmark\n\n";

	{
		std::cout << "\n=== UNALIGNED: ===\n\n";;

		constexpr int32_t base_num = (1 << 12);
		constexpr int32_t base_buf_size = (1 << 16);
		constexpr int32_t adv_steps = 8;

		int32_t test_buf_size = base_buf_size;
		int32_t test_num = base_num;
		  
		for (int adv_step = 0; adv_step < adv_steps; adv_step++)
		{
			std::minstd_rand rng(std::chrono::system_clock::now().time_since_epoch().count());
			std::uniform_int_distribution<uint32_t> dist(0, 4294967295U);

			std::array<uint64_t, base_num> bench_time_h, bench_time_hpp;

			std::vector<uint32_t> input_buffer;
			input_buffer.resize(test_buf_size);

			for (int i = 0; i < test_num; i++)
			{

				std::generate(input_buffer.begin(), input_buffer.end(), [&rng, &dist]() {return dist(rng); });
				uint64_t seed = ((static_cast<uint64_t>(dist(rng)) << 32) + dist(rng));

				// Each test is ran four time, first with the C version, second with the C++ version, third again with the C version, and fourth with the C++ version. This is done to account for any possible slowdowns caused by cache misses.

				// warmup
				auto tp_h_1 = std::chrono::system_clock::now();
				meow_lane res_h = MeowHash1(seed, test_buf_size * sizeof(uint32_t), input_buffer.data());
				auto tp_h_2 = std::chrono::system_clock::now();

				auto tp_hpp_1 = std::chrono::system_clock::now();
				meowh::hash_t<128> res_hpp = meowh::meow_hash<128>(input_buffer, seed);
				auto tp_hpp_2 = std::chrono::system_clock::now();

				// reality
				tp_h_1 = std::chrono::system_clock::now();
				res_h = MeowHash1(seed, test_buf_size * sizeof(uint32_t), input_buffer.data());
				tp_h_2 = std::chrono::system_clock::now();

				bench_time_h[i] = std::chrono::duration_cast<std::chrono::nanoseconds>(tp_h_2 - tp_h_1).count();

				tp_hpp_1 = std::chrono::system_clock::now();
				res_hpp = meowh::meow_hash<128>(input_buffer, seed);
				tp_hpp_2 = std::chrono::system_clock::now();

				bench_time_hpp[i] = std::chrono::duration_cast<std::chrono::nanoseconds>(tp_hpp_2 - tp_hpp_1).count();

				if (res_h.Sub[0] != res_hpp.as<64>(0))
				{
					std::cout << "ERROR: adv_step: " << adv_step << " i: " << i << " | res_h: " << res_h.Sub[0] << " | res_hpp: " << res_hpp.as<64>(0) << "\n";
				}
			}

			uint64_t min_h = *std::min_element(bench_time_h.begin(), bench_time_h.begin() + test_num);
			uint64_t min_hpp = *std::min_element(bench_time_hpp.begin(), bench_time_hpp.begin() + test_num);

			uint64_t max_h = *std::max_element(bench_time_h.begin(), bench_time_h.begin() + test_num);
			uint64_t max_hpp = *std::max_element(bench_time_hpp.begin(), bench_time_hpp.begin() + test_num);

			uint64_t total_h = std::accumulate(bench_time_h.begin(), bench_time_h.begin() + test_num, 0ull);
			uint64_t total_hpp = std::accumulate(bench_time_hpp.begin(), bench_time_hpp.begin() + test_num, 0ull);

			uint64_t mean_h = total_h / test_num;
			uint64_t mean_hpp = total_hpp / test_num;

			std::sort(bench_time_h.begin(), bench_time_h.begin() + test_num);
			std::sort(bench_time_hpp.begin(), bench_time_hpp.begin() + test_num);

			uint64_t median_h = bench_time_h[test_num / 2];
			uint64_t median_hpp = bench_time_hpp[test_num / 2];

			std::cout << "Tests ran: " << test_num << "\nInput buffer size: " << test_buf_size * sizeof(uint32_t) << "\n\n";

			std::cout << "meow_hash.h:\n* Total time: " << pretty_time(total_h) <<
				"\n* Mean: " << pretty_time(mean_h) <<
				"\n* Min: " << pretty_time(min_h) <<
				"\n* Max: " << pretty_time(max_h) <<
				"\n* Median: " << pretty_time(median_h) << "\n\n";

			std::cout << "meow_hash.hpp:\n* Total time: " << pretty_time(total_hpp) <<
				"\n* Mean: " << pretty_time(mean_hpp) <<
				"\n* Min: " << pretty_time(min_hpp) <<
				"\n* Max: " << pretty_time(max_hpp) <<
				"\n* Median: " << pretty_time(median_hpp) << "\n\n";

			std::wcout << "Advantage of the C version: " <<
				"\n* Mean: " << 100 * (1 - (static_cast<double>(mean_h) / static_cast<double>(mean_hpp))) << "%"
				"\n* Min: " << 100 * (1 - (static_cast<double>(min_h) / static_cast<double>(min_hpp))) << "%"
				"\n* Max: " << 100 * (1 - (static_cast<double>(max_h) / static_cast<double>(max_hpp))) << "%"
				"\n* Median: " << 100 * (1 - (static_cast<double>(median_h) / static_cast<double>(median_hpp))) << "%\n\n\n";

			test_num /= 2;
			test_buf_size *= 2;
		}
	}


	{
		std::cout << "\n=== ALIGNED: ===\n\n";

		constexpr int32_t align_base_num = (1 << 12);
		constexpr int32_t align_base_buf_size = (1 << 16);
		constexpr int32_t align_adv_steps = 8;

		int32_t align_test_buf_size = align_base_buf_size;
		int32_t align_test_num = align_base_num; 

		uint32_t* input_buffer = reinterpret_cast<uint32_t*>(ALIGN_MALLOC((align_base_buf_size << align_adv_steps) * sizeof(uint32_t), 64)); 

		for (int adv_step = 0; adv_step < align_adv_steps; adv_step++)
		{
			std::minstd_rand rng(std::chrono::system_clock::now().time_since_epoch().count());
			std::uniform_int_distribution<uint32_t> dist(0, 4294967295U);

			std::array<uint64_t, align_base_num> bench_time_h, bench_time_hpp;

			for (int i = 0; i < align_test_num; i++)
			{

				std::generate(input_buffer, input_buffer + align_test_buf_size, [&rng, &dist]() {return dist(rng); });
				uint64_t seed = ((static_cast<uint64_t>(dist(rng)) << 32) + dist(rng));

				// Each test is ran four time, first with the C version, second with the C++ version, third again with the C version, and fourth with the C++ version. This is done to account for any possible slowdowns caused by cache misses.

				// warmup
				auto tp_h_1 = std::chrono::system_clock::now();
				meow_lane res_h = MeowHash1(seed, align_test_buf_size * sizeof(uint32_t), input_buffer);
				auto tp_h_2 = std::chrono::system_clock::now();

				auto tp_hpp_1 = std::chrono::system_clock::now();
				meowh::hash_t<128> res_hpp = meowh::meow_hash<128, true>(input_buffer, align_test_buf_size * sizeof(uint32_t), seed);
				auto tp_hpp_2 = std::chrono::system_clock::now();

				// reality
				tp_h_1 = std::chrono::system_clock::now();
				res_h = MeowHash1(seed, align_test_buf_size * sizeof(uint32_t), input_buffer);
				tp_h_2 = std::chrono::system_clock::now();

				bench_time_h[i] = std::chrono::duration_cast<std::chrono::nanoseconds>(tp_h_2 - tp_h_1).count();

				tp_hpp_1 = std::chrono::system_clock::now();
				res_hpp = meowh::meow_hash<128, true>(input_buffer, align_test_buf_size * sizeof(uint32_t), seed);
				tp_hpp_2 = std::chrono::system_clock::now();

				bench_time_hpp[i] = std::chrono::duration_cast<std::chrono::nanoseconds>(tp_hpp_2 - tp_hpp_1).count();

				if (res_h.Sub[0] != res_hpp.as<64>(0))
				{
					std::cout << "ERROR: adv_step: " << adv_step << " i: " << i << " | res_h: " << res_h.Sub[0] << " | res_hpp: " << res_hpp.as<64>(0) << "\n";
				}
			}

			uint64_t min_h = *std::min_element(bench_time_h.begin(), bench_time_h.begin() + align_test_num);
			uint64_t min_hpp = *std::min_element(bench_time_hpp.begin(), bench_time_hpp.begin() + align_test_num);

			uint64_t max_h = *std::max_element(bench_time_h.begin(), bench_time_h.begin() + align_test_num);
			uint64_t max_hpp = *std::max_element(bench_time_hpp.begin(), bench_time_hpp.begin() + align_test_num);

			uint64_t total_h = std::accumulate(bench_time_h.begin(), bench_time_h.begin() + align_test_num, 0ull);
			uint64_t total_hpp = std::accumulate(bench_time_hpp.begin(), bench_time_hpp.begin() + align_test_num, 0ull);

			uint64_t mean_h = total_h / align_test_num;
			uint64_t mean_hpp = total_hpp / align_test_num;

			std::sort(bench_time_h.begin(), bench_time_h.begin() + align_test_num);
			std::sort(bench_time_hpp.begin(), bench_time_hpp.begin() + align_test_num);

			uint64_t median_h = bench_time_h[align_test_num / 2];
			uint64_t median_hpp = bench_time_hpp[align_test_num / 2];

			std::cout << "Tests ran: " << align_test_num << "\nInput buffer size: " << align_test_buf_size * sizeof(uint32_t) << "\n\n";

			std::cout << "meow_hash.h:\n* Total time: " << pretty_time(total_h) <<
				"\n* Mean: " << pretty_time(mean_h) <<
				"\n* Min: " << pretty_time(min_h) <<
				"\n* Max: " << pretty_time(max_h) <<
				"\n* Median: " << pretty_time(median_h) << "\n\n";

			std::cout << "meow_hash.hpp:\n* Total time: " << pretty_time(total_hpp) <<
				"\n* Mean: " << pretty_time(mean_hpp) <<
				"\n* Min: " << pretty_time(min_hpp) <<
				"\n* Max: " << pretty_time(max_hpp) <<
				"\n* Median: " << pretty_time(median_hpp) << "\n\n";

			std::wcout << "Advantage of the C version: " <<
				"\n* Mean: " << 100 * (1 - (static_cast<double>(mean_h) / static_cast<double>(mean_hpp))) << "%"
				"\n* Min: " << 100 * (1 - (static_cast<double>(min_h) / static_cast<double>(min_hpp))) << "%"
				"\n* Max: " << 100 * (1 - (static_cast<double>(max_h) / static_cast<double>(max_hpp))) << "%"
				"\n* Median: " << 100 * (1 - (static_cast<double>(median_h) / static_cast<double>(median_hpp))) << "%\n\n\n";

			align_test_num /= 2;
			align_test_buf_size *= 2;
		}

		ALIGN_FREE(input_buffer);
	}

	return res;
}

TEST_CASE("Results are the same as the original implementation for small inputs", "[compatibility]")
{
	uint32_t alternating_data[] = { 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF };
	uint32_t zero_data[] = { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 };
	uint32_t one_data[] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
	uint32_t alternating_data2[] = { 0xAAAAAAAA, 0xAAAAAAAA, 0xAAAAAAAA, 0xAAAAAAAA, 0xAAAAAAAA, 0xAAAAAAAA, 0xAAAAAAAA, 0xAAAAAAAA };

	meow_lane res_h_1 = MeowHash1(0, 32, alternating_data);
	meow_lane res_h_2 = MeowHash1(0, 32, zero_data);
	meow_lane res_h_3 = MeowHash1(0, 32, one_data);
	meow_lane res_h_4 = MeowHash1(0, 32, alternating_data2);

	meowh::hash_t<128> res_hpp_1 = meowh::meow_hash<128>(alternating_data, 32, 0);
	meowh::hash_t<128> res_hpp_2 = meowh::meow_hash<128>(zero_data, 32, 0);
	meowh::hash_t<128> res_hpp_3 = meowh::meow_hash<128>(one_data, 32, 0);
	meowh::hash_t<128> res_hpp_4 = meowh::meow_hash<128>(alternating_data2, 32, 0);


#ifdef _MEOWH_512
	// Compares the __m512i's directly
	REQUIRE(cmp(res_h_1.Q0, res_hpp_1.as<512>(0)));
	REQUIRE(cmp(res_h_2.Q0, res_hpp_2.as<512>(0)));
	REQUIRE(cmp(res_h_3.Q0, res_hpp_3.as<512>(0)));
	REQUIRE(cmp(res_h_4.Q0, res_hpp_4.as<512>(0)));
#endif

#ifdef _MEOWH_256
	// Compares the constituent __m256i's to test whether the internal bit order is preserved
	REQUIRE(cmp(res_h_1.D0, res_hpp_1.as<256>(0)));
	REQUIRE(cmp(res_h_1.D1, res_hpp_1.as<256>(1)));
	REQUIRE(cmp(res_h_2.D0, res_hpp_2.as<256>(0)));
	REQUIRE(cmp(res_h_2.D1, res_hpp_2.as<256>(1)));
	REQUIRE(cmp(res_h_3.D0, res_hpp_3.as<256>(0)));
	REQUIRE(cmp(res_h_3.D1, res_hpp_3.as<256>(1)));
	REQUIRE(cmp(res_h_4.D0, res_hpp_4.as<256>(0)));
	REQUIRE(cmp(res_h_4.D1, res_hpp_4.as<256>(1)));
#endif
}

TEST_CASE("Results are the same as the original implementation for large, randomly generated inputs", "[compatibility]")
{
	constexpr int32_t test_num = 64;
	constexpr int32_t test_buf_size = (1 << 20);

	std::minstd_rand rng(std::chrono::system_clock::now().time_since_epoch().count());
	std::uniform_int_distribution<uint32_t> dist(0, 4294967295U);

	for (int i = 0; i < test_num; i++)
	{
		std::vector<uint32_t> input_buffer;

		input_buffer.resize(test_buf_size);
		std::generate(input_buffer.begin(), input_buffer.end(), [&rng, &dist]() {return dist(rng); });

		uint64_t seed = ((static_cast<uint64_t>(dist(rng)) << 32) + dist(rng));

		meow_lane res_h = MeowHash1(seed, test_buf_size * sizeof(uint32_t), input_buffer.data());
		meowh::hash_t<128> res_hpp = meowh::meow_hash<128>(input_buffer, seed);

#ifdef _MEOWH_512
		REQUIRE(cmp(res_h.Q0, res_hpp[0].as<512>(0));
#endif

#ifdef _MEOWH_256
		REQUIRE(cmp(res_h.D0, res_hpp.as<256>(0)));
		REQUIRE(cmp(res_h.D1, res_hpp.as<256>(1)));
#endif

#ifdef _MEOWH_128
		REQUIRE(cmp(res_h.L0, res_hpp[0]));
		REQUIRE(cmp(res_h.L1, res_hpp[1]));
		REQUIRE(cmp(res_h.L2, res_hpp[2]));
		REQUIRE(cmp(res_h.L3, res_hpp[3]));
#endif

		for (int k = 0; k < 8; k++)
		{
			REQUIRE(res_h.Sub[k] == res_hpp.as<64>(k));
		}

		for (int k = 0; k < 16; k++)
		{
			REQUIRE(res_h.Sub32[k] == res_hpp.as<32>(k));
		}
	}

}
