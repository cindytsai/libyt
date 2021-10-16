#include "yt_rma_particle.h"
#include <string.h>

//-------------------------------------------------------------------------------------------------------
// Class       :  yt_rma_particle
// Method      :  Constructor
//
// Notes       :  1. Initialize m_Window, which used inside OpenMPI RMA operation. And set m_Window info
//                   to "no_locks".
//                2. Copy the input ptype to m_ParticleType and attribute to m_AttributeName, in case
//                   it is freed.
//                3. Find particle and its attribute index inside particle_list and assign to
//                   m_ParticleIndex and m_AttributeIndex. We assume that we can always find them.
//                4. Grab the data type of the attribute, and store in m_AttributeDataType.
//                5. Set the std::vector capacity.
//
// Arguments   :  char*       ptype: Particle type.
//                char*   attribute: Attribute name.
//                int   len_prepare: Number of grid to prepare.
//                long      len_get: Number of grid to get.
//-------------------------------------------------------------------------------------------------------
yt_rma_particle::yt_rma_particle(char *ptype, char *attribute, int len_prepare, long len_get)
 : m_LenAllPrepare(0)
{
    // Initialize m_Window and set info to "no_locks".
    MPI_Info windowInfo;
    MPI_Info_create( &windowInfo );
    MPI_Info_set( windowInfo, "no_locks", "true" );
    MPI_Win_create_dynamic( windowInfo, MPI_COMM_WORLD, &m_Window );
    MPI_Info_free( &windowInfo );

    // Copy input ptype and attribute.
    int len = strlen(ptype);
    m_ParticleType = new char [len+1];
    strcpy(m_ParticleType, ptype);

    len = strlen(attribute);
    m_AttributeName = new char [len+1];
    strcpy(m_AttributeName, attribute);

    for(int v = 0; v < g_param_yt.num_species; v++){
        if( strcmp(m_ParticleType, g_param_yt.particle_list[v].species_name) == 0 ){
            m_ParticleIndex = v;
            for(int a = 0; a < g_param_yt.particle_list[v].num_attr; a++) {
                if( strcmp(m_AttributeName, g_param_yt.particle_list[v].attr_list[a].attr_name) == 0 ){
                    m_AttributeIndex = a;
                    m_AttributeDataType = g_param_yt.particle_list[v].attr_list[a].attr_dtype;
                    break;
                }
            }
            break;
        }
    }

    // Set std::vector capacity
    m_Prepare.reserve(len_prepare);
    m_PrepareData.reserve(len_prepare);
    m_Fetched.reserve(len_get);
    m_FetchedData.reserve(len_get);

    printf("yt_rma_particle: Particle Type  = %s\n", m_ParticleType);
    printf("yt_rma_particle: Attribute Name = %s\n", m_AttributeName);
}

//-------------------------------------------------------------------------------------------------------
// Class       :  yt_rma_particle
// Method      :  Destructor
//
// Notes       :  1. Free m_ParticleType, m_AttributeName, m_Window.
//                2. Clear m_Fetched and m_FetchedData, even though it is already empty.
//
// Arguments   :  None
//-------------------------------------------------------------------------------------------------------
yt_rma_particle::~yt_rma_particle()
{
    MPI_Win_free(&m_Window);
    delete [] m_ParticleType;
    delete [] m_AttributeName;
    m_Fetched.clear();
    m_FetchedData.clear();
    printf("yt_rma_particle: Destructor called.\n");
}

