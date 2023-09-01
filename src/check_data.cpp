#include "yt_combo.h"
#include "LibytProcessControl.h"


//-------------------------------------------------------------------------------------------------------
// Function    :  check_sum_num_grids_local_MPI
// Description :  Check sum of number of local grids in each MPI rank is equal to num_grids input by user.
//
// Note        :  1. Use inside yt_set_Parameters()
//                2. Check sum of number of local grids in each MPI rank is equal to num_grids input by 
//                   user, which is equal to the number of total grids.
//                
// Parameter   :  int * &num_grids_local_MPI : Address to the int*, each element stores number of local 
//                                             grids in each MPI rank.
//
// Return      :  YT_SUCCESS or YT_FAIL
//-------------------------------------------------------------------------------------------------------
int check_sum_num_grids_local_MPI( int NRank, int * &num_grids_local_MPI ) {
    long num_grids = 0;
    for (int rid = 0; rid < NRank; rid = rid+1){
        num_grids = num_grids + (long)num_grids_local_MPI[rid];
    }
    if (num_grids != g_param_yt.num_grids){
        for(int rid = 0; rid < NRank; rid++){
            log_error("MPI rank [ %d ], num_grids_local = %d.\n", rid, num_grids_local_MPI[rid]);
        }
        YT_ABORT("Sum of local grids in each MPI rank [%ld] are not equal to input num_grids [%ld]!\n",
                 num_grids, g_param_yt.num_grids );
    }

    return YT_SUCCESS;
}


//-------------------------------------------------------------------------------------------------------
// Function    :  check_field_list
// Description :  Check g_param_yt.field_list.
//
// Note        :  1. Use inside yt_commit().
//                2. Check field_list
//                  (1) Validate each yt_field element in field_list.
//                  (2) Name of each field are unique.
//                
// Parameter   :  None
//
// Return      :  YT_SUCCESS or YT_FAIL
//-------------------------------------------------------------------------------------------------------
int check_field_list(){
    yt_field *field_list = LibytProcessControl::Get().field_list;

    // (1) Validate each yt_field element in field_list.
    for ( int v = 0; v < g_param_yt.num_fields; v++ ){
        yt_field& field = field_list[v];
        if ( !(field.validate()) ){
            YT_ABORT("Validating input field list element [%d] ... failed\n", v);
        }
    }

    // (2) Name of each field are unique.
    for ( int v1 = 0; v1 < g_param_yt.num_fields; v1++ ){
        for ( int v2 = v1+1; v2 < g_param_yt.num_fields; v2++ ){
            if ( strcmp(field_list[v1].field_name, field_list[v2].field_name) == 0 ){
                YT_ABORT("field_name in field_list[%d] and field_list[%d] are not unique!\n", v1, v2);
            }
        }
    }

    return YT_SUCCESS;
}


//-------------------------------------------------------------------------------------------------------
// Function    :  check_particle_list
// Description :  Check g_param_yt.particle_list.
//
// Note        :  1. Use inside yt_commit().
//                2. Check particle_list
//                  (1) Validate each yt_particle element in particle_list.
//                  (2) Species name (or ptype in YT-term) cannot be the same as g_param_yt.frontend.
//                  (3) Species names (or ptype in YT-term) are all unique.
//                
// Parameter   :  None
//
// Return      :  YT_SUCCESS or YT_FAIL
//-------------------------------------------------------------------------------------------------------
int check_particle_list(){
    yt_particle *particle_list = LibytProcessControl::Get().particle_list;

    // (1) Validate each yt_particle element in particle_list.
    // (2) Check particle type name (or ptype in YT-term) cannot be the same as g_param_yt.frontend.
    for ( int p = 0; p < g_param_yt.num_par_types; p++ ){
        yt_particle& particle = particle_list[p];
        if ( !(particle.validate()) ){
            YT_ABORT("Validating input particle list element [%d] ... failed\n", p);
        }
        if ( strcmp(particle.par_type, g_param_yt.frontend) == 0 ){
            YT_ABORT("particle_list[%d], par_type == %s, frontend == %s, expect particle type name different from the frontend!\n",
                     p, particle.par_type, g_param_yt.frontend);
        }
    }

    // (3) Particle type name (or ptype in YT-term) are all unique.
    for ( int p1 = 0; p1 < g_param_yt.num_par_types; p1++ ){
        for ( int p2 = p1+1; p2 < g_param_yt.num_par_types; p2++ ){
            if ( strcmp(particle_list[p1].par_type, particle_list[p2].par_type) == 0 ){
                YT_ABORT("par_type in particle_list[%d] and particle_list[%d] are the same, par_type should be unique!\n", p1, p2);
            }
        }
    }

    return YT_SUCCESS;
}


