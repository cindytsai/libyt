#ifdef INTERACTIVE_MODE

#include "yt_combo.h"
#include <string.h>
#include "func_status_list.h"

//-------------------------------------------------------------------------------------------------------
// Class       :  func_status_list
// Method      :  reset
//
// Notes       :  1. Reset every func_status data member m_Status in list to -1 (not run yet).
//
// Arguments   :  None
//
// Return      : YT_SUCCESS
//-------------------------------------------------------------------------------------------------------
int func_status_list::reset() {
    for (int i=0; i<size(); i++) {
        m_FuncStatusList[i].set_status(-1);
    }
    return YT_SUCCESS;
}


//-------------------------------------------------------------------------------------------------------
// Class       :  func_status_list
// Method      :  print_summary
//
// Notes       :  1. Print function status and error msg in func_status_list.
//                2. normal      -> bold white
//                   idle        -> bold blue
//                   not run yet -> bold yellow
//                   success     -> bold green
//                   failed      -> bold red
//                   MPI process -> bold cyan
//
// Arguments   :  None
//
// Return      : YT_SUCCESS or YT_FAIL
//-------------------------------------------------------------------------------------------------------
int func_status_list::print_summary() {
    // make sure every rank has reach here, so that printing in other ranks are done
    fflush(stdout);
    fflush(stderr);

    if (g_myrank == 0) {
        printf("\033[1;37m");
        printf("=====================================================================\n");
        printf("  %-40s     %-12s   %s\n", "Inline Function", "Status", "Run");
        printf("---------------------------------------------------------------------\n");
        for (int i=0; i<size(); i++) {
            printf("\033[1;37m"); // change to bold white
            printf("  * %-43s", m_FuncStatusList[i].get_func_name());
            int run = m_FuncStatusList[i].get_run();
            int status = m_FuncStatusList[i].get_status();

            if (status == 0) {
                printf("\033[1;31m"); // bold red: failed
                printf("%-12s", "failed");
            }
            else if (status == 1) {
                printf("\033[1;32m"); // bold green: success
                printf("%-12s", "success");
            }
            else if (status == -1 ) {
                printf("\033[1;34m"); // bold blue: idle
                printf("%-12s", "idle");
            }
            else {
                printf("\033[0;37m"); // change to white
                printf("%-12s (%d)", "unkown status", status);
            }

            printf("\033[1;33m"); // bold yellow
            if (run == 1) printf("%5s\n", "V");
            else          printf("%5s\n", "X");

            fflush(stdout);
        }
        printf("\033[1;37m");
        printf("=====================================================================\n");
        printf("\033[0;37m"); // change to white
    }

    return YT_SUCCESS;
}


//-------------------------------------------------------------------------------------------------------
// Class       :  func_status_list
// Method      :  get_func_index
//
// Notes       :  1. Look up index of func_name in m_FuncStatusList.
//
// Arguments   :  char   *func_name: inline function name
//
// Return      :  index : index of func_name in list, return -1 if doesn't exist.
//-------------------------------------------------------------------------------------------------------
int func_status_list::get_func_index(const char *func_name) {
    int index = -1;
    for (int i=0; i<size(); i++) {
        if (strcmp(m_FuncStatusList[i].get_func_name(), func_name) == 0) {
            index = i;
            break;
        }
    }
    return index;
}


//-------------------------------------------------------------------------------------------------------
// Class       :  func_status_list
// Method      :  add_new_func
//
// Notes       :  1. Check if func_name is defined inside the vector, if not create one.
//
// Arguments   :  char   *func_name: inline function name
//                int     run      : run in next inline analysis or not.
//
// Return      : YT_SUCCESS
//-------------------------------------------------------------------------------------------------------
int func_status_list::add_new_func(char *func_name, int run) {
    // Check if func_name exist, return YT_SUCCESS if exist
    if (get_func_index(func_name) >= 0) return YT_SUCCESS;

    // add func_name
    func_status new_func(func_name, run);
    m_FuncStatusList.push_back(new_func);

    return YT_SUCCESS;
}


//-------------------------------------------------------------------------------------------------------
// Class       :  func_status_list
// Method      :  run_func
//
// Notes       :  1. This is a collective call. It executes new added functions that haven't run by
//                   yt_inline/yt_inline_argument yet.
//                2. Only supports function that don't have input arguments. It runs function func() in
//                   Python.
//
// Arguments   :  None
//
// Return      :  YT_SUCCESS
//-------------------------------------------------------------------------------------------------------
int func_status_list::run_func() {
    for (int i=0; i<size(); i++) {
        int run = m_FuncStatusList[i].get_run();
        int status = m_FuncStatusList[i].get_status();
        if (run == 1 && status == -1) {
            // command
            char *funcname = m_FuncStatusList[i].get_func_name();
            int command_width = 150 + strlen(g_param_libyt.script) + strlen(funcname) * 2;
            char *command = (char*) malloc(command_width * sizeof(char));
            sprintf(command, "try:\n"
                             "    %s.%s(%s)\n"
                             "except Exception as e:\n"
                             "    libyt.interactive_mode[\"func_err_msg\"][\"%s\"] = traceback.format_exc()\n",
                             g_param_libyt.script, funcname, m_FuncStatusList[i].get_args().c_str(), funcname);

            // run and update status
            m_FuncStatusList[i].set_status(-2);
            PyRun_SimpleString(command);
            m_FuncStatusList[i].get_status();

            // clean up
            free(command);
        }
    }
    return YT_SUCCESS;
}


