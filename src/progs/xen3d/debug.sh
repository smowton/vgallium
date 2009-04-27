#!/bin/bash

. run.config

LD_LIBRARY_PATH=../../$mesadir:../../$galdir/lib EGL_DRIVER=librawgal gdb --directory=/root/sources/libx/libx11-1.1.3/src --args xen3d $@
