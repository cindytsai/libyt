#include "yt_combo.h"
#include "libyt.h"


//-------------------------------------------------------------------------------------------------------
// Function    :  yt_check_grid.cpp
// Description :  Check that all the grids are loaded.
//
// Note        :  1. Check that all the grids (the hierarchy) are set, every MPI rank need to do this.
// 				  2. Check that the hierarchy is correct.
// 				  3. Use inside yt_inline(), before perform yt operation "def yt_inline():"
// 				  
//
// Parameter   :
//
// Return      :  YT_SUCCESS or YT_FAIL
//-------------------------------------------------------------------------------------------------------

int yt_check_grid() {
	
	// Check that all the grids hierarchy are set.
	for (int g=0; g<g_param_yt.num_grids; g++)
   	{
      	if ( g_param_libyt.grid_hierarchy_set[g] == false )
         	YT_ABORT( "In hierarchy, grid [%ld] has not been set!\n", g );
   	}
	
	// Check that data set are well collected.
	// 		Notes: Collect and check at RootRank
	int     MyRank, NRank;
	int     RootRank = 0;

	MPI_Comm_rank( MPI_COMM_WORLD, &MyRank );
	MPI_Comm_size( MPI_COMM_WORLD, &NRank  );

	bool    *collected_grid_data_set = new bool [NRank * g_param_yt.num_grids * g_param_yt.num_fields];
	for (int i=0; i<NRank*g_param_yt.num_grids*g_param_yt.num_fields; i++) collected_grid_data_set[i] = false;

	MPI_Gather( g_param_libyt.grid_data_set, g_param_yt.num_grids, MPI_C_BOOL,
				 collected_grid_data_set, g_param_yt.num_grids, MPI_C_BOOL, RootRank, MPI_COMM_WORLD);

	if ( MyRank == RootRank ) {
		int field_id, grid_id;
		int stride = g_param_yt.num_fields * g_param_yt.num_grids;
		
		for ( int id=0; id<stride; id++ ) {
			for ( int rid=0; rid<NRank; rid++ ) {
				// TODO: We should also check that it matches the proc_num
				// TODO: Should proc_num be input as an array?
				if ( collected_grid_data_set[id+rid*stride] == true ) {
					break;
				}
				else {
					field_id = id / g_param_yt.num_grids;
					grid_id  = id % g_param_yt.num_grids;
					// TODO: Should be output as field name, not field_id
					// 		 Should pass field_label array in param_yt as well
					YT_ABORT( "Grid [%ld] data %d has not been set!\n", grid_id, field_id );
				}
			}
		}
	}

	delete [] collected_grid_data_set;
	
	// TODO: Not yet done
	// Check that the hierarchy relationship are correct.
	// 		notes: there are already some check depends on YT parameter in yt_add_grid() . 

	return YT_SUCCESS;
}