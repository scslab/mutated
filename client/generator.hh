#ifndef MUTATED_GENERATOR_HH
#define MUTATED_GENERATOR_HH

/**
 * generator.h - support for load generators
 */

#include <cstdint>
#include <random>

#include "opts.hh"

class generator
{
private:
	std::mt19937 & rand_;
	std::exponential_distribution<double> service_dist_;

	uint64_t gen_service_time(void);

public:
	/* Constructor */
	generator(double service_us, std::mt19937 & rand);

	/* No copy or move */
	generator(const generator &) = delete;
	generator(generator &&) = delete;
	generator & operator=(const generator &) = delete;
	generator & operator=(generator &&) = delete;

	/* Generate requests */
	void do_request(bool should_measure);
};

#endif /* MUTATED_GENERATOR_HH */