//-------------------------------------------------------------------------------------------------------
// Function    :  check_grid
// Description :  Check g_param_yt.grids_local.
//
// Note        :  1. Use inside yt_commit().
//                2. Check grids_local
//                  (1) Validate each yt_grid element in grids_local.
//                  (2) parent ID is not bigger or equal to num_grids.
//                  (3) Root level starts at 0. So if level > 0, then parent ID >= 0.
//                  (4) domain left edge <= grid left edge.
//                  (5) grid right edge <= domain right edge.
//                  (6) grid left edge <= grid right edge.
//                      (Not sure if this still holds for periodic condition.)
//                  (7) Abort if field_type = "cell-centered", and data_ptr == NULL.
//                  (8) Abort if field_type = "face-centered", and data_ptr == NULL.
//                  (9) If data_ptr != NULL, then data_dimensions > 0
//
// Parameter   :  None
//
// Return      :  YT_SUCCESS or YT_FAIL
//-------------------------------------------------------------------------------------------------------
int check_grid(){
    yt_grid *grids_local = LibytProcessControl::Get().grids_local;
    yt_field *field_list = LibytProcessControl::Get().field_list;

    // Checking grids
    // check each grids individually
    for (int i = 0; i < g_param_yt.num_grids_local; i = i+1) {

        yt_grid& grid = grids_local[i];

        // (1) Validate each yt_grid element in grids_local.
        if (check_yt_grid(grid) != YT_SUCCESS)
            YT_ABORT(  "Validating input grid ID [%ld] ... failed\n", grid.id );

        // (2) parent ID is not bigger or equal to num_grids.
        if (grid.parent_id >= g_param_yt.num_grids)
            YT_ABORT(  "Grid [%ld] parent ID [%ld] >= total number of grids [%ld]!\n",
                       grid.id, grid.parent_id, g_param_yt.num_grids );

        // (3) Root level starts at 0. So if level > 0, then parent ID >= 0.
        if (grid.level > 0 && grid.parent_id < 0)
            YT_ABORT(  "Grid [%ld] parent ID [%ld] < 0 at level [%d]!\n",
                       grid.id, grid.parent_id, grid.level );

        // edge
        for (int d = 0; d < 3; d = d+1) {

            // (4) Domain left edge <= grid left edge.
            if (grid.left_edge[d] < g_param_yt.domain_left_edge[d])
                YT_ABORT( "Grid [%ld] left edge [%13.7e] < domain left edge [%13.7e] along the dimension [%d]!\n",
                          grid.id, grid.left_edge[d], g_param_yt.domain_left_edge[d], d );

            // (5) grid right edge <= domain right edge.
            if (grid.right_edge[d] > g_param_yt.domain_right_edge[d])
                YT_ABORT( "Grid [%ld] right edge [%13.7e] > domain right edge [%13.7e] along the dimension [%d]!\n",
                          grid.id, grid.right_edge[d], g_param_yt.domain_right_edge[d], d );

            // (6) grid left edge <= grid right edge.
            if (grid.right_edge[d] < grid.left_edge[d])
                YT_ABORT( "Grid [%ld], right edge [%13.7e] < left edge [%13.7e]!\n",
                          grid.id, grid.right_edge[d], grid.left_edge[d]);
        }

        // check field_data in each individual grid
        for (int v = 0; v < g_param_yt.num_fields; v = v+1){

            // If field_type == "cell-centered"
            if ( strcmp(field_list[v].field_type, "cell-centered") == 0 ) {

                // (7) Raise warning if field_type = "cell-centered", and data_ptr is not set == NULL.
                if ( grid.field_data[v].data_ptr == NULL ){
                    YT_ABORT( "Grid [%ld], field_data [%s], field_type [%s], data_ptr is NULL, not set yet!",
                                 grid.id, field_list[v].field_name, field_list[v].field_type);
                }
            }

            // If field_type == "face-centered"
            if ( strcmp(field_list[v].field_type, "face-centered") == 0 ) {

                // (8) Raise warning if field_type = "face-centered", and data_ptr is not set == NULL.
                if ( grid.field_data[v].data_ptr == NULL ){
                    YT_ABORT( "Grid [%ld], field_data [%s], field_type [%s], data_ptr is NULL, not set yet!",
                                 grid.id, field_list[v].field_name, field_list[v].field_type);
                }
                else{
                    // (9) If data_ptr != NULL, then data_dimensions > 0
                    for ( int d = 0; d < 3; d++ ){
                        if ( grid.field_data[v].data_dimensions[d] <= 0 ){
                            YT_ABORT("Grid [%ld], field_data [%s], field_type [%s], data_dimensions[%d] == %d <= 0, should be > 0!\n",
                                      grid.id, field_list[v].field_name, field_list[v].field_type, d, grid.field_data[v].data_dimensions[d]);
                        }
                    }
                }
            }

            // If field_type == "derived_func"
            if ( strcmp(field_list[v].field_type, "derived_func") == 0 ) {
                // (10) If data_ptr != NULL, then data_dimensions > 0
                if ( grid.field_data[v].data_ptr != NULL ){
                    for ( int d = 0; d < 3; d++ ){
                        if ( grid.field_data[v].data_dimensions[d] <= 0 ){
                            YT_ABORT("Grid [%ld], field_data [%s], field_type [%s], data_dimensions[%d] == %d <= 0, should be > 0!\n",
                                      grid.id, field_list[v].field_name, field_list[v].field_type, d, grid.field_data[v].data_dimensions[d]);
                        }
                    }
                }
            }
        }
    }

    return YT_SUCCESS;
}


