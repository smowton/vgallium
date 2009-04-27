#!/bin/bash

. run.config

LD_LIBRARY_PATH=/usr/local/lib:../../$mesadir:../../$galdir/lib EGL_DRIVER=librawgal ./xen3d $@
