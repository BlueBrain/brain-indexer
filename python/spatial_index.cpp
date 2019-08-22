#include "spatial_index.hpp"


struct py_sphere_rtree : public py_rtree<si::IndexedSphere>{
    using rtree_type = py_rtree<si::IndexedSphere>;

    inline void make_bindings(py::module& m) {
        // Note: py_sphere_rtree now doesnt need to add specific Soma overloads, all generic.
        init_class_bindings(m, "SphereIndex");
    }
};


struct py_morph_rtree: public py_rtree<si::MorphoEntry, si::Soma> {
    using rtree_type = py_rtree<si::MorphoEntry, si::Soma>;

    inline static void add_branch(Class& obj,
                                  id_t neuron_id, unsigned segment_i, unsigned n_segments,
                                  const point_t *points, const coord_t *radii) {
        for(int i=0; i < n_segments; i++, segment_i++) {
            obj.insert(si::Segment{
                neuron_id, segment_i, points[i], points[i+1], radii[i]});
        }
    }

    inline void make_bindings(py::module& m) {
        init_class_bindings(m, "MorphIndex")
            .def(
                "insert",
                [](Class& obj, id_t gid, unsigned segment_i,
                               array_t p1, array_t p2, coord_t radius) {
                    obj.insert(si::Segment{gid, segment_i, mk_point(p1), mk_point(p2), radius});
                },
                R"(
                    Inserts a new segment object in the tree.

                    Args:
                        gid(int): The id of the neuron
                        segment_i(int): The id of the segment
                        p1(array): A len-3 list or np.array[float32] with the cylinder first point
                        p2(array): A len-3 list or np.array[float32] with the cylinder second point
                        radius(float): The radius of the cylinder
                )")

            .def(
                "place",
                [](Class& obj, array_t region_corners,
                               id_t gid, unsigned segment_i,
                               array_t p1, array_t p2, coord_t radius) {
                    if (region_corners.ndim() != 2 || region_corners.size() != 6) {
                        throw std::invalid_argument("Please provide a 2x3[float32] array");
                    }
                    const coord_t *c0 = region_corners.data(0, 0);
                    const coord_t *c1 = region_corners.data(1, 0);
                    return obj.place(
                        si::Box3D{point_t(c0[0], c0[1], c0[2]), point_t(c1[0], c1[1], c1[2])},
                        si::Segment{gid, segment_i, mk_point(p1), mk_point(p2), radius}
                    );
                },
                R"(
                    Attempts at inserting a segment without overlapping any existing shape.

                    Args:
                        region_corners(array): A 2x3 list/np.array of the region corners.\
                            E.g. region_corners[0] is the 3D min_corner point.
                        gid(int): The id of the neuron
                        segment_i(int): The id of the segment
                        p1(array): A len-3 list or np.array[float32] with the cylinder first point
                        p2(array): A len-3 list or np.array[float32] with the cylinder second point
                        radius(float): The radius of the cylinder
                )")

            .def(
                "add_branch",
                [](Class& obj, id_t gid, unsigned segment_i,
                               array_t centroids_np, array_t radii_np) {
                    auto point_radii = convert_input(centroids_np, radii_np);
                    add_branch(obj, gid, segment_i, radii_np.size() - 1,
                               point_radii.first, point_radii.second.data(0)
                    );
                },
                R"(
                    Adds a branch, i.e., a line of cylinders.

                    It adds a line of cylinders representing a branch. Each point in the centroids
                    array is the begginning/end of a segment, and therefore it must be length N+1,
                    where N is thre number of created cylinders.

                    Args:
                        gid(int): The id of the soma
                        segment_i(int): The id of the first segment of the branch
                        centroids_np(np.array): A Nx3 array[float32] of the segments' end points
                        radii_np(np.array): An array[float32] with the segments' radii
                )")

            .def(
                "add_neuron",
                [](Class& obj, id_t gid,
                               array_t centroids_np, array_t radii_np,
                               array_offsets branches_offset_np) {
                    auto point_radii = convert_input(centroids_np, radii_np);
                    // Get raw pointers to data
                    const auto points = point_radii.first;
                    const auto radii = point_radii.second.data(0);
                    auto n_branches = branches_offset_np.size();
                    const auto offsets = branches_offset_np.template unchecked<1>().data(0);

                    // Add soma
                    obj.insert(si::Soma{gid, points[0], radii[0]});

                    // Add segments
                    int cur_segment_i = 1;
                    for (unsigned branch_i=0; branch_i < n_branches - 1; branch_i++) {
                        unsigned p_start = offsets[branch_i];
                        unsigned n_segments = offsets[branch_i + 1] - p_start - 1;
                        add_branch(obj, gid, cur_segment_i, n_segments,
                                        points + p_start,
                                        radii + p_start);
                        cur_segment_i += n_segments;
                    }
                    // Last
                    unsigned p_start = offsets[n_branches - 1];
                    unsigned n_segments = radii_np.size() - p_start - 1;
                    add_branch(obj, gid, cur_segment_i, n_segments,
                                    points + p_start,
                                    radii + p_start);
                },
                R"(
                    Bulk add a neuron (1 soma and lines of segments) to the spatial index.

                    It interprets the first point & radius as the soma properties. Subsequent
                    points & radii are interpreted as branch segments (cylinders).
                    The first point (index) of each branch must be specified in branches_offset_np,
                    so that a new branch is started without connecting it to the last segment.

                    **Example:** Adding a neuron with two branches.
                      With 1 soma, first branch with 9 segments and second branch with 5::

                        ( S ).=.=.=.=.=.=.=.=.=.
                                  .=.=.=.=.=.

                      Implies 16 points. ('S' and '.'), and branches starting at points 1 and 11
                      It can be created in the following way:

                      >>> points = np.zeros([16, 3], dtype=np.float32)
                      >>> points[:, 0] = np.concatenate((np.arange(11), np.arange(4, 10)))
                      >>> points[11:, 1] = 1.0  # Change Y coordinate
                      >>> radius = np.ones(N, dtype=np.float32)
                      >>> rtree = MorphIndex()
                      >>> rtree.add_neuron(1, points, radius, [1, 11])

                    **Note:** There is not the concept of branching off from previous points.
                    All branches start in a new point, the user can however provide a point
                    close to an exisitng point to mimick branching.

                    Args:
                        gid(int): The id of the soma
                        centroids_np(np.array): A Nx3 array[float32] of the segments' end points
                        radii_np(np.array): An array[float32] with the segments' radii
                        branches_offset_np(array): A list/array[int] with the offset to
                            the first point of each branch
                )");
    }

};


PYBIND11_MODULE(_spatial_index, m) {
    PYBIND11_NUMPY_DTYPE(si::gid_segm_t, gid, segment_i);  // Pybind11 wow!

    py_sphere_rtree{}.make_bindings(m);
    py_morph_rtree{}.make_bindings(m);
}
