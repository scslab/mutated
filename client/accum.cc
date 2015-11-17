#include <algorithm>
#include <numeric>

#include "accum.hh"

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

	for (std::vector<uint64_t>::size_type i = 0; i < samples.size(); i++)
		avg += (double) samples[i] / (double) samples.size();

	return avg;
}

double accum::stddev(void)
{
	double avg = mean();
	double sum = 0;

	for (std::vector<uint64_t>::size_type i = 0; i < samples.size(); i++) {
		sum += ((double) samples[i] - avg) * ((double) samples[i] - avg);
	}

	return std::sqrt(sum / samples.size());
}

uint64_t accum::percentile(double percent)
{
	std::sort(samples.begin(), samples.end());

	return samples[ceil((double) samples.size() * percent) - 1];
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
