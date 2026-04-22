// REQUIRES: any-device
// RUN: %clangxx -fsycl -Wno-error=deprecated-declarations %s -o %t.out
// RUN: %t.out

#include <sycl/sycl.hpp>

#include <cassert>
#include <memory>

using namespace sycl;

// TODO: original test works with buffers, revert changes to USM once they are
// implemented.
// TODO add cases with dimensions more than 1
int main() {
  bool Fail{};

  constexpr size_t DataSize = 10;
  const range<1> globalRange(6);
  // Id indexer
  {
    queue Q;
    int *Data = sycl::malloc_shared<int>(DataSize, Q);
    for (size_t i = 0; i < DataSize; ++i)
      Data[i] = -1;

    Q.parallel_for<class id1>(globalRange,
                              [=](id<1> index) { Data[index] = index[0]; });
    Q.wait();

    for (size_t i = 0; i < DataSize; i++) {
      const int id = Data[i];
      if (i < globalRange[0]) {
        Fail |= !(id == i);
      } else {
        Fail |= !(id == -1);
      }
    }

    free(Data, Q);
  }
  // print and return;

  // Item indexer without offset
  {
    // TODO: replace strcut with sycl::int2 once implemented.
    struct DoubleInt {
      int First;
      int Second;
    };
    queue Q;
    DoubleInt *Data = sycl::malloc_shared<DoubleInt>(DataSize, Q);
    for (size_t i = 0; i < DataSize; ++i)
      Data[i] = {-1, -1};

    Q.parallel_for<class item1_nooffset>(
        globalRange, [=](item<1, false> index) {
          Data[index.get_id()] = {int(index.get_id()[0]),
                                  int(index.get_range()[0])};
        });
    Q.wait();
    for (size_t i = 0; i < DataSize; ++i) {
      const int id = Data[i].First;
      const int range = Data[i].Second;
      if (i < globalRange[0]) {
        Fail |= !(id == i);
        Fail |= !(range == globalRange[0]);
      } else {
        Fail |= !(id == -1);
        Fail |= !(range == -1);
      }
    }
    free(Data, Q);
  }

  // get_linear_id()
  {
    queue Q;
    size_t DataSize3D = DataSize * DataSize * DataSize;
    int *Data = sycl::malloc_shared<int>(DataSize3D, Q);
    Q.parallel_for(range<3>(DataSize, DataSize, DataSize), [=](item<3> Idx) {
      auto Id = Idx.get_linear_id();
      Data[Id] = Id;
    });
    Q.wait();
    for (size_t i = 0; i < DataSize3D; ++i) {
      Fail |= !(Data[i] == i);
    }
    free(Data, Q);
  }

  // TODO:  Item indexer with offset
  // blocked by liboffload support
  // blocked by absence of sycl::handler implementation

  // TODO: add nd_item check
  return Fail;
}