//-------------------------------------------------------------------------------------------------------
// Function    :  check_hierarchy
// Description :  Check that the hierarchy, parent-children relationships are correct
//
// Note        :  1. Use inside yt_commit()
// 			      2. Check that the hierarchy is correct, even though we didn't build a parent-children 
//                   map.
//                  (1) Check every grid id are unique. (Be careful that it can be non-0-indexing.)
//                  (2) Check if all grids with level > 0, have a good parent id.
//                  (3) Check if children grids' edge fall between parent's.
//                  (4) Check parent's level = children level - 1.
// 				  
// Parameter   :  yt_hierarchy * &hierarchy : Contain full hierarchy
//
// Return      :  YT_SUCCESS or YT_FAIL
//-------------------------------------------------------------------------------------------------------
int check_hierarchy(yt_hierarchy * &hierarchy) {

    // Create a search table for matching gid to hierarchy array index
    long *order = new long [g_param_yt.num_grids];
    for (long i = 0; i < g_param_yt.num_grids; i = i+1) {
        order[i] = -1;
    }

    // Check every grid id are unique, and also filled in the search table
    int index_offset = g_param_yt.index_offset;
    for (long i = 0; i < g_param_yt.num_grids; i = i+1) {
        if (order[ hierarchy[i].id - index_offset ] == -1) {
            order[ hierarchy[i].id - index_offset ] = i;
        }
        else {
            YT_ABORT("Grid ID [ %ld ] are not unique, both MPI rank %d and %d are using this grid id!\n",
                     hierarchy[i].id, hierarchy[i].proc_num, hierarchy[ order[ hierarchy[i].id - index_offset ] ].proc_num);
        }
    }

    // Check if all level > 0 have good parent id, and that children's edges don't exceed parent's
    for (long i = 0; i < g_param_yt.num_grids; i = i+1) {

        if ( hierarchy[i].level > 0 ) {

            // Check parent id
            if ( (hierarchy[i].parent_id - index_offset < 0) || hierarchy[i].parent_id - index_offset >= g_param_yt.num_grids ){
                YT_ABORT("Grid ID [%ld], Level %d, Parent ID [%ld], ID is out of range, expect to be between %d ~ %ld.\n",
                         hierarchy[i].id, hierarchy[i].level, hierarchy[i].parent_id, index_offset, g_param_yt.num_grids + index_offset - 1);
            }
            else {
                // Check children's edges fall between parent's
                double *parent_left_edge = hierarchy[order[hierarchy[i].parent_id - index_offset]].left_edge;
                double *parent_right_edge = hierarchy[order[hierarchy[i].parent_id - index_offset]].right_edge;
                for (int d = 0; d < 3; d = d+1){
                    if ( !(parent_left_edge[d] <= hierarchy[i].left_edge[d]) ) {
                        YT_ABORT("Grid ID [%ld], Parent ID [%ld], grid_left_edge[%d] < parent_left_edge[%d].\n",
                                 hierarchy[i].id, hierarchy[i].parent_id, d, d);
                    }
                    if ( !(hierarchy[i].right_edge[d] <= parent_right_edge[d]) ) {
                        YT_ABORT("Grid ID [%ld], Parent ID [%ld], parent_right_edge[%d] < grid_right_edge[%d].\n",
                                 hierarchy[i].id, hierarchy[i].parent_id, d, d);
                    }
                }

                // Check parent's level = children level - 1
                int parent_level = hierarchy[order[hierarchy[i].parent_id - index_offset]].level;
                if ( !(parent_level == hierarchy[i].level - 1) ){
                    YT_ABORT("Grid ID [%ld], Parent ID [%ld], parent level %d != children level %d - 1.\n",
                             hierarchy[i].id, hierarchy[i].parent_id, parent_level, hierarchy[i].level);
                }
            }
        }
    }

    // Free resource
    delete [] order;

    return YT_SUCCESS;
}


