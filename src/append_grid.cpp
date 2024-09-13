#include "LibytProcessControl.h"
#include "libyt.h"
#include "yt_combo.h"

#ifdef USE_PYBIND11
#include "pybind11/embed.h"
#endif

static int set_field_data(yt_grid* grid);
static int set_particle_data(yt_grid* grid);

//-------------------------------------------------------------------------------------------------------
// Function    :  append_grid
// Description :  Add a single full grid to the libyt Python module
//
// Note        :  1. Store the input "grid" to libyt.hierarchy, libyt.grid_data, libyt.particle_data.
//                2. When setting libyt.hierarchy:
//                   since grid id doesn't have to be 0-indexed (set g_param_yt.index_offset), but the
//                   hierarchy array starts at 0, we need to minus index_offset when setting hierarchy.
//                3. When setting libyt.grid_data and libyt.particle_data:
//                   always maintain the same grid passed in by simulation, which means it doesn't have
//                   to start from 0.
//                4. Called and use by yt_commit().
//
// Parameter   :  yt_grid *grid
//
// Return      :  YT_SUCCESS or YT_FAIL
//-------------------------------------------------------------------------------------------------------
int append_grid(yt_grid* grid) {
    // export grid info to libyt.hierarchy
#ifndef USE_PYBIND11
    PyArrayObject* py_array_obj;

// convenient macro
// note that PyDict_GetItemString() returns a **borrowed** reference ==> no need to call Py_DECREF
#define FILL_ARRAY(KEY, ARRAY, DIM, TYPE)                                                                              \
    {                                                                                                                  \
        for (int t = 0; t < DIM; t++) {                                                                                \
            if ((py_array_obj = (PyArrayObject*)PyDict_GetItemString(g_py_hierarchy, KEY)) == NULL)                    \
                YT_ABORT("Accessing the key \"%s\" from libyt.hierarchy ... failed!\n", KEY);                          \
                                                                                                                       \
            *(TYPE*)PyArray_GETPTR2(py_array_obj, (grid->id) - g_param_yt.index_offset, t) = (TYPE)(ARRAY)[t];         \
        }                                                                                                              \
    }

    FILL_ARRAY("grid_left_edge", grid->left_edge, 3, npy_double)
    FILL_ARRAY("grid_right_edge", grid->right_edge, 3, npy_double)
    FILL_ARRAY("grid_dimensions", grid->grid_dimensions, 3, npy_int)
    FILL_ARRAY("grid_parent_id", &grid->parent_id, 1, npy_long)
    FILL_ARRAY("grid_levels", &grid->level, 1, npy_int)
    FILL_ARRAY("proc_num", &grid->proc_num, 1, npy_int)
    if (g_param_yt.num_par_types > 0) {
        FILL_ARRAY("par_count_list", grid->par_count_list, g_param_yt.num_par_types, npy_long)
    }
#undef FILL_ARRAY
#else
    long index = grid->id - g_param_yt.index_offset;
    for (int d = 0; d < 3; d++) {
        LibytProcessControl::Get().grid_left_edge[index * 3 + d] = grid->left_edge[d];
        LibytProcessControl::Get().grid_right_edge[index * 3 + d] = grid->right_edge[d];
        LibytProcessControl::Get().grid_dimensions[index * 3 + d] = grid->grid_dimensions[d];
    }
    LibytProcessControl::Get().grid_parent_id[index] = grid->parent_id;
    LibytProcessControl::Get().grid_levels[index] = grid->level;
    LibytProcessControl::Get().proc_num[index] = grid->proc_num;
    if (g_param_yt.num_par_types > 0) {
        for (int p = 0; p < g_param_yt.num_par_types; p++) {
            LibytProcessControl::Get().par_count_list[index * g_param_yt.num_par_types + p] = grid->par_count_list[p];
        }
    }
#endif  // #ifndef USE_PYBIND11

    log_debug("Inserting grid [%ld] info to libyt.hierarchy ... done\n", grid->id);

    // export grid data and particle data to libyt.grid_data and libyt.particle_data
    if (grid->field_data != nullptr && set_field_data(grid) != YT_SUCCESS) {
        YT_ABORT("Failed to append grid [%ld] field data.\n", grid->id);
    }
    if (grid->particle_data != nullptr && set_particle_data(grid) != YT_SUCCESS) {
        YT_ABORT("Failed to append grid [%ld] particle data.\n", grid->id);
    }

    return YT_SUCCESS;
}

