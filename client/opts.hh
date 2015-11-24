#ifndef OPTS_HH
#define OPTS_HH

#include <cstdint>

struct Config
{
public:
	char addr[128];         /* the server address */
	uint16_t port;          /* the server port */
	const char *label;      /* label describing server (-m relevant only) */

	double service_us;      /* service time mean microseconds */
	double arrival_us;      /* arrival time mean microseconds */

	double step_size;       /* step size in req/s */
	double step_stop;       /* maximum req/s */

	uint64_t pre_samples;   /* number of samples to warm up on */
	uint64_t samples;       /* number of samples to measure */
	uint64_t post_samples;  /* number of samples to cool down on */
	uint64_t total_samples; /* total number of samples to collect */

	bool machine_readable;  /* generate machine readable output? */

	int lb_cnt;
	bool least_loaded;

public:
  Config(void);
	Config(int argc, char *argv[]);
};

#endif /* OPTS_HH */
