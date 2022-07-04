#define BOOST_TEST_NO_MAIN
#define BOOST_TEST_MODULE SpatialIndex_UnitTests
#include <boost/test/unit_test.hpp>
namespace bt = boost::unit_test;

#include <spatial_index/mpi_wrapper.hpp>
#include <spatial_index/distributed_sorting.hpp>
#include <spatial_index/sort_tile_recursion.hpp>

#include <array>
#include <vector>
#include <random>

using namespace spatial_index;

struct Sortable {
    double value;
    std::array<int, 2> payload;
};

std::vector<Sortable> random_values(size_t n, int tag) {
    auto gen = std::default_random_engine{};
    std::uniform_int_distribution<int> int_dist(0, 1000);
    std::uniform_real_distribution<double> real_dist(-100.0, 100.0);

    auto values = std::vector<Sortable>{};
    values.reserve(n);

    for (size_t i = 0; i < n; ++i) {
        values.push_back(Sortable{real_dist(gen), {tag, int(i)}});
    }

    return values;
}

class GetValue {
public:
    static double apply(const Sortable &v) {
        return v.value;
    }
};

BOOST_AUTO_TEST_CASE(DistributedSortingTests) {
    auto mpi_rank = spatial_index::mpi::rank(MPI_COMM_WORLD);
    auto comm_size = spatial_index::mpi::size(MPI_COMM_WORLD);

    int n_required_ranks = 2;
    if(comm_size < n_required_ranks) {
        throw std::runtime_error("Expected at least 2 MPI ranks.");
    }

    auto comm = mpi::comm_split(MPI_COMM_WORLD, mpi_rank < n_required_ranks, mpi_rank);

    if(mpi_rank >= n_required_ranks) {
        return;
    }

    size_t n_r0 = 100ul; // size on rank 0
    size_t n_r1 = 2 * n_r0; // size on rank 1

    auto m_r0 = (n_r0 + n_r1 + 1)/2;
    auto m_r1 = (n_r0 + n_r1)/2;

    auto unsorted = random_values((mpi_rank == 0 ? n_r0 : n_r1), mpi_rank);
    auto sorted = unsorted;  // force a copy.

    using DMS = spatial_index::DistributedMemorySorter<Sortable, GetValue>;
    DMS::sort_and_balance(sorted, *comm);

    // Round up division on rank 0, but not on rank 1.
    auto m_expected = (mpi_rank == 0 ? m_r0 : m_r1);

    std::cout << "n_expected = " << sorted.size() << " / " << m_expected << "\n" << std::flush;
    if(m_expected != sorted.size()) {
        throw std::runtime_error("Incorrect size.");
    }

    for(size_t i = 0; i < m_expected - 1; ++i) {
        if(sorted[i].value > sorted[i+1].value) {
            throw std::runtime_error("Incorrectly sorted.");
        }
    }

    auto combined = std::vector<Sortable>(n_r0 + n_r1);
    auto mpi_sortable = mpi::Datatype(mpi::create_contiguous_datatype<Sortable>());
    if(mpi_rank == 0) {
        MPI_Send(sorted.data(),          m_r0, *mpi_sortable, 1, 0, *comm);
        MPI_Recv(combined.data() + m_r0, m_r1, *mpi_sortable, 1, 0, *comm, MPI_STATUS_IGNORE);
        std::copy(sorted.begin(), sorted.end(), combined.begin());
    }
    else {
        MPI_Recv(combined.data(), m_r0, *mpi_sortable, 0, 0, *comm, MPI_STATUS_IGNORE);
        MPI_Send(sorted.data(),   m_r1, *mpi_sortable, 0, 0, *comm);
        std::copy(sorted.begin(), sorted.end(), combined.begin() + m_r0);
    }

    auto is_unchanged = [](const Sortable &a, const Sortable &b) {
        return a.value == b.value && a.payload == b.payload;
    };

    auto sorted_counts = std::vector<int>(mpi_rank == 0 ? n_r0 : n_r1, 0);
    for(const auto &p : combined) {
        if(p.payload[0] == mpi_rank) {
            ++sorted_counts[p.payload[1]];

            const auto &expected = unsorted[p.payload[1]];

            if(!is_unchanged(p, expected)) {
                throw std::runtime_error("Value or payload changed.");
            }
        }
    }

    for(const auto &k : sorted_counts) {
        if(k != 1) {
            throw std::runtime_error("Something got lost.");
        }
    }
}


int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    auto return_code = bt::unit_test_main([](){ return true; }, argc, argv );

    MPI_Finalize();
    return return_code;
}
