#include <stdio.h>
#include "yt.h"
#include "yt_macro.h"
#include "yt_type.h"
#include "yt_prototype.h"




//-------------------------------------------------------------------------------------------------------
// Function    :  yt_init
// Description :  Initialize libyt
//
// Note        :  None
//
// Parameter   :  argc  : Argument count
//                argv  : Argument vector
//                param : libyt runtime parameters
//
// Return      :  YT_SUCCESS or YT_FAIL
//-------------------------------------------------------------------------------------------------------
int yt_init( int argc, char *argv[], const yt_param *param )
{

   static bool initialized = false;

// nothing to do if libyt has been initialized
   if ( initialized )
   {
      if ( param->verbose >= YT_VERBOSE_WARNING )  log_warning( "libyt has already been initialized\n" );

      return YT_SUCCESS;
   }

   if ( param->verbose )   log_info( "Initializing libyt ...\n" );


// print out runtime parameters
   if ( param->verbose == YT_VERBOSE_DEBUG )
   {
      log_info( "   verbose = %d\n", param->verbose );
      log_info( "   script  = %s\n", param->script );
   }


// initialize Python interpreter
   if ( init_python(argc,argv,param) == YT_FAIL )  return YT_FAIL;


   initialized = true;
   return YT_SUCCESS;

} // FUNCTION : yt_init
