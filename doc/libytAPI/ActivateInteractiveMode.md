---
layout: default
title: yt_run_InteractiveMode -- Activate Python prompt
parent: libyt API
nav_order: 10
---
# Activate Interactive Mode
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

## yt\_run\_InteractiveMode
```cpp
int yt_run_InteractiveMode(const char* flag_file_name);
```
- Usage: Activate [interactive Python prompt]({% link InSituPythonAnalysis/InteractivePythonPrompt.md %}#interactive-python-prompt) when there are errors occurred in Python runtime during [calling Python functions]({% link libytAPI/PerformInlineAnalysis.md %}#calling-python-functions) or file `flag_file_name` is detected in the same directory where simulation executable is.
- Return: 
  - `YT_SUCCESS`
  - `YT_FAIL`: When `libyt` is not compiled with `-DINTERACTIVE_MODE`, it returns `YT_FAIL`.

> :information_source: Must compile `libyt` with `-DINTERACTIVE_MODE`. See [How to Install]({% link HowToInstall.md %}#options).

## Example
```cpp
#include "libyt.h"
...
if (yt_run_InteractiveMode("LIBYT_STOP") != YT_SUCCESS) {
    fprintf(stderr, "ERROR: yt_run_InteractiveMode failed!\n");
    exit(EXIT_FAILURE);
}
```