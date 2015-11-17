#ifndef GENERATOR_HH
#define GENERATOR_HH

/**
 * generator.h - support for load generators
 */

#include <cstdint>
#include <random>

#include "client.hh"

struct config {
	char addr[128];		     /* the server address */
	uint16_t port;		     /* the server port */
	const char *label;	   /* label describing server (-m relevant only) */

	double service_us;	   /* service time mean microseconds */
	double arrival_us;	   /* arrival time mean microseconds */

	double step_size;	     /* step size in req/s */
	double step_stop;	     /* maximum req/s */

	uint64_t pre_samples;	 /* number of samples to warm up on */
	uint64_t samples;	     /* number of samples to measure */
	uint64_t post_samples; /* number of samples to cool down on */

	bool machine_readable; /* generate machine readable output? */

	int lb_cnt;
	bool least_loaded;
};

class generator
{
public:
  generator(void) {};
  virtual ~generator(void) {};

	virtual int start(void) = 0;
	virtual int do_request(bool should_measure) = 0;
};

class generator_flowperreq : public generator
{
protected:
	const struct config & cfg;

public:
	generator_flowperreq(const struct config & cfg)
		: cfg{cfg} {};

	/* No copy or move */
	generator_flowperreq(const generator_flowperreq &) = delete;
	generator_flowperreq(generator_flowperreq &&) = delete;
	generator_flowperreq & operator=(const generator_flowperreq &) = delete;
	generator_flowperreq & operator=(generator_flowperreq &&) = delete;

	int start(void) override;
	int do_request(bool should_measure) override;
};

#endif /* GENERATOR_HH */
