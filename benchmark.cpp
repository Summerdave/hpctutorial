#include <iostream>
#include <algorithm>
#include <chrono>
#include <vector>
#include <numeric>

using TFloat = double;

decltype(auto) init(int N) {
  auto vec = std::vector<TFloat>(N);
  std::iota(vec.begin(), vec.end(), 0);
  return vec;
}

void stream(TFloat* a, TFloat* b, TFloat* c, TFloat* d, int N) {
  for (int i = 0; i < N; ++i) {
    *a++ = *b++ * *c++ + *d++;
  }
}

void faststream(TFloat* a, TFloat* b, TFloat* c, TFloat* d, int N) {
  for (int i = 0; i < N/2; ++i) {
    *a++ = *b++ * *c++ + *d++;
  }
}

int main(int argc, char* argv[]) {
  constexpr int N = 10'000'000;
  int num_iters = 10;
  num_iters += 1 - (num_iters%2);
  using TimeUnit = std::chrono::microseconds;

  std::array functions {stream, faststream};
  for (const auto& func: functions) {
    auto results = std::vector<int>(num_iters);
    for (auto i = 0; i < num_iters; ++i) {
      auto a = init(N);
      auto b = init(N);
      auto c = init(N);
      auto d = init(N);
      auto start = std::chrono::high_resolution_clock::now();
      func(&a[0], &b[0], &c[0], &d[0], N);
      auto end = std::chrono::high_resolution_clock::now();
      const auto diff = std::chrono::duration_cast<TimeUnit>(end - start).count();
      results[i] = diff;
    }
    const auto max = *std::max_element(results.begin(), results.end());
    const auto min = *std::min_element(results.begin(), results.end());
    const auto middle = num_iters/2;
    std::nth_element(results.begin(), results.begin() + middle, results.end());
    const auto median = results[middle];
    const auto mean = (max + min) / 2.;
    std::cout << "Max: " << max << "\n";
    std::cout << "Min: " << min << "\n";
    std::cout << "Mean: " << mean << "\n";
    std::cout << "Median: " << median << "\n";
  }
}
