#ifndef MUTATED_ACCUM_HH
#define MUTATED_ACCUM_HH

#include <cstdint>
#include <vector>

/**
 * A sample accumulator container.
 */
class accum
{
  private:
    std::vector<uint64_t> samples;

  public:
    using size_type = std::vector<uint64_t>::size_type;

    accum(void)
      : samples{}
    {
    }

    void clear(void);
    void add_sample(uint64_t val);

    double mean(void);
    double stddev(void);
    uint64_t percentile(double percent);
    uint64_t min(void);
    uint64_t max(void);
    size_type size(void);
};

#endif /* MUTATED_ACCUM_HH */