//-------------------------------------------------------------------------------------------------------
// Class       :  yt_rma_particle
// Method      :  prepare_data
// Description :  Prepare particle data in grid = gid and species_name = m_ParticleType, then attach
//                particle data to m_Window and get the address.
//
// Notes       :  1. Prepare the particle data in grid = gid, and attach particle data to m_Window.
//                2. Insert data pointer into m_PrepareData.
//                3. Insert data information into m_Prepare.
//                4. data_len is the length of the particle data array.
//                5. We assume that all gid can be found on this rank.
//
// Arguments   :  long gid : Particle data to prepare in grid id = gid.
//
// Return      :  YT_SUCCESS or YT_FAIL
//-------------------------------------------------------------------------------------------------------
int yt_rma_particle::prepare_data(long& gid)
{
    printf("yt_rma_particle: Prepare particle [%s] in grid [%ld].\n", m_ParticleType, gid);

    // Get particle info
    yt_rma_particle_info par_info;
    par_info.id = gid;

    for(int lid = 0; lid < g_param_yt.num_grids_local; lid++){
        if( g_param_yt.grids_local[lid].id == gid ){
            par_info.rank = g_param_yt.grids_local[lid].proc_num;
            par_info.data_len = g_param_yt.grids_local[lid].particle_count_list[m_ParticleIndex];
        }
    }

    // Generate particle data
    void (*get_attr) (long, char*, void*);
    get_attr = g_param_yt.particle_list[m_ParticleIndex].get_attr;

    int dtype_size;
    get_dtype_size(m_AttributeDataType, &dtype_size);
    void *data_ptr = malloc( par_info.data_len * dtype_size );
    (*get_attr) (gid, m_AttributeName, data_ptr);

    // Attach buffer to window.
    if( MPI_Win_attach(m_Window, data_ptr, par_info.data_len * dtype_size ) != MPI_SUCCESS ){
        YT_ABORT("yt_rma_particle: Attach particle [%s] attribute [%s] to window failed!\n",
                 m_ParticleType, m_AttributeName);
    }

    // Get the address of the attached buffer.
    if( MPI_Get_address(data_ptr, &(par_info.address)) != MPI_SUCCESS ){
        YT_ABORT("yt_rma_particle: Get attached particle [%s] attribute [%s] buffer address failed!\n",
                 m_ParticleType, m_AttributeName);
    }

    // Push back to m_Prepare, m_PrepareData.
    m_PrepareData.push_back( data_ptr );
    m_Prepare.push_back( par_info );

    return YT_SUCCESS;
}

//-------------------------------------------------------------------------------------------------------
// Class       :  yt_rma_particle
// Method      :  gather_all_prepare_data
// Description :  Gather all prepared data in each rank.
//
// Notes       :  1. This should be called after preparing all the needed particle data.
//                2. Perform big_MPI_Gatherv and big_MPI_Bcast at root rank.
//                3. Open the window epoch.
//
// Parameter   :  int root : root rank.
//
// Return      :  YT_SUCCESS or YT_FAIL
//-------------------------------------------------------------------------------------------------------
int yt_rma_particle::gather_all_prepare_data(int root)
{
    int NRank;
    MPI_Comm_size(MPI_COMM_WORLD, &NRank);

    // Get m_Prepare data and its length
    int PreparedInfoListLength = m_Prepare.size();
    yt_rma_particle_info *PreparedInfoList = m_Prepare.data();

    // MPI_Gather send count in each rank, then MPI_Bcast.
    int *SendCount = new int [NRank];
    MPI_Gather(&PreparedInfoListLength, 1, MPI_INT, SendCount, 1, MPI_INT, root, MPI_COMM_WORLD);
    MPI_Bcast(SendCount, NRank, MPI_INT, root, MPI_COMM_WORLD);

    // Calculate m_LenAllPrepare.
    for(int rank = 0; rank < NRank; rank++){
        m_LenAllPrepare += SendCount[rank];
    }

    // Gather PreparedInfoList, which is m_Prepare in each rank
    // (1) Create MPI_Datatype for yt_rma_particle_info
    MPI_Datatype yt_rma_particle_info_mpi_type;
    int lengths[4] = {1, 1, 1, 1};
    const MPI_Aint displacements[4] = {0,
                                       1 * sizeof(long),
                                       1 * sizeof(long) + 1 * sizeof(MPI_Aint),
                                       2 * sizeof(long) + 1 * sizeof(MPI_Aint)};
    MPI_Datatype types[4] = {MPI_LONG, MPI_AINT, MPI_LONG, MPI_INT};
    MPI_Type_create_struct(4, lengths, displacements, types, &yt_rma_particle_info_mpi_type);
    MPI_Type_commit(&yt_rma_particle_info_mpi_type);

    // (2) Perform big_MPI_Gatherv and big_MPI_Bcast
    m_AllPrepare = new yt_rma_particle_info [m_LenAllPrepare];
    big_MPI_Gatherv(root, SendCount, (void*)PreparedInfoList, &yt_rma_particle_info_mpi_type, (void*)m_AllPrepare, 2);
    big_MPI_Bcast(root, m_LenAllPrepare, (void*)m_AllPrepare, &yt_rma_particle_info_mpi_type, 2);

    // Open window epoch.
    MPI_Win_fence(MPI_MODE_NOSTORE | MPI_MODE_NOPUT | MPI_MODE_NOPRECEDE, m_Window);

    // Free unused resource
    delete [] SendCount;

    return YT_SUCCESS;
}

