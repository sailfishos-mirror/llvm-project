// REQUIRES: any-device
// RUN: %clangxx -fsycl %s -o %t.out
// RUN: %t.out

#include <sycl/sycl.hpp>

#include <cassert>
#include <iostream>
#include <type_traits>

int main() {
  // TODO: uncomment property once it is implemented. now all sycl::queue
  // objects are in-order due to liboffload limitation. Test is intended to
  // check in-order execution.
  sycl::queue q{/*sycl::property::queue::in_order()*/};
  auto dev = q.get_device();
  auto ctx = q.get_context();
  constexpr int N = 8;

  auto A = static_cast<int *>(sycl::malloc_shared(N * sizeof(int), dev, ctx));

  for (int i = 0; i < N; i++) {
    A[i] = 1;
  }

  q.parallel_for<class Bar>(N, [=](auto i) {
    static_assert(std::is_same<decltype(i), sycl::item<1>>::value,
                  "lambda arg type is unexpected");
    A[i]++;
  });

  q.parallel_for<class Foo>({N}, [=](auto i) {
    static_assert(std::is_same<decltype(i), sycl::item<1>>::value,
                  "lambda arg type is unexpected");
    A[i]++;
  });

  // TODO: add kernel with offset and kernel with nd_range once they
  // are implemented.

  q.wait();

  for (int i = 0; i < N; i++) {
    assert(A[i] == 3);
  }
  sycl::free(A, ctx);
}
