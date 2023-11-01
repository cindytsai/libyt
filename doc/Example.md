---
layout: default
title: Example
nav_order: 4
---
# Example
{: .no_toc }
<details open markdown="block">
  <summary>
    Table of contents
  </summary>
  {: .text-delta }
- TOC
{:toc}
</details>
---

The [`example`](https://github.com/yt-project/libyt/blob/main/example) demonstrates how to implement `libyt` in adaptive mesh refinement simulation.
The steps related to implementation of `libyt` is commented like this:
```c++
// ==========================================
// libyt: 0. include libyt header
// ==========================================

// ==========================================
// libyt: [Optional] ...
// ==========================================
```

The example has a set of pre-calculated data.
It assigns the data to MPI processes randomly to simulate the actual code of having data distributed on different processes. (Though in real world, data won't distribute randomly.) 

The example initializes `libyt`, loads data to `libyt` in simulation's iterative process inside for loop, and finalizes it before terminating the simulation. To know the step by step details, you can read [`example/example.cpp`](https://github.com/yt-project/libyt/blob/main/example/example.cpp), it is well-commented.


## How to Compile and Run

1. Follow [**How to Install**]({% link HowToInstall.md %}#how-to-install) to install `libyt` and `yt_libyt`.
2. Install [`yt`](https://yt-project.org/). This example uses `yt` as the core analytic method.
  ```bash
  pip install yt
  ```
3. Update **MPI_PATH** in `example/Makefile`, which is MPI installation prefix, under this folder, there should be folders like `include`, `lib` etc.
  ```makefile
  MPI_PATH := $(YOUR_MPI_PATH)
  ```
  > :warning: Make sure you are using the same MPI to compile `libyt` and the example.
4. Go to `example` folder and compile.
  ```bash
  make clean
  make
  ```
5. Run `example`.
  ```bash
  mpirun -np 4 ./example
  ```

## Playground

### Activate Interactive Mode
The example assumes that `libyt` is in [**normal mode**]({% link HowToInstall.md %}#options).

To try out [**interactive mode**]({% link HowToInstall.md %}#options), we need to compile `libyt` in [**interactive mode**]({% link HowToInstall.md %}#options).
Then un-comment this block in `example.cpp` and create a file `LIBYT_STOP`. 
`LIBYT_STOP` and the executable should be in the same folder.   
`libyt` will enter [**interactive Python prompt**]({% link InSituPythonAnalysis/InteractivePythonPrompt.md %}#interactive-python-prompt) only if it detects `LIBYT_STOP` file, or an inline function failed.


```c++
// file: example/example.cpp
// =======================================================================================================
// libyt: 9. activate python prompt in interactive mode, should call it in situ function call using API
// =======================================================================================================
// Only supports when compile libyt in interactive mode (-DINTERACTIVE_MODE)
// Interactive prompt will start only if it detects "LIBYT_STOP" file, or an inline function failed.
if (yt_run_InteractiveMode("LIBYT_STOP") != YT_SUCCESS) {
    fprintf(stderr, "ERROR: yt_run_InteractiveMode failed!\n");
    exit(EXIT_FAILURE);
}
```

### Update Python Script
The example uses `inline_script.py` for in situ Python analysis. 
To change the Python script name, simpy change `param_libyt.script` and do not include file extension `.py` in `example.cpp`. 

```c++
// file: example/example.cpp
yt_param_libyt param_libyt;
param_libyt.verbose = YT_VERBOSE_INFO;   // libyt log level
param_libyt.script = "inline_script";    // inline python script, excluding ".py"
param_libyt.check_data = false;          // check passed in data or not
```

To know more about writing inline Python script, you can refer to [**In Situ Python Analysis**]({% link InSituPythonAnalysis/index.md %}#in-situ-python-analysis).