//-------------------------------------------------------------------------------------------------------
// Function    :  check_yt_param_yt
// Description :  Check yt_param_yt struct
//
// Note        :  1. Cosmological parameters are checked only if cosmological_simulation == 1
//                2. Only check if data are set properly, does not alter them.
//
// Parameter   :  const yt_param_yt &param_yt
//
// Return      :  YT_SUCCESS or YT_FAIL
//-------------------------------------------------------------------------------------------------------
int check_yt_param_yt(const yt_param_yt &param_yt) {
    if ( param_yt.frontend                 == NULL ) YT_ABORT( "\"%s\" has not been set!\n", "frontend" );
    for (int d=0; d<3; d++) {
        if ( param_yt.domain_left_edge [d] == DBL_UNDEFINED )   YT_ABORT( "\"%s[%d]\" has not been set!\n", "domain_left_edge",  d );
        if ( param_yt.domain_right_edge[d] == DBL_UNDEFINED )   YT_ABORT( "\"%s[%d]\" has not been set!\n", "domain_right_edge", d );
    }
    if ( param_yt.current_time             == DBL_UNDEFINED )   YT_ABORT( "\"%s\" has not been set!\n",     "current_time" );
    if ( param_yt.cosmological_simulation  == INT_UNDEFINED )   YT_ABORT( "\"%s\" has not been set!\n",     "cosmological_simulation" );
    if ( param_yt.cosmological_simulation == 1 ) {
        if ( param_yt.current_redshift     == DBL_UNDEFINED )   YT_ABORT( "\"%s\" has not been set!\n",     "current_redshift" );
        if ( param_yt.omega_lambda         == DBL_UNDEFINED )   YT_ABORT( "\"%s\" has not been set!\n",     "omega_lambda" );
        if ( param_yt.omega_matter         == DBL_UNDEFINED )   YT_ABORT( "\"%s\" has not been set!\n",     "omega_matter" );
        if ( param_yt.hubble_constant      == DBL_UNDEFINED )   YT_ABORT( "\"%s\" has not been set!\n",     "hubble_constant" );
    }
    if ( param_yt.length_unit              == DBL_UNDEFINED )   YT_ABORT( "\"%s\" has not been set!\n",     "length_unit" );
    if ( param_yt.mass_unit                == DBL_UNDEFINED )   YT_ABORT( "\"%s\" has not been set!\n",     "mass_unit" );
    if ( param_yt.time_unit                == DBL_UNDEFINED )   YT_ABORT( "\"%s\" has not been set!\n",     "time_unit" );
    if ( param_yt.magnetic_unit            == DBL_UNDEFINED )   log_warning( "\"%s\" has not been set!\n",  "magnetic_unit" );

    for (int d=0; d<3; d++) {
        if ( param_yt.periodicity      [d] == INT_UNDEFINED )   YT_ABORT( "\"%s[%d]\" has not been set!\n", "periodicity", d );
        if ( param_yt.domain_dimensions[d] == INT_UNDEFINED )   YT_ABORT( "\"%s[%d]\" has not been set!\n", "domain_dimensions", d );
    }
    if ( param_yt.dimensionality           == INT_UNDEFINED )   YT_ABORT( "\"%s\" has not been set!\n",     "dimensionality" );
    if ( param_yt.refine_by                == INT_UNDEFINED )   YT_ABORT( "\"%s\" has not been set!\n",     "refine_by" );
    if ( param_yt.num_grids                == LNG_UNDEFINED )   YT_ABORT( "\"%s\" has not been set!\n",     "num_grids" );
    if ( param_yt.num_par_types > 0 && param_yt.par_type_list == NULL  )   YT_ABORT( "Particle type info par_type_list has not been set!\n");
    if ( param_yt.num_par_types < 0 && param_yt.par_type_list != NULL  )   YT_ABORT( "Particle type info num_par_types has not been set!\n");
    for (int s=0; s<param_yt.num_par_types; s++) {
        if ( param_yt.par_type_list[s].par_type == NULL || param_yt.par_type_list[s].num_attr < 0 ) YT_ABORT( "par_type_list element [ %d ] is not set properly!\n", s);
    }

    return YT_SUCCESS;
}