//-------------------------------------------------------------------------------------------------------
// Function    :  set_field_data
// Description :  Wrap data pointer and added under libyt.grid_data
//
// Note        :  1. libyt.grid_data[grid_id][field_list.field_name] = NumPy array or memoryview from field pointer.
//                2. Append to dictionary only when there is data pointer passed in.
//
// Parameter   :  yt_grid *grid
//
// Return      :  YT_SUCCESS or YT_FAIL
//-------------------------------------------------------------------------------------------------------
static int set_field_data(yt_grid* grid) {
    yt_field* field_list = LibytProcessControl::Get().field_list;

#ifndef USE_PYBIND11
    PyObject *py_grid_id, *py_field_labels, *py_field_data;
    py_grid_id = PyLong_FromLong(grid->id);
    py_field_labels = PyDict_New();
    for (int v = 0; v < g_param_yt.num_fields; v++) {
        // append data to dict only if data is not NULL.
        if ((grid->field_data)[v].data_ptr == nullptr) continue;

        // check if dictionary exists, if no add new dict under key gid
        if (PyDict_Contains(g_py_grid_data, py_grid_id) != 1) {
            PyDict_SetItem(g_py_grid_data, py_grid_id, py_field_labels);
        }

        // insert data under py_field_labels dict
        // (1) Grab NumPy Enumerate Type in order: (1)data_dtype (2)field_dtype
        int grid_dtype;
        if (get_npy_dtype((grid->field_data)[v].data_dtype, &grid_dtype) == YT_SUCCESS) {
            log_debug("Grid ID [ %ld ], field data [ %s ], grab NumPy enumerate type from data_dtype.\n", grid->id,
                      field_list[v].field_name);
        } else if (get_npy_dtype(field_list[v].field_dtype, &grid_dtype) == YT_SUCCESS) {
            (grid->field_data)[v].data_dtype = field_list[v].field_dtype;
            log_debug("Grid ID [ %ld ], field data [ %s ], grab NumPy enumerate type from field_dtype.\n", grid->id,
                      field_list[v].field_name);
        } else {
            Py_DECREF(py_grid_id);
            Py_DECREF(py_field_labels);
            YT_ABORT("Grid ID [ %ld ], field data [ %s ], cannot get the NumPy enumerate type properly.\n", grid->id,
                     field_list[v].field_name);
        }

        // (2) Get the dimension of the input array
        // Only "cell-centered" will be set to grid_dimensions + ghost cell, else should be set in data_dimensions.
        if (strcmp(field_list[v].field_type, "cell-centered") == 0) {
            // Get grid_dimensions and consider contiguous_in_x or not, since grid_dimensions is defined as [x][y][z].
            if (field_list[v].contiguous_in_x) {
                for (int d = 0; d < 3; d++) {
                    (grid->field_data)[v].data_dimensions[d] = (grid->grid_dimensions)[2 - d];
                }
            } else {
                for (int d = 0; d < 3; d++) {
                    (grid->field_data)[v].data_dimensions[d] = (grid->grid_dimensions)[d];
                }
            }
            // Plus the ghost cell to get the actual array dimensions.
            for (int d = 0; d < 6; d++) {
                (grid->field_data)[v].data_dimensions[d / 2] += field_list[v].field_ghost_cell[d];
            }
        }
        // See if all data_dimensions > 0, abort if not.
        for (int d = 0; d < 3; d++) {
            if ((grid->field_data)[v].data_dimensions[d] <= 0) {
                Py_DECREF(py_grid_id);
                Py_DECREF(py_field_labels);
                YT_ABORT("Grid ID [ %ld ], field data [ %s ], data_dimensions[%d] = %d <= 0.\n", grid->id,
                         field_list[v].field_name, d, (grid->field_data)[v].data_dimensions[d]);
            }
        }

        npy_intp grid_dims[3] = {(grid->field_data)[v].data_dimensions[0], (grid->field_data)[v].data_dimensions[1],
                                 (grid->field_data)[v].data_dimensions[2]};

        // (3) Insert data to dict
        // PyArray_SimpleNewFromData simply creates an array wrapper and does not allocate and own the array
        py_field_data = PyArray_SimpleNewFromData(3, grid_dims, grid_dtype, (grid->field_data)[v].data_ptr);

        // Mark this memory (NumPy array) read-only
        PyArray_CLEARFLAGS((PyArrayObject*)py_field_data, NPY_ARRAY_WRITEABLE);

        // add the field data to dict "libyt.grid_data[grid_id][field_list.field_name]"
        PyDict_SetItemString(py_field_labels, field_list[v].field_name, py_field_data);

        // call decref since PyDict_SetItemString() returns a new reference
        Py_DECREF(py_field_data);

        log_debug("Inserting grid [%ld] field data [%s] to libyt.grid_data ... done\n", grid->id,
                  field_list[v].field_name);
    }

    // call decref since both PyLong_FromLong() and PyDict_New() return a new reference
    Py_DECREF(py_grid_id);
    Py_DECREF(py_field_labels);
#else
    pybind11::module_ libyt = pybind11::module_::import("libyt");
    pybind11::dict py_grid_data = libyt.attr("grid_data");

    for (int v = 0; v < g_param_yt.num_fields; v++) {
        // append data to dict only if data is not NULL.
        if ((grid->field_data)[v].data_ptr == nullptr) continue;

        // check if libyt.grid_data[gid] dict exist, if not, create a new dict
        pybind11::dict py_field_data;
        if (!py_grid_data.contains(pybind11::int_(grid->id))) {
            py_field_data = pybind11::dict();
            py_grid_data[pybind11::int_(grid->id)] = py_field_data;
        } else {
            py_field_data = py_grid_data[pybind11::int_(grid->id)];
        }

        // insert field data to py_field_data
        // (1) Get array element sizeof
        int dtype_size;
        if (get_dtype_size((grid->field_data)[v].data_dtype, &dtype_size) == YT_SUCCESS) {
            log_debug("Grid ID [ %ld ], field data [ %s ], grab data size from data_dtype.\n", grid->id,
                      field_list[v].field_name);
        } else if (get_dtype_size(field_list[v].field_dtype, &dtype_size) == YT_SUCCESS) {
            (grid->field_data)[v].data_dtype = field_list[v].field_dtype;
            log_debug("Grid ID [ %ld ], field data [ %s ], grab data size from field_dtype.\n", grid->id,
                      field_list[v].field_name);
        } else {
            YT_ABORT("Grid ID [ %ld ], field data [ %s ], cannot get data size properly.\n", grid->id,
                     field_list[v].field_name);
        }

        // (2) Get the dimension of the cell-centered array, considering contiguous_in_x and ghost cell
        if (strcmp(field_list[v].field_type, "cell-centered") == 0) {
            if (field_list[v].contiguous_in_x) {
                for (int d = 0; d < 3; d++) {
                    (grid->field_data)[v].data_dimensions[d] = (grid->grid_dimensions)[2 - d];
                }
            } else {
                for (int d = 0; d < 3; d++) {
                    (grid->field_data)[v].data_dimensions[d] = (grid->grid_dimensions)[d];
                }
            }
            // Plus the ghost cell to get the actual array dimensions.
            for (int d = 0; d < 6; d++) {
                (grid->field_data)[v].data_dimensions[d / 2] += field_list[v].field_ghost_cell[d];
            }
        }
        for (int d = 0; d < 3; d++) {
            if ((grid->field_data)[v].data_dimensions[d] <= 0) {
                YT_ABORT("Grid ID [ %ld ], field data [ %s ], data_dimensions[%d] = %d <= 0.\n", grid->id,
                         field_list[v].field_name, d, (grid->field_data)[v].data_dimensions[d]);
            }
        }

        // (3) Wrap memory buffer and insert to py_field_data
        // Step 1 and 2 make sure that data_dimensions and data_dtype are set properly
        int* data_dim = (grid->field_data)[v].data_dimensions;
        switch ((grid->field_data)[v].data_dtype) {
            case YT_FLOAT:
                py_field_data[field_list[v].field_name] = pybind11::memoryview::from_buffer(
                    static_cast<float*>((grid->field_data)[v].data_ptr), {data_dim[0], data_dim[1], data_dim[2]},
                    {dtype_size * data_dim[1] * data_dim[2], dtype_size * data_dim[2], dtype_size}, true);
                break;
            case YT_DOUBLE:
                py_field_data[field_list[v].field_name] = pybind11::memoryview::from_buffer(
                    static_cast<double*>((grid->field_data)[v].data_ptr), {data_dim[0], data_dim[1], data_dim[2]},
                    {dtype_size * data_dim[1] * data_dim[2], dtype_size * data_dim[2], dtype_size}, true);
                break;
            case YT_LONGDOUBLE:
                py_field_data[field_list[v].field_name] = pybind11::memoryview::from_buffer(
                    static_cast<long double*>((grid->field_data)[v].data_ptr), {data_dim[0], data_dim[1], data_dim[2]},
                    {dtype_size * data_dim[1] * data_dim[2], dtype_size * data_dim[2], dtype_size}, true);
                break;
            case YT_CHAR:
                py_field_data[field_list[v].field_name] = pybind11::memoryview::from_buffer(
                    static_cast<char*>((grid->field_data)[v].data_ptr), {data_dim[0], data_dim[1], data_dim[2]},
                    {dtype_size * data_dim[1] * data_dim[2], dtype_size * data_dim[2], dtype_size}, true);
                break;
            case YT_UCHAR:
                py_field_data[field_list[v].field_name] = pybind11::memoryview::from_buffer(
                    static_cast<unsigned char*>((grid->field_data)[v].data_ptr),
                    {data_dim[0], data_dim[1], data_dim[2]},
                    {dtype_size * data_dim[1] * data_dim[2], dtype_size * data_dim[2], dtype_size}, true);
                break;
            case YT_SHORT:
                py_field_data[field_list[v].field_name] = pybind11::memoryview::from_buffer(
                    static_cast<short*>((grid->field_data)[v].data_ptr), {data_dim[0], data_dim[1], data_dim[2]},
                    {dtype_size * data_dim[1] * data_dim[2], dtype_size * data_dim[2], dtype_size}, true);
                break;
            case YT_USHORT:
                py_field_data[field_list[v].field_name] = pybind11::memoryview::from_buffer(
                    static_cast<unsigned short*>((grid->field_data)[v].data_ptr),
                    {data_dim[0], data_dim[1], data_dim[2]},
                    {dtype_size * data_dim[1] * data_dim[2], dtype_size * data_dim[2], dtype_size}, true);
                break;
            case YT_INT:
                py_field_data[field_list[v].field_name] = pybind11::memoryview::from_buffer(
                    static_cast<int*>((grid->field_data)[v].data_ptr), {data_dim[0], data_dim[1], data_dim[2]},
                    {dtype_size * data_dim[1] * data_dim[2], dtype_size * data_dim[2], dtype_size}, true);
                break;
            case YT_UINT:
                py_field_data[field_list[v].field_name] = pybind11::memoryview::from_buffer(
                    static_cast<unsigned int*>((grid->field_data)[v].data_ptr), {data_dim[0], data_dim[1], data_dim[2]},
                    {dtype_size * data_dim[1] * data_dim[2], dtype_size * data_dim[2], dtype_size}, true);
                break;
            case YT_LONG:
                py_field_data[field_list[v].field_name] = pybind11::memoryview::from_buffer(
                    static_cast<long*>((grid->field_data)[v].data_ptr), {data_dim[0], data_dim[1], data_dim[2]},
                    {dtype_size * data_dim[1] * data_dim[2], dtype_size * data_dim[2], dtype_size}, true);
                break;
            case YT_ULONG:
                py_field_data[field_list[v].field_name] = pybind11::memoryview::from_buffer(
                    static_cast<unsigned long*>((grid->field_data)[v].data_ptr),
                    {data_dim[0], data_dim[1], data_dim[2]},
                    {dtype_size * data_dim[1] * data_dim[2], dtype_size * data_dim[2], dtype_size}, true);
                break;
            case YT_LONGLONG:
                py_field_data[field_list[v].field_name] = pybind11::memoryview::from_buffer(
                    static_cast<long long*>((grid->field_data)[v].data_ptr), {data_dim[0], data_dim[1], data_dim[2]},
                    {dtype_size * data_dim[1] * data_dim[2], dtype_size * data_dim[2], dtype_size}, true);
                break;
            case YT_ULONGLONG:
                py_field_data[field_list[v].field_name] = pybind11::memoryview::from_buffer(
                    static_cast<unsigned long long*>((grid->field_data)[v].data_ptr),
                    {data_dim[0], data_dim[1], data_dim[2]},
                    {dtype_size * data_dim[1] * data_dim[2], dtype_size * data_dim[2], dtype_size}, true);
                break;
            case YT_DTYPE_UNKNOWN:
                YT_ABORT("Grid ID [ %ld ], field data [ %s ], unknown data type.\n", grid->id,
                         field_list[v].field_name);
                break;
            default:
                YT_ABORT("Grid ID [ %ld ], field data [ %s ], unsupported data type.\n", grid->id,
                         field_list[v].field_name);
        }
    }

#endif  // #ifndef USE_PYBIND11

    return YT_SUCCESS;
}

