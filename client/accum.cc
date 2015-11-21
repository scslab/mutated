#include <algorithm>
#include <cmath>
#include <numeric>

#include "accum.hh"

// TODO: Reserve space?
// TODO: Sort once?

void accum::clear(void)
{
	samples.clear();
}

void accum::add_sample(uint64_t val)
{
	samples.push_back(val);
}

double accum::mean(void)
{
	double avg = 0;
	double sz  = size();

	for (auto i : samples) {
		avg += double(i) / sz;
	}

	return avg;
}

double accum::stddev(void)
{
	double avg = mean();
	double sum = 0;

	for (auto i : samples) {
		double diff = double(i) - avg;
		sum += diff * diff;
	}

	return std::sqrt(sum / size());
}

uint64_t accum::percentile(double percent)
{
	std::sort(samples.begin(), samples.end());
	return samples[ceil(double(size()) * percent) - 1];
}

uint64_t accum::min(void)
{
	std::sort(samples.begin(), samples.end());
	return samples[0];
}

uint64_t accum::max(void)
{
	std::sort(samples.begin(), samples.end());
	return samples[samples.size() - 1];
}

std::vector<uint64_t>::size_type accum::size(void)
{
	return samples.size();
}