//-------------------------------------------------------------------------------------------------------
// Class       :  yt_rma_particle
// Method      :  fetch_remote_data
// Description :  Allocate spaces and fetch remote data, and store in std::vector m_Fetched, m_FetchData.
//
// Notes       :  1. Look for particle info in m_AllPrepare, and allocate memory.
//                2. We allocate buffer to store fetched data, but we do not free them. It is Python's
//                   responsibility.
//
// Parameters  : long gid  : From which grid id to fetch the particle data.
//               int  rank : Fetch data from rank.
//
// Return      :  YT_SUCCESS or YT_FAIL
//-------------------------------------------------------------------------------------------------------
int yt_rma_particle::fetch_remote_data(long& gid, int& rank)
{
    // Look for gid in m_AllPrepare, and allocate memory.
    int  dtype_size;
    MPI_Datatype mpi_dtype;
    yt_rma_particle_info fetched;
    for(long aid = 0; aid < m_LenAllPrepare; aid++){
        if( m_AllPrepare[aid].id == gid ){
            fetched = m_AllPrepare[aid];
            break;
        }
    }
    get_dtype_size( m_AttributeDataType, &dtype_size );
    get_mpi_dtype( m_AttributeDataType, &mpi_dtype );
    void *fetchedData = malloc( fetched.data_len * dtype_size );

    // TODO: MPI_Get with big number.
    if( MPI_Get(fetchedData, (int)fetched.data_len, mpi_dtype, rank, fetched.address, (int)fetched.data_len, mpi_dtype, m_Window) != MPI_SUCCESS ){
        YT_ABORT("yt_rma_particle: MPI_Get fetch particle [%s] attribute [%s] in grid [%ld] failed!\n",
                 m_ParticleType, m_AttributeName, gid);
    }

    // Push back to m_Fetched, m_FetchedData.
    m_Fetched.push_back(fetched);
    m_FetchedData.push_back(fetchedData);

    return YT_SUCCESS;
}

//-------------------------------------------------------------------------------------------------------
// Class       :  yt_rma_particle
// Method      :  clean_up
// Description :  Clean up prepared data.
//
// Notes       :  1. Close the window epoch, and detach the prepared data buffer.
//                2. Free m_AllPrepare.
//                3. Free local prepared data, drop m_Prepare and m_PrepareData vector.
//
// Return      :  YT_SUCCESS or YT_FAIL
//-------------------------------------------------------------------------------------------------------
int yt_rma_particle::clean_up()
{
    // Close the window epoch
    MPI_Win_fence(MPI_MODE_NOSTORE | MPI_MODE_NOPUT | MPI_MODE_NOSUCCEED, m_Window);

    // Detach m_PrepareData from m_Window, and then free it.
    for(int i = 0; i < m_PrepareData.size(); i++){
        MPI_Win_detach(m_Window, m_PrepareData[i]);
        delete [] m_PrepareData[i];
    }

    // Free m_AllPrepare, m_PrepareData, m_Prepare
    delete [] m_AllPrepare;
    m_PrepareData.clear();
    m_Prepare.clear();

    return YT_SUCCESS;
}

//-------------------------------------------------------------------------------------------------------
// Class       :  yt_rma_particle
// Method      :  get_fetched_data
// Description :  Get fetched data.
//
// Notes       :  1. Get fetched data one by one. If vector is empty, then it will return YT_FAIL.
//                2. Whenever one gets the data from m_Fetched and m_FetchedData, it will also remove it
//                   from std::vector.
//                3. Write fetched data and info to passed in parameters.
//
// Parameters  :  long     *gid        : Grid id fetched.
//                char    **ptype      : Particle type fetched.
//                char    **attribute  : Attribute fetched.
//                yt_dtype *data_dtype : Fetched data type.
//                long     *data_len   : Fetched data length.
//                void    **data_ptr   : Fetched data pointer.
//
// Return      :  YT_SUCCESS or YT_FAIL
//-------------------------------------------------------------------------------------------------------
int yt_rma_particle::get_fetched_data(long *gid, char **ptype, char **attribute, yt_dtype *data_dtype, long *data_len, void **data_ptr)
{
    // Check if there are left fetched data to get.
    if( m_Fetched.size() == 0 ){
        return YT_FAIL;
    }

    // Access m_Fetched and m_FetchedData from the back.
    yt_rma_particle_info fetched = m_Fetched.back();
    *gid = fetched.id;
    *ptype = m_ParticleType;
    *attribute = m_AttributeName;
    *data_dtype = m_AttributeDataType;
    *data_len = fetched.data_len;
    *data_ptr = m_FetchedData.back();

    // Pop back fetched data.
    m_Fetched.pop_back();
    m_FetchedData.pop_back();

    return YT_SUCCESS;
}
