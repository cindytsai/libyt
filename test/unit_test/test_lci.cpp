#include <gtest/gtest.h>
#include <mpi.h>

#include <cstdlib>
#include <cstring>
#include <lci.hpp>

TEST(TestLCI, post_get) {
  lci::g_runtime_init();

  const int nmsgs = 1000;
  int rank = lci::get_rank_me();
  int nranks = lci::get_rank_n();
  // local cq
  lci::comp_t cq = lci::alloc_cq();
  // prepare buffer
  size_t msg_size = lci::get_max_bcopy_size();
  void* send_buffer = malloc(msg_size);
  void* recv_buffer = malloc(msg_size);
  std::memset(send_buffer, 65, msg_size);
  // register recv buffer
  lci::mr_t mr = lci::register_memory(send_buffer, msg_size);
  lci::rkey_t rkey = lci::get_rkey(mr);

  // loopback message
  for (int i = 0; i < nmsgs; i++) {
    lci::status_t status;
    do {
      status = lci::post_get(rank,
                             recv_buffer,
                             msg_size,
                             cq,
                             reinterpret_cast<uintptr_t>(send_buffer),
                             rkey);
      lci::progress();
    } while (status.is_retry());

    if (status.is_posted()) {
      do {
        lci::progress();
        status = lci::cq_pop(cq);
      } while (status.is_retry());
    }

    for (size_t j = 0; j < msg_size; ++j) {
      ASSERT_EQ(static_cast<char*>(recv_buffer)[j], 65);
    }
  }
  // clean up
  lci::deregister_memory(&mr);
  free(send_buffer);
  free(recv_buffer);
  lci::free_comp(&cq);
  lci::g_runtime_fina();
}

TEST(TestLCI, one_sided) {}

int main(int argc, char* argv[]) {
  int result = 0;

  ::testing::InitGoogleTest(&argc, argv);
  MPI_Init(&argc, &argv);
  result = RUN_ALL_TESTS();
  MPI_Finalize();

  return result;
}