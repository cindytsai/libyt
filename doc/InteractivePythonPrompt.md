# Interactive Python Prompt
> :information_source: To make interactive prompt more smoothly, set lower [YT_VERBOSE](./Initialize.md#yt_param_libyt).

## Status Board
Interactive python prompt will list all the inline functions call by [`yt_inline`](./PerformInlineAnalysis.md#yt_inline) or [`yt_inline_argument`](./PerformInlineAnalysis.md#yt_inline_argument).

#### Inline Function
Whenever you load inline script at [initialization stage](./Initialize.md#yt_param_libyt), use [`%libyt load`](#load), or directly type in [Python prompt](#python-statements), `libyt` will detect callables and list them here.

#### Status
List functions' status:
- `success`: run successfully.
- `failed`: failed.
- `idle`: this function was set to idle in current step.
- `not run yet`: not execute by `libyt` yet.

#### Run
Whether these functions will execute in next in situ analysis or not:
- `V`: this function will run automatically in following in situ analysis.
- `X`: this function will idle in next in situ analysis.

> :information_source: When calling [`yt_interactive_mode`](./ActivateInteractiveMode.md#yt_interactive_mode), `libyt` will run all the functions that was set to run using [`%libyt run`](#run), but had not been run by [`yt_inline`](./PerformInlineAnalysis.md#yt_inline) or [`yt_inline_argument`](./PerformInlineAnalysis.md#yt_inline_argument) yet.

```
=====================================================================
  Inline Function                              Status         Run
---------------------------------------------------------------------
  * yt_inline_ProjectionPlot                   success         V
  * yt_inline_ProfilePlot                      success         V
  * yt_inline_ParticlePlot                     success         V
  * yt_derived_field_demo                      success         V
  * test_function                              failed          V
=====================================================================
>>> [Type in python statements or libyt defined commands]
```

## Python Statements
You can run any python statements here, including importing modules. These statements run in [already loaded inline script](./Initialize.md#yt_param_libyt)'s namespace, and will make changes to it. This is like using IPython, except that there are functions and objects already defined in it. Changes will maintain througout in situ analysis.


## libyt Defined Commands
```
>>> %libyt [Command]
```

> :information_source: No spaces between `%`and `libyt`.

### Commands
#### help
```
>>> %libyt help
```
Print help messages.

#### exit
```
>>> %libyt exit
```
Exit interactive mode, and continue iteration process in simulation.

#### load
```
>>> %libyt load <file name>
```
Load and run file in [already loaded inline script](./Initialize.md#yt_param_libyt)'s namespace. This is like running every single line in these files line by line in IPython, so you can overwrite and update objects. Changes will maintain througout in situ analysis.

All function detected in this new loaded script will be set to idle, and will not run in the following in situ analysis, unless you switch it on using [`%libyt run`](#run).

#### export
```
>>> %libyt export <file name>
```
Export successfully run libyt defined commands and python statement into file in current in situ analysis. History of interactive prompt will be cleared when leaving prompt. 

> :information_source: Will overwrite file if `<file name>` already exist. 

#### status
```
>>> %libyt status
```
Print status board.

```
>>> %libyt status <function name>
```
Print function's definition and error messages if has.

#### idle
```
>>> %libyt idle <function name>
```
Idle `<function name>` in next in situ analysis. You will see `X` at run column in status board. It will clear all the input arguments set through [`%libyt run`](#run).


#### run
```
>>> %libyt run <function name> [args ...]
```
Run `<function name>` in the following in situ analysis using `[args ...]` if given. Set input arguments every time you switch this function on, because [`%libyt idle`](#idle) clears them.

###### Example
This is equivalent of `func(a, 2, "3")` in Python.
```
>>> a = 1
>>> def func(*args):
...     print(args)
>>> %libyt run func a 2 "3"
```
> :information_source: When calling [`yt_interactive_mode`](./ActivateInteractiveMode.md#yt_interactive_mode), `libyt` will run all the functions that was set to run using [`%libyt run`](#run), but had not been run by [`yt_inline`](./PerformInlineAnalysis.md#yt_inline) or [`yt_inline_argument`](./PerformInlineAnalysis.md#yt_inline_argument) yet.