//-------------------------------------------------------------------------------------------------------
// Function    :  set_particle_data
// Description :  Wrap data pointer and added under libyt.particle_data
//
// Note        :  1. libyt.particle_data[grid_id][particle_list.par_type][attr_name]
//                   = NumPy array or memoryview created through wrapping data pointer.
//                2. Append to dictionary only when there is data pointer passed in.
//                3. The logistic is we create dictionary no matter what, and only append under
//                   g_py_particle_data if there is data to wrap, so that ref count increases.
//
// Parameter   :  yt_grid *grid
//
// Return      :  YT_SUCCESS or YT_FAIL
//-------------------------------------------------------------------------------------------------------
static int set_particle_data(yt_grid* grid) {
    yt_particle* particle_list = LibytProcessControl::Get().particle_list;

#ifndef USE_PYBIND11
    PyObject *py_grid_id, *py_ptype_labels, *py_attributes, *py_data;
    py_grid_id = PyLong_FromLong(grid->id);
    py_ptype_labels = PyDict_New();
    for (int p = 0; p < g_param_yt.num_par_types; p++) {
        py_attributes = PyDict_New();
        for (int a = 0; a < particle_list[p].num_attr; a++) {
            // skip if particle attribute pointer is NULL
            if ((grid->particle_data)[p][a].data_ptr == nullptr) continue;

            // Wrap the data array if pointer exist
            int data_dtype;
            if (get_npy_dtype(particle_list[p].attr_list[a].attr_dtype, &data_dtype) != YT_SUCCESS) {
                log_error("Cannot get particle type [%s] attribute [%s] data type. Unable to wrap particle array\n",
                          particle_list[p].par_type, particle_list[p].attr_list[a].attr_name);
                continue;
            }
            if ((grid->par_count_list)[p] <= 0) {
                log_error("Cannot wrapped particle array with length %ld <= 0\n", (grid->par_count_list)[p]);
                continue;
            }
            npy_intp array_dims[1] = {(grid->par_count_list)[p]};
            py_data = PyArray_SimpleNewFromData(1, array_dims, data_dtype, (grid->particle_data)[p][a].data_ptr);
            PyArray_CLEARFLAGS((PyArrayObject*)py_data, NPY_ARRAY_WRITEABLE);

            // Get the dictionary and append py_data
            if (PyDict_Contains(g_py_particle_data, py_grid_id) != 1) {
                // 1st time append, nothing exist under libyt.particle_data[gid]
                PyDict_SetItem(g_py_particle_data, py_grid_id, py_ptype_labels);  // libyt.particle_data[gid] = dict()
                PyDict_SetItemString(py_ptype_labels, particle_list[p].par_type, py_attributes);
            } else {
                // libyt.particle_data[gid] exist, check if libyt.particle_data[gid][ptype] exist
                PyObject* py_ptype_name = PyUnicode_FromString(particle_list[p].par_type);
                if (PyDict_Contains(py_ptype_labels, py_ptype_name) != 1) {
                    PyDict_SetItemString(py_ptype_labels, particle_list[p].par_type, py_attributes);
                }
                Py_DECREF(py_ptype_name);
            }
            PyDict_SetItemString(py_attributes, particle_list[p].attr_list[a].attr_name, py_data);
            Py_DECREF(py_data);

            // debug message
            log_debug("Inserting grid [%ld] particle [%s] attribute [%s] data to libyt.particle_data ... done\n",
                      grid->id, particle_list[p].par_type, particle_list[p].attr_list[a].attr_name);
        }
        Py_DECREF(py_attributes);
    }

    Py_DECREF(py_ptype_labels);
    Py_DECREF(py_grid_id);
#else
    pybind11::module_ libyt = pybind11::module_::import("libyt");
    pybind11::dict py_particle_data = libyt.attr("particle_data");

    for (int p = 0; p < g_param_yt.num_par_types; p++) {
        for (int a = 0; a < particle_list[p].num_attr; a++) {
            // skip if particle attribute pointer is NULL
            if ((grid->particle_data)[p][a].data_ptr == nullptr) continue;

            // Get and check data type size and length
            int dtype_size;
            if (get_dtype_size(particle_list[p].attr_list[a].attr_dtype, &dtype_size) != YT_SUCCESS) {
                YT_ABORT("Cannot get particle type [%s] attribute [%s] data type. Unable to wrap particle array.\n",
                         particle_list[p].par_type, particle_list[p].attr_list[a].attr_name);
            }
            if ((grid->par_count_list)[p] <= 0) {
                YT_ABORT("Cannot wrapped particle array with length %ld <= 0\n", (grid->par_count_list)[p]);
            }

            // Wrap the data to memoryview and bind to libyt.particle_data
            pybind11::dict py_attr_data;
            if (!py_particle_data.contains(pybind11::int_(grid->id))) {
                py_attr_data = pybind11::dict();
                py_particle_data[pybind11::int_(grid->id)] = pybind11::dict();
                py_particle_data[pybind11::int_(grid->id)][particle_list[p].par_type] = py_attr_data;
            } else {
                if (!py_particle_data[pybind11::int_(grid->id)].contains(particle_list[p].par_type)) {
                    py_attr_data = pybind11::dict();
                    py_particle_data[pybind11::int_(grid->id)][particle_list[p].par_type] = py_attr_data;
                } else {
                    py_attr_data = py_particle_data[pybind11::int_(grid->id)][particle_list[p].par_type];
                }
            }

            const char* attr_name = particle_list[p].attr_list[a].attr_name;
            switch (particle_list[p].attr_list[a].attr_dtype) {
                case YT_FLOAT:
                    py_attr_data[attr_name] =
                        pybind11::memoryview::from_buffer(static_cast<float*>((grid->particle_data)[p][a].data_ptr),
                                                          {(grid->par_count_list)[p]}, {dtype_size}, true);
                    break;
                case YT_DOUBLE:
                    py_attr_data[attr_name] =
                        pybind11::memoryview::from_buffer(static_cast<double*>((grid->particle_data)[p][a].data_ptr),
                                                          {(grid->par_count_list)[p]}, {dtype_size}, true);
                    break;
                case YT_LONGDOUBLE:
                    py_attr_data[attr_name] = pybind11::memoryview::from_buffer(
                        static_cast<long double*>((grid->particle_data)[p][a].data_ptr), {(grid->par_count_list)[p]},
                        {dtype_size}, true);
                    break;
                case YT_CHAR:
                    py_attr_data[attr_name] =
                        pybind11::memoryview::from_buffer(static_cast<char*>((grid->particle_data)[p][a].data_ptr),
                                                          {(grid->par_count_list)[p]}, {dtype_size}, true);
                    break;
                case YT_UCHAR:
                    py_attr_data[attr_name] = pybind11::memoryview::from_buffer(
                        static_cast<unsigned char*>((grid->particle_data)[p][a].data_ptr), {(grid->par_count_list)[p]},
                        {dtype_size}, true);
                    break;
                case YT_SHORT:
                    py_attr_data[attr_name] =
                        pybind11::memoryview::from_buffer(static_cast<short*>((grid->particle_data)[p][a].data_ptr),
                                                          {(grid->par_count_list)[p]}, {dtype_size}, true);
                    break;
                case YT_USHORT:
                    py_attr_data[attr_name] = pybind11::memoryview::from_buffer(
                        static_cast<unsigned short*>((grid->particle_data)[p][a].data_ptr), {(grid->par_count_list)[p]},
                        {dtype_size}, true);
                    break;
                case YT_INT:
                    py_attr_data[attr_name] =
                        pybind11::memoryview::from_buffer(static_cast<int*>((grid->particle_data)[p][a].data_ptr),
                                                          {(grid->par_count_list)[p]}, {dtype_size}, true);
                    break;
                case YT_UINT:
                    py_attr_data[attr_name] = pybind11::memoryview::from_buffer(
                        static_cast<unsigned int*>((grid->particle_data)[p][a].data_ptr), {(grid->par_count_list)[p]},
                        {dtype_size}, true);
                    break;
                case YT_LONG:
                    py_attr_data[attr_name] =
                        pybind11::memoryview::from_buffer(static_cast<long*>((grid->particle_data)[p][a].data_ptr),
                                                          {(grid->par_count_list)[p]}, {dtype_size}, true);
                    break;
                case YT_ULONG:
                    py_attr_data[attr_name] = pybind11::memoryview::from_buffer(
                        static_cast<unsigned long*>((grid->particle_data)[p][a].data_ptr), {(grid->par_count_list)[p]},
                        {dtype_size}, true);
                    break;
                case YT_LONGLONG:
                    py_attr_data[attr_name] =
                        pybind11::memoryview::from_buffer(static_cast<long long*>((grid->particle_data)[p][a].data_ptr),
                                                          {(grid->par_count_list)[p]}, {dtype_size}, true);
                    break;
                case YT_ULONGLONG:
                    py_attr_data[attr_name] = pybind11::memoryview::from_buffer(
                        static_cast<unsigned long long*>((grid->particle_data)[p][a].data_ptr),
                        {(grid->par_count_list)[p]}, {dtype_size}, true);
                    break;
                case YT_DTYPE_UNKNOWN:
                    YT_ABORT("Grid ID [ %ld ], particle type [%s] attribute [ %s ], unknown data type.\n", grid->id,
                             particle_list[p].par_type, particle_list[p].attr_list[a].attr_name);
                    break;
                default:
                    YT_ABORT("Grid ID [ %ld ], particle type [%s] attribute [ %s ], unsupported data type.\n", grid->id,
                             particle_list[p].par_type, particle_list[p].attr_list[a].attr_name);
            }
        }
    }
#endif  // #ifndef USE_PYBIND11

    return YT_SUCCESS;
}