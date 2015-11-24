#ifndef MUTATED_CLIENT2_HH
#define MUTATED_CLIENT2_HH

#include "accum.hh"
#include "generator.hh"
#include "opts.hh"

/**
 * Mutated load generator.
 */
struct Client
{
private:
  Config cfg;

  generator *gen;
  std::random_device rd;
  std::mt19937 randgen;

  accum service_samples;
  accum wait_samples;
  double throughput;

  struct timespec start_ts;
  uint64_t in_count, out_count, measure_count;
  double step_pos;
  uint64_t step_count;

  int epollfd;
  int timerfd;

	struct timespec *deadlines;
	struct timespec start_time;

	void do_request(void);
	int timer_arm(struct timespec deadline);
	void timer_handler(void);
	void setup_deadlines(void);
  void setup_experiment(void);

public:
	/* Create a new client */
  Client(int argc, char *argv[]);

  /* Destructor */
  ~Client(void);

  /* No copy or move */
  Client(const Client &) = delete;
  Client(Client &&) = delete;
  Client & operator=(const Client &) = delete;
  Client & operator=(Client &&) = delete;

	/* Run the load generator */
	void run(void);

	/**
	 * epoll_watch - registers a file descriptor for epoll events
	 * @fd: the file descriptor
	 * @data: a cookie for the event
	 * @event: the event mask
	 *
	 * Returns 0 if successful, otherwise < 0.
	 */
	int epoll_watch(int fd, void *data, uint32_t events);

	/* Print summary of current results */
	void print_summary(void);

	/* Record a latency sample */
	void record_sample(uint64_t service_us, uint64_t wait_us, bool should_measure);
};

#endif /* MUTATED_CLIENT2_HH */
