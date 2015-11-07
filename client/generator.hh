#ifndef GENERATOR_HH
#define GENERATOR_HH

/**
 * generator.h - support for load generators
 */

#include <cstdint>
#include <random>

struct config {
	char addr[128];		/* the server address */
	uint16_t port;		/* the server port */
	const char *label;	/* label describing server (-m relevant only) */

	double service_us;	/* service time mean microseconds */
	double arrival_us;	/* arrival time mean microseconds */

	double step_size;	/* step size in req/s */
	double step_stop;	/* maximum req/s */

	uint64_t pre_samples;	/* number of samples to warm up on */
	uint64_t samples;	/* number of samples to measure */
	uint64_t post_samples;	/* number of samples to cool down on */

	bool machine_readable;	/* generate machine readable output? */

	int lb_cnt;
	bool least_loaded;
};

extern std::mt19937 randgen;
extern void
record_sample(uint64_t service_us, uint64_t wait_us, bool should_measure);

struct generator {
	virtual int start(void) {return 0;};
	virtual int do_request(bool should_measure) {return 0;};
};

struct generator_flowperreq : public generator {
	generator_flowperreq(const struct config *cfg) : cfg(cfg) {};

	int start(void) override;
	int do_request(bool should_measure) override;

protected:
	const struct config *cfg;
};

#endif /* GENERATOR_HH */
