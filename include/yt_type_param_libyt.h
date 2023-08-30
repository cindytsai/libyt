#ifndef __YT_TYPE_PARAM_LIBYT_H__
#define __YT_TYPE_PARAM_LIBYT_H__



/*******************************************************************************
/
/  yt_param_libyt structure
/
/  ==> included by yt_type.h
/
********************************************************************************/


//-------------------------------------------------------------------------------------------------------
// Structure   :  yt_param_libyt
// Description :  Data structure of libyt runtime parameters
//
// Notes       :  1. The lifetime of script should cover the whole in situ process in libyt.
//
// Data Member :  verbose : Verbose level
//                script  : Name of the YT inline analysis script (without the .py extension)
//                counter : Number of rounds doing inline-analysis
//                check_data: Check the input data (ex: hierarchy, grid information...), if it is true.
//-------------------------------------------------------------------------------------------------------
struct yt_param_libyt
{
   yt_verbose verbose;
   const char *script;
   long       counter;   
   bool       check_data;

#ifdef __cplusplus
   //===================================================================================
   // Method      :  yt_param_libyt
   // Description :  Constructor of the structure "yt_param_libyt"
   //
   // Note        :  Initialize all data members
   //
   // Parameter   :  None
   //===================================================================================
   yt_param_libyt()
   {
      verbose    = YT_VERBOSE_WARNING;
      script     = "yt_inline_script";
      counter    = 0;
      check_data = true;
   }
#endif // #ifdef __cplusplus

};

#endif // #ifndef __YT_TYPE_PARAM_LIBYT_H__
