#include <algorithm>
#include <cmath>
#include <numeric>
#include <iostream>

#include "accum.hh"

// TODO: Histogram rather than all samples?

using namespace std;

void accum::clear(void)
{
    samples_.clear();
    sorted_ = false;
}

void accum::add_sample(uint64_t val)
{
    samples_.push_back(val);
    sorted_ = false;
}

void accum::print_samples(void)
{
    for (auto i : samples_) {
        cout << i << endl;
    }
}

double accum::mean(void)
{
    double avg = 0;
    double sz = size();

    for (auto i : samples_) {
        avg += double(i) / sz;
    }

    return avg;
}

double accum::stddev(void)
{
    double avg = mean();
    double sum = 0;

    for (auto i : samples_) {
        double diff = double(i) - avg;
        sum += diff * diff;
    }

    return sqrt(sum / size());
}

uint64_t accum::percentile(double percent)
{
    if (not sorted_) {
        sort(samples_.begin(), samples_.end());
        sorted_ = true;
    }
    return samples_[ceil(double(size()) * percent) - 1];
}

uint64_t accum::min(void)
{
    if (not sorted_) {
        sort(samples_.begin(), samples_.end());
        sorted_ = true;
    }
    return samples_[0];
}

uint64_t accum::max(void)
{
    if (not sorted_) {
        sort(samples_.begin(), samples_.end());
        sorted_ = true;
    }
    return samples_[samples_.size() - 1];
}

vector<uint64_t>::size_type accum::size(void) { return samples_.size(); }
