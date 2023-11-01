---
layout: default
title: Using yt
parent: In Situ Python Analysis
nav_order: 4
---
# Using yt for In Situ Python Analysis
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

## Use yt for In Situ Python Analysis under Parallel Computation
`libyt` directly borrows parallel computation feature in `yt` using `mpi4py`. You can also refer to [**Parallel Computation With yt**](https://yt-project.org/doc/analyzing/parallel_computation.html#parallel-computation-with-yt).

We should always include the first three lines, then wrap the other statements inside Python functions, 
so that we can call these functions to conduct in situ analysis. (See [**Calling Python Functions**]({% link libytAPI/PerformInlineAnalysis.md %}#calling-python-functions).)

Because we now load data directly from `libyt`, we need to replace `yt.load()` to `yt_libyt.libytDataset()`.
Everything else is the same.

For example, the function `yt_inline` plots a density projection plot.
```python
# inline script
import yt_libyt                   # import libyt's yt frontend
import yt                         # import yt
yt.enable_parallelism()           # make yt work in parallelism feature

def yt_inline():
    ds = yt_libyt.libytDataset()  # <--> yt.load("Data")
    proj = yt.ProjectionPlot(ds, "density")
    if yt.is_root():
        proj.save()
```

## Supported yt Functionalities
These are the functions we have tested.
Basically, everything will work in Python under parallel computation using `mpi4py`. 

|       `yt` Function      | Supported | Notes                                                               |
|:------------------------:|:---------:|---------------------------------------------------------------------|
| `find_max`               |     V     |                                                                     |
| `ProjectionPlot`         |     V     |                                                                     |
| `OffAxisProjectionPlot`  |     V     |                                                                     |
| `SlicePlot`              |     V     |                                                                     |
| `OffAxisSlicePlot`       |     V     |                                                                     |
| `covering_grid`          |     V     |                                                                     |
| 1D `create_profile`      |     V     |                                                                     |
| 2D `create_profile`      |     V     |                                                                     |
| `ProfilePlot`            |     V     |                                                                     |
| `PhasePlot`              |     V     |                                                                     |
| `LinePlot`               |     V     |                                                                     |
| Halo Analysis            |           | Not test yet.                                                       |
| Isocontours              |     V     |                                                                     |
| `volume_render`          |     V     | :warning: Need even MPI processes.                                  |
| `ParticlePlot`           |     V     |                                                                     |
| `ParticleProjectionPlot` |     V     |                                                                     |
| Annotations              |     V     | :warning: Some[^1] require `save()` be outside of `if yt.is_root()` |

Reading and accessing data is a collective operation, and it requires every MPI process to join.
If only some of the processes participate in reading data during a yt function, then the program will hang, 
because some processes are blocked at data reading stage and waiting for other processes to join.

For example, `volume_render`, which has a restriction of working under even MPI processes only.
And plots with annotations `annotate_quiver`, `annotate_cquiver`, `annotate_velocity`, `annotate_line_integral_convolution`, 
`annotate_magnetic_field`, and `annotate_particles`, need to access data when saving figure. 
Which means every MPI process should run `save()`, and we have to move `save()` outside of `if yt.is_root()`.

> :lizard: Since there is no way we can know what data to access and build up a communication graph for data exchange beforehand,
> when accessing simulation data, `libyt` requires every process to participate in this.
> We are working on this in both `yt` and `libyt`.

## Distinguish libyt Fields and yt Fields

### libyt Fields and yt Fields
- **libyt fields** are fields loaded by `libyt`. They are fields defined inside [`yt_get_FieldsPtr`]({% link libytAPI/FieldInfo/SetFieldsInformation.md %}#yt_get_fieldsptr). It could be actual simulation field data, or data generated through [derived field function]({% link libytAPI/FieldInfo/DerivedField.md %}#derived-field-function).
    
    ```python
    ds = yt_libyt.libytDataset()
    ds.field_list  # prints a list of libyt fields
    ```
  
- **yt fields** are fields defined in field information class (class `XXXFieldInfo`) in yt frontend, or yt built-in derived fields. `XXX` is frontend name defined in `frontend` in [`yt_param_yt`]({% link libytAPI/SetYTParameter.md %}#yt_param_yt).

    ```python
    ds = yt_libyt.libytDataset()
    ds.derived_field_list  # prints a list of yt derived field
    ```

> :information_source: We can use both **libyt fields** and **yt fields** in in situ analysis Python script. 

### Naming and Field Information
libyt inherits field information (ex: units, name aliases) defined in yt frontend, and it can access yt built-in derived fields.

yt frontend (`frontend` set through [`yt_param_yt`]({% link libytAPI/SetYTParameter.md %}#yt_param_yt)
using [`yt_set_Parameters`]({% link libytAPI/SetYTParameter.md %}#yt_set_parameters)) has the highest priority, the next is fields/particles defined through [`yt_get_FieldsPtr`]({% link libytAPI/FieldInfo/SetFieldsInformation.md %}#yt_get_fieldsptr)/[`yt_get_ParticlesPtr`]({% link libytAPI/SetParticlesInformation.md %}#yt_get_particlesptr), 
and finally yt built-in derived fields.

Which is:
1. If field name `"A"` is both defined in [`yt_get_FieldsPtr`]({% link libytAPI/FieldInfo/SetFieldsInformation.md %}#yt_get_fieldsptr)/[`yt_get_ParticlesPtr`]({% link libytAPI/SetParticlesInformation.md %}#yt_get_particlesptr) and yt [`frontend`]({% link libytAPI/SetYTParameter.md %}#yt_param_yt), then `yt` uses the field information (ex: units, name alias) defined in yt [`frontend`]({% link libytAPI/SetYTParameter.md %}#yt_param_yt). (It also adds name alias defined through libyt API to this field information if there is.)
2. If field name `"B"` is only defined in [`yt_get_FieldsPtr`]({% link libytAPI/FieldInfo/SetFieldsInformation.md %}#yt_get_fieldsptr)/[`yt_get_ParticlesPtr`]({% link libytAPI/SetParticlesInformation.md %}#yt_get_particlesptr), then `yt` uses the information defined through libyt API.
3. If field name `"C"` defined in [`yt_get_FieldsPtr`]({% link libytAPI/FieldInfo/SetFieldsInformation.md %}#yt_get_fieldsptr)/[`yt_get_ParticlesPtr`]({% link libytAPI/SetParticlesInformation.md %}#yt_get_particlesptr) overlapped with yt built-in derived field (`"C"` and yt derived field have the same name), then `yt` uses `"C"` defined through libyt API. Namely, it overwrites yt derived field.

## FAQs
### Why Does my Program Hang and How Do I Solve It?
Though `libyt` can execute any Python module, when it comes to reading simulation data, it requires every MPI process to participate
The program hanging problem is due to only some MPI processes are accessing the data, but not all of them.

Please do:
1. Check if there is an if statements that makes MPI processes non-symmetric. For example, only root process runs the statement:
    ```python
    def func():
        if yt.is_root():
            ...  # <-- This statement only executes in MPI root rank
    ```
2. Move the statement out of `if yt.is_root()` (for the case here).

> :lizard: When accessing simulation data, `libyt` requires every process to participate in this.
> We are working on this in both `yt` and `libyt`.

----
[^1]: `annotate_quiver`, `annotate_cquiver`, `annotate_velocity`, `annotate_line_integral_convolution`, `annotate_magnetic_field`, and `annotate_particles`