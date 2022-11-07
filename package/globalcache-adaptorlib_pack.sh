#!/bin/bash
set -e

curd=$(pwd)
globalcache_adapter_dir=$curd/../

tar_mode=$1

cpu_type=$(uname -m)

function initialize()
{
        if [[ -d "$curd/globalcache-adaptorlib" ]]; then
                rm -rf $curd/globalcache-adaptorlib-${cpu_type}
        fi

        mkdir -p $curd/globalcache-adaptorlib-${cpu_type}

        # lianjieku
        cp -rf $globalcache_adapter_dir/build/lib/* $curd/globalcache-adaptorlib-${cpu_type}
}

function pack_lib()
{
        if [[ $tar_mode == "DEBUG" ]]
        then
                tar -zcvf globalcache-adaptorlib-debug-oe1.${cpu_type}.tar.gz globalcache-adaptorlib-${cpu_type}
        else
                tar -zcvf globalcache-adaptorlib-release-oe1.${cpu_type}.tar.gz globalcache-adaptorlib-${cpu_type}
        fi
}

function main()
{
        initialize
        pack_lib
}

main $@
