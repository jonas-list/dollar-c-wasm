#!/bin/bash

emcc dollar.c -o index.html --shell-file shell.html --preload-file data.txt -sNO_EXIT_RUNTIME=1 -s"EXPORTED_RUNTIME_METHODS=['ccall']" -s"EXPORTED_FUNCTIONS=['_free','_malloc','_load_templates','_construct_stroke','_recognize']" -O3
