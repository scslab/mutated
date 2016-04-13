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
    bool sorted;

  public:
    using size_type = std::vector<uint64_t>::size_type;

    accum(void) noexcept : samples{}, sorted{false} {}

    explicit accum(std::size_t reserve) noexcept : samples{}, sorted{false}
    {
        samples.reserve(reserve);
    }

    ~accum(void) noexcept {}

    void clear(void);
    void add_sample(uint64_t val);
    void print_samples(void);

    double mean(void);
    double stddev(void);
    uint64_t percentile(double percent);
    uint64_t min(void);
    uint64_t max(void);
    size_type size(void);
};

#endif /* MUTATED_ACCUM_HH */
