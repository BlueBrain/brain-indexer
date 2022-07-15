#pragma once

#include <spatial_index/distributed_sort_tile_recursion.hpp>

namespace spatial_index {

template <class Derived, class TopTree, class SubTree, class Filenames>
MultiIndexStorage<Derived, TopTree, SubTree, Filenames>::MultiIndexStorage(std::string output_dir)
    : output_dir(std::move(output_dir)) {
}
template <class Derived, class TopTree, class SubTree, class Filenames>
inline void
MultiIndexStorage<Derived, TopTree, SubTree, Filenames>::save_subtree(
    const SubTree& subtree,
    size_t subtree_id) const {

    save_subtree(subtree, output_dir, subtree_id);
}
template <class Derived, class TopTree, class SubTree, class Filenames>
inline void
MultiIndexStorage<Derived, TopTree, SubTree, Filenames>::save_subtree(
    const SubTree& subtree,
    const std::string& output_dir,
    size_t subtree_id) {

    Derived::save_tree(subtree, Filenames::subtree(output_dir, subtree_id));
}
template <class Derived, class TopTree, class SubTree, class Filenames>
inline void
MultiIndexStorage<Derived, TopTree, SubTree, Filenames>::save_top_tree(
    const TopTree& tree) const {

    save_top_tree(tree, output_dir);
}
template <class Derived, class TopTree, class SubTree, class Filenames>
inline void
MultiIndexStorage<Derived, TopTree, SubTree, Filenames>::save_top_tree(
    const TopTree& tree,
    const std::string& output_dir) {

    auto filename = Filenames::top_tree(output_dir);
    Derived::save_tree(tree, filename);
}

template <class Derived, class TopTree, class SubTree, class Filenames>
inline SubTree
MultiIndexStorage<Derived, TopTree, SubTree, Filenames>::load_subtree(
    size_t subtree_id) const {

    return load_subtree(output_dir, subtree_id);
}

template <class Derived, class TopTree, class SubTree, class Filenames>
inline SubTree
MultiIndexStorage<Derived, TopTree, SubTree, Filenames>::load_subtree(
    const std::string& output_dir,
    size_t subtree_id) {

    return Derived::template load_tree<SubTree>(
        Filenames::subtree(output_dir, subtree_id)
    );
}

template <class Derived, class TopTree, class SubTree, class Filenames>
inline TopTree
MultiIndexStorage<Derived, TopTree, SubTree, Filenames>::load_top_tree() const {
    return load_top_tree(output_dir);
}

template <class Derived, class TopTree, class SubTree, class Filenames>
inline TopTree
MultiIndexStorage<Derived, TopTree, SubTree, Filenames>::load_top_tree(
    const std::string& output_dir) {

    return Derived::template load_tree<TopTree>(Filenames::top_tree(output_dir));
}

template <class TopTree, class SubTree>
inline
NativeStorage<TopTree, SubTree>::NativeStorage(std::string output_dir)
    : super(std::move(output_dir)) {}


template <class TopTree, class SubTree>
template <class RTree>
inline void
NativeStorage<TopTree, SubTree>::save_tree(const RTree& rtree,
                                           const std::string& filename) {

    save_tree_impl(rtree, filename);
    util::check_signals();
}

template <class TopTree, class SubTree>
template <class RTree>
inline RTree
NativeStorage<TopTree, SubTree>::load_tree(const std::string& filename) {
    RTree rtree;
    load_tree_impl(rtree, filename);
    util::check_signals();

    return rtree;
}

template <class TopTree, class SubTree>
template <class... Args>
inline void
NativeStorage<TopTree, SubTree>::load_tree_impl(bgi::rtree<Args...>& tree,
                                                const std::string& filename) {
    std::ifstream ifs(filename, std::ios::binary);
    boost::archive::binary_iarchive ia(ifs);
    ia >> tree;
}

template <class TopTree, class SubTree>
template <class... Args>
inline void
NativeStorage<TopTree, SubTree>::save_tree_impl(const bgi::rtree<Args...>& tree,
                                                const std::string& filename) {
    std::ofstream ofs(filename, std::ios::binary | std::ios::trunc);
    boost::archive::binary_oarchive oa(ofs);
    oa << tree;
}


template <class Storage>
inline double
UsageRateCache<Storage>::MetaData::usage_rate(size_t query_count) const {
    if (query_count == load_generation_) {
        // These were loaded during this query. Try not to evict these. However,
        // it's safe to evict these since the subtree that will be queried next
        // will be loaded after this eviction; and therefore can't be evicted
        // before it's ever used.
        return std::numeric_limits<double>::max();
    }

    return double(access_count()) / double(incache_count(query_count));
}

template <class Storage>
inline size_t
UsageRateCache<Storage>::MetaData::access_count() const {
    return previous_access_count_ + current_access_count_;
}

template <class Storage>
inline size_t
UsageRateCache<Storage>::MetaData::incache_count(size_t query_count) const {
    return (query_count - load_generation_ + 1) + previous_age_;
}

template <class Storage>
inline size_t
UsageRateCache<Storage>::MetaData::eviction_count() const {
    return eviction_count_;
}

template <class Storage>
inline void
UsageRateCache<Storage>::MetaData::on_query() {
    ++current_access_count_;
}

template <class Storage>
inline void
UsageRateCache<Storage>::MetaData::on_load(size_t query_count) {
    load_generation_ = query_count;
    current_access_count_ = 1;
}

template <class Storage>
inline void
UsageRateCache<Storage>::MetaData::on_evict(size_t query_count) {
    previous_access_count_ += current_access_count_;
    previous_age_ = query_count - load_generation_ + 1;

    current_access_count_ = 0;
    eviction_count_ += 1;
}


template <class Storage>
UsageRateCache<Storage>::~UsageRateCache() {
    nlohmann::json j;
    for(const auto &kv : meta_data) {
        const auto & id = kv.first;
        const auto & md = kv.second;

        j.push_back({
            { "id", id },
            { "access_count", md.access_count() },
            { "eviction_count", md.eviction_count() },
            { "incache_count", md.incache_count(most_recent_query_count) },
            { "usage_rate", md.usage_rate(most_recent_query_count) }
        });
    }

    auto filename = "si_cache_stats_" + util::iso_datetime_now() + ".json";
    auto o = std::ofstream(filename);
    o << std::setw(4) << j << std::endl;
}


template <class Storage>
template<class SubtreeID>
inline auto
UsageRateCache<Storage>::load_subtree(const SubtreeID& subtree_id, size_t query_count)
        -> const subtree_type& {

    most_recent_query_count = query_count;
    auto id = subtree_id.id;

    const auto& found = subtrees.find(id);
    if (found == subtrees.end()) {
        evict_subtrees(subtree_id, query_count);

        meta_data[id].on_load(query_count);
        return subtrees[id] = storage.load_subtree(id);
    }

    meta_data[found->first].on_query();
    return found->second;
}

template <class Storage>
inline size_t
UsageRateCache<Storage>::cached_elements() const {
    size_t n_cached_elements = 0ul;
    for(const auto &kv: subtrees) {
        n_cached_elements += kv.second.size();
    }

    return n_cached_elements;
}


template <class Storage>
template<class SubtreeID>
inline void
UsageRateCache<Storage>::evict_subtrees(const SubtreeID& subtree_id,
                                        size_t query_count) {
    auto n_cached_elements = cached_elements();
    auto n_elements = subtree_id.n_elements;

    if (n_cached_elements + n_elements <= cache_params.max_cached_elements) {
        return;
    }

    std::cout << "eviction required: "
              << subtree_id.id << ", "
              << n_cached_elements << " / "
              << cache_params.max_cached_elements << "\n";

    auto loaded_subtree_ids = subtree_ids_sorted_by_usage_rate(query_count);

    for (size_t k = 0; k < cache_params.max_evict; ++k) {
        size_t i = loaded_subtree_ids[k];
        auto it = subtrees.find(i);
        if (it == subtrees.end()) {
            throw std::runtime_error("Failed to find a supposedly loaded subtree.");
        }

        meta_data[i].on_evict(query_count);
        subtrees.erase(it);
    }
}


template <class Storage>
inline std::vector<size_t>
UsageRateCache<Storage>::subtree_ids_sorted_by_usage_rate(size_t query_count) {
    std::vector<size_t> loaded_subtree_ids;
    loaded_subtree_ids.reserve(subtrees.size());

    for (const auto& kv: subtrees) {
        loaded_subtree_ids.push_back(kv.first);
    }

    std::sort(loaded_subtree_ids.begin(),
              loaded_subtree_ids.end(),
              [&](const auto& il, const auto& ir) {
                  auto usage_rate_left = meta_data[il].usage_rate(query_count);
                  auto usage_rate_right = meta_data[ir].usage_rate(query_count);

                  return usage_rate_left < usage_rate_right;
              });

    return loaded_subtree_ids;
}


template <class SubtreeCache>
MultiIndexTreeBase<SubtreeCache>::MultiIndexTreeBase(const storage_type& storage,
                                                     SubtreeCache subtree_cache)
    : top_rtree(storage.load_top_tree())
    , subtree_cache(std::move(subtree_cache)) {
}


template <class SubtreeCache>
template <class Predicates, class OutIt>
inline void
MultiIndexTreeBase<SubtreeCache>::query(const Predicates& predicates,
                                        const OutIt& it) const {
    auto to_query = std::vector<typename toptree_type::value_type>();
    top_rtree.query(predicates, std::back_inserter(to_query));

    for (const auto& value: to_query) {
        util::check_signals();
        query_subtree(value, predicates, it);
    }

    ++query_count;
}


template <class SubtreeCache>
template <class SubtreeID, class Predicates, class OutIt>
inline void
MultiIndexTreeBase<SubtreeCache>::query_subtree(const SubtreeID& subtree_id,
                                                const Predicates& predicates,
                                                const OutIt& it) const {

    const auto& subtree = load_subtree(subtree_id);
    subtree.query(predicates, it);
}


template <class SubtreeCache>
template <class SubtreeID>
inline auto
MultiIndexTreeBase<SubtreeCache>::load_subtree(const SubtreeID& subtree_id) const
        -> const subtree_type& {
    return subtree_cache.load_subtree(subtree_id, query_count);
}


template <typename T>
MultiIndexTree<T>::MultiIndexTree(const std::string& output_dir, size_t max_cached_bytes)
    : MultiIndexTree(NativeStorageT<T>(output_dir),
                     UsageRateCacheParams(max_cached_bytes / sizeof(value_type)))
{}


template <typename T>
MultiIndexTree<T>::MultiIndexTree(
    const NativeStorage<MultiIndexTopTreeT, MultiIndexSubTreeT<T>>& storage,
    const UsageRateCacheParams& params)
    : MultiIndexTree(storage, UsageRateCacheT<T>(params, storage))
{}


template <typename T>
template <typename ShapeT>
inline bool
MultiIndexTree<T>::is_intersecting(const ShapeT& shape) const {
    auto inner_sweep = [&shape](const auto &tree) {
        auto it = tree.qbegin(bgi::intersects(bgi::indexable<ShapeT>{}(shape)));
        for(; it != tree.qend(); ++it) {
            if(geometry_intersects(shape, *it)) {
                return true;
            }
        }

        return false;
    };

    auto it = this->top_rtree.qbegin(bgi::intersects(bgi::indexable<ShapeT>{}(shape)));
    for(; it != this->top_rtree.qend(); ++it) {
        if (geometry_intersects(shape, *it)) {
            const auto &tree = this->load_subtree(*it);

            if(inner_sweep(tree)) {
                return true;
            }
        }
    }
    return false;
}


template <typename T>
template <typename ShapeT>
inline auto
MultiIndexTree<T>::find_intersecting_objs(const ShapeT& shape) const
    -> std::vector<value_type> {

    std::vector<value_type> results;
    this->find_intersecting(shape, std::back_inserter(results));
    return results;
}


template <size_t dim, typename Value>
inline CoordType get_centroid_coordinate(const Value& value) {
    return value.template get_centroid_coord<dim>();
}


template<size_t dim, typename... VariantArgs>
inline CoordType get_centroid_coordinate(boost::variant<VariantArgs...> const& value) {
    return boost::apply_visitor(
        [](const auto& value) {
            return value.template get_centroid_coord<dim>();
        },
        value
    );
}

#if SI_MPI == 1

template <class Value>
MultiIndexBulkBuilder<Value>::MultiIndexBulkBuilder(std::string output_dir)
    : output_dir_(std::move(output_dir)) {

    util::ensure_valid_output_directory(output_dir_);
}


template <class Value>
template <class BeginIt, class EndIt>
inline void MultiIndexBulkBuilder<Value>::insert(BeginIt begin, EndIt end) {
    values_.insert(values_.end(), begin, end);
}


template <class Value>
inline void MultiIndexBulkBuilder<Value>::insert(const Value& value) {
    values_.push_back(value);
}


template <class Value>
inline void MultiIndexBulkBuilder<Value>::finalize(MPI_Comm comm) {
    auto comm_size = mpi::size(comm);

    size_t n_values = values_.size();
    size_t n_total_values = 0;
    MPI_Allreduce(&n_values, &n_total_values, 1, MPI_SIZE_T, MPI_SUM, comm);
    n_total_values_ = n_total_values;

    auto max_elements_per_part = size_t(4e6);

    auto str_params = two_level_str_heuristic(
        n_total_values,
        max_elements_per_part,
        comm_size
    );
    auto storage = NativeStorageT<Value>(output_dir_);
    using GetCoordinate = GetCenterCoordinate<Value>;
    distributed_partition<GetCoordinate>(storage, values_, str_params, comm);
}


template <class Value>
inline void MultiIndexBulkBuilder<Value>::reserve(size_t n_local_elements) {
    values_.reserve(n_local_elements);
}


template <class Value>
inline size_t MultiIndexBulkBuilder<Value>::size() const {
    if(!n_total_values_) {
        throw std::runtime_error("Total number of elements not yet known.");
    }

    return *n_total_values_;
}


template <class Value>
inline size_t MultiIndexBulkBuilder<Value>::local_size() const {
    return values_.size();
}
#endif

}