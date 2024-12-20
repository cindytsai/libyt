#include "libyt.h"
#include "libyt_process_control.h"
#include "yt_combo.h"

//-------------------------------------------------------------------------------------------------------
// Function    :  yt_get_GridsPtr
// Description :  Get pointer of the array of struct yt_grid with length num_grids_local.
//
// Note        :  1. User should call this function after yt_set_Parameters(),
//                   since we need num_grids_local, num_fields, and num_par_types.
//                2. Initialize field_data and particle_data in one grid with
//                   (1) data_dimensions[3] = {0, 0, 0}
//                   (2) data_ptr           = NULL
//                   (3) data_dtype         = YT_DTYPE_UNKNOWN
//                   field_data[0]       represents field_list[0]
//                   particle_data[0][1] represents particle_list[0].attr_list[1]
//                3. If user call this function twice, then it will just return the previously initialized
//                   and allocated array.
//
// Parameter   :  yt_grid **grids_local : Initialize and store the grid structure array under this
//                                        pointer points to.
//
// Return      :  YT_SUCCESS or YT_FAIL
//-------------------------------------------------------------------------------------------------------
int yt_get_GridsPtr(yt_grid** grids_local) {
    SET_TIMER(__PRETTY_FUNCTION__);

    // check if libyt has been initialized
    if (!LibytProcessControl::Get().libyt_initialized) {
        YT_ABORT("Please invoke yt_initialize() before calling %s()!\n", __FUNCTION__);
    }

    // check if yt_set_Parameters() have been called
    if (!LibytProcessControl::Get().param_yt_set) {
        YT_ABORT("Please invoke yt_set_Parameters() before calling %s()!\n", __FUNCTION__);
    }

    yt_param_yt& param_yt = LibytProcessControl::Get().param_yt_;

    // check if num_grids_local > 0, if not, grids_local won't be initialized
    if (param_yt.num_grids_local <= 0) {
        YT_ABORT("num_grids_local == %d <= 0, you don't need to input grids_local!\n", param_yt.num_grids_local);
    }

    log_info("Getting pointer to local grids information ...\n");

    // If user call for the first time.
    int mpi_rank = LibytProcessControl::Get().mpi_rank_;
    if (!LibytProcessControl::Get().get_gridsPtr) {
        // Initialize the grids_local array.
        // Set the value if overlapped with param_yt,
        // and each fields data are set to NULL, so that we can check if user input the data
        *grids_local = new yt_grid[param_yt.num_grids_local];
        yt_particle* particle_list = LibytProcessControl::Get().particle_list;
        for (int id = 0; id < param_yt.num_grids_local; id = id + 1) {
            (*grids_local)[id].proc_num = mpi_rank;

            if (param_yt.num_fields > 0) {
                // Array for storing pointers for fields in a grid
                (*grids_local)[id].field_data = new yt_data[param_yt.num_fields];
            } else {
                (*grids_local)[id].field_data = nullptr;
            }

            if (param_yt.num_par_types > 0) {
                // Array for storing pointers for different particle data attributes in a grid.
                // Ex: particle_data[0][1] represents particle_list[0].attr_list[1] data
                (*grids_local)[id].particle_data = new yt_data*[param_yt.num_par_types];
                for (int p = 0; p < param_yt.num_par_types; p++) {
                    (*grids_local)[id].particle_data[p] = new yt_data[particle_list[p].num_attr];
                }

                // Array for storing particle count in different particle type
                (*grids_local)[id].par_count_list = new long[param_yt.num_par_types];
                for (int s = 0; s < param_yt.num_par_types; s++) {
                    (*grids_local)[id].par_count_list[s] = 0;
                }
            } else {
                (*grids_local)[id].particle_data = nullptr;
                (*grids_local)[id].par_count_list = nullptr;
            }
        }

        LibytProcessControl::Get().grids_local = *grids_local;
    } else {
        // If user already called this function before, we just return the initialized grids_local,
        // to avoid memory leak.
        *grids_local = LibytProcessControl::Get().grids_local;
    }

    // Above all works like charm
    LibytProcessControl::Get().get_gridsPtr = true;
    log_info("Getting pointer to local grids information  ... done.\n");

    return YT_SUCCESS;
}