//-------------------------------------------------------------------------------------------------------
// Function    :  check_yt_grid
// Description :  Check yt_grid struct
//
// Note        :  1. This function does not perform checks that depend on the input
//                   YT parameters (e.g., whether left_edge lies within the simulation domain)
//                   ==> These checks are performed in check_grid()
//                2. If check needs information other than this grid's info, it will be done in check_grid.
//
// Parameter   :  const yt_grid &grid
//
// Return      :  YT_SUCCESS or YT_FAIL
//-------------------------------------------------------------------------------------------------------
int check_yt_grid(const yt_grid &grid) {
    for (int d=0; d<3; d++) {
        if ( grid.left_edge [d]  == DBL_UNDEFINED    )   YT_ABORT( "\"%s[%d]\" has not been set for grid id [%ld]!\n", "left_edge",  d, grid.id );
        if ( grid.right_edge[d]  == DBL_UNDEFINED    )   YT_ABORT( "\"%s[%d]\" has not been set for grid id [%ld]!\n", "right_edge", d, grid.id );
    }
    for (int d=0; d<3; d++) {
        if ( grid.grid_dimensions[d]  == INT_UNDEFINED ) YT_ABORT( "\"%s[%d]\" has not been set for grid id [%ld]!\n", "grid_dimensions", d,  grid.id );
    }
    if ( grid.id             == LNG_UNDEFINED    )   YT_ABORT(     "\"%s\" has not been set for grid id [%ld]!\n", "id",             grid.id );
    if ( grid.parent_id      == LNG_UNDEFINED    )   YT_ABORT(     "\"%s\" has not been set for grid id [%ld]!\n", "parent_id",      grid.id );
    if ( grid.level          == INT_UNDEFINED    )   YT_ABORT(     "\"%s\" has not been set for grid id [%ld]!\n", "level",          grid.id );
    if ( grid.proc_num       == INT_UNDEFINED    )   YT_ABORT(     "\"%s\" has not been set for grid id [%ld]!\n", "proc_num",       grid.id );
    for (int d=0; d<3; d++) {
        if ( grid.grid_dimensions[d] <= 0 )   YT_ABORT( "\"%s[%d]\" == %d <= 0 for grid [%ld]!\n", "grid_dimensions", d, grid.grid_dimensions[d], grid.id );
    }
    if ( grid.level < 0 )                     YT_ABORT( "\"%s\" == %d < 0 for grid [%ld]!\n", "level", grid.level, grid.id );

    return YT_SUCCESS;
}