//-------------------------------------------------------------------------------------------------------
// Class         :  func_status_list
// Static Method :  load_file_func_body
//
// Notes         :  1. This is a static method.
//                  2. It updates functions' body defined in filename and put it under
//                     libyt.interactive_mode["func_body"].
//                  3. Get only keyword def defined functions. If the functors are defined using __call__
//                     this method cannot grab the corresponding definition.
//
// Arguments     :  const char *filename: update function body for function defined inside filename
//
// Return        : YT_SUCCESS or YT_FAIL
//-------------------------------------------------------------------------------------------------------
int func_status_list::load_file_func_body(const char *filename) {
    int command_len = 500 + strlen(filename);
    char *command = (char*) malloc(command_len * sizeof(char));
    sprintf(command, "for key in libyt.interactive_mode[\"script_globals\"].keys():\n"
                     "    if key.startswith(\"__\") and key.endswith(\"__\"):\n"
                     "        continue\n"
                     "    else:\n"
                     "        var = libyt.interactive_mode[\"script_globals\"][key]\n"
                     "        try:\n"
                     "            if callable(var) and inspect.getsourcefile(var).split(\"/\")[-1] == \"%s\":\n"
                     "                libyt.interactive_mode[\"func_body\"][key] = inspect.getsource(var)\n"
                     "        except:\n"
                     "            pass\n", filename);

    if (PyRun_SimpleString(command) == 0) {
        log_debug("Loading function body in script %s ... done\n", filename);
        free(command);
        return YT_SUCCESS;
    }
    else {
        log_debug("Loading function body in script %s ... failed\n", filename);
        free(command);
        return YT_FAIL;
    }
}


//-------------------------------------------------------------------------------------------------------
// Class         :  func_status_list
// Static Method :  load_input_func_body
//
// Notes         :  1. This is a static method.
//                  2. Detect if there are functors defined in code object py_src, if yes, put it under
//                     libyt.interactive_mode["func_body"].
//                  3. It's not this method's responsibility to dereference py_src.
//
// Arguments     :  PyObject **py_src : python code object to load and detect.
//
// Return        : YT_SUCCESS or YT_FAIL
//-------------------------------------------------------------------------------------------------------
int func_status_list::load_input_func_body(PyObject **py_src) {

}


//-------------------------------------------------------------------------------------------------------
// Class         :  func_status_list
// Static Method :  get_funcname_defined
//
// Notes         :  1. This is a static method.
//                  2. It grabs functions or any callable object's name defined in filename.
//
// Arguments     :  const char *filename: update function body for function defined inside filename
//
// Return        : std::vector<std::string> contains a list of function name defined in filename.
//-------------------------------------------------------------------------------------------------------
std::vector<std::string> func_status_list::get_funcname_defined(const char *filename) {
    int command_len = 400 + strlen(filename);
    char *command = (char*) malloc(command_len * sizeof(char));
    sprintf(command, "libyt.interactive_mode[\"temp\"] = []\n"
                     "for key in libyt.interactive_mode[\"script_globals\"].keys():\n"
                     "    if key.startswith(\"__\") and key.endswith(\"__\"):\n"
                     "        continue\n"
                     "    else:\n"
                     "        var = libyt.interactive_mode[\"script_globals\"][key]\n"
                     "        if callable(var) and inspect.getsourcefile(var).split(\"/\")[-1] == \"%s\":\n"
                     "            libyt.interactive_mode[\"temp\"].append(key)\n", filename);
    if (PyRun_SimpleString(command) != 0) log_error("Unable to grab functions in python script %s.\n", filename);

    PyObject *py_func_list = PyDict_GetItemString(g_py_interactive_mode, "temp");
    Py_ssize_t py_list_len = PyList_Size(py_func_list);
    std::vector<std::string> func_list;
    func_list.reserve((long)py_list_len);
    for (Py_ssize_t i=0; i<py_list_len; i++) {
        const char *func_name = PyUnicode_AsUTF8(PyList_GET_ITEM(py_func_list, i));
        func_list.emplace_back(std::string(func_name));
    }

    // clean up
    free(command);
    PyRun_SimpleString("del libyt.interactive_mode[\"temp\"]");

    return func_list;
}


//-------------------------------------------------------------------------------------------------------
// Class         :  func_status_list
// Static Method :  set_exception_hook()
//
// Notes         :  1. This is a static method.
//                  2. Makes MPI not invoke MPI_Abort when getting errors in Python in interactive mode.
//                  3. Can only be called after libyt is initialized.
//
// Arguments     :  None
//
// Return        :  YT_SUCCESS or YT_FAIL
//-------------------------------------------------------------------------------------------------------
int func_status_list::set_exception_hook() {
    const char *command = "import sys\n"
                          "def mpi_libyt_interactive_mode_excepthook(exception_type, exception_value, tb):\n"
                          "    from mpi4py import MPI\n"
                          "    myrank = MPI.COMM_WORLD.Get_rank()\n"
                          "    traceback.print_tb(tb)\n"
                          "    print(\"[YT_ERROR  ] {}: {}\".format(exception_type.__name__, exception_value))\n"
                          "    print(\"[YT_ERROR  ] Error occurred on rank {}.\".format(myrank))\n"
                          "sys.excepthook = mpi_libyt_interactive_mode_excepthook\n";
    if (PyRun_SimpleString(command) == 0) return YT_SUCCESS;
    else return YT_FAIL;
}

#endif // #ifdef INTERACTIVE_MODE
