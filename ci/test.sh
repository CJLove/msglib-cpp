#!/bin/bash

function ShowUsage()
{
    cat <<EOT
$(basename $0) options
    [--builddir=<builddir>]  - name of build directory

EOT
    return 0    
}

BUILDDIR=build

while test $# -gt 0; do
    param="$1"
    if test "${1::1}" = "-"; then
        if test ${#1} -gt 2 -a "${1::2}" = "--" ; then
            param="${1:2}"
        else
            param="${1:1}"
        fi
    else
        break
    fi

    shift

    case $param in
    builddir=*)
        BUILDDIR=$(echo $param|cut -f2 -d'=')
        ;;
    help|h|?|-?)
        ShowUsage
        exit 0
        ;;
    *)
        echo "Error: Unknown parameter: $param"
        ShowUsage
        exit 2
        ;;    
    esac
done

echo "BUILDDIR=$BUILDDIR"

[ ! -d ./msglib-cpp-git ] && { echo "ERROR: repo not cloned!"; exit 1; }
[ ! -d ./msglib-cpp-git/$BUILDDIR ] && { echo "ERROR: repo not built!"; exit 1; }

# Change to the base directory of the repo
cd msglib-cpp-git/$BUILDDIR

# Run tests
./tests/msglibTests
ret=$?
[ $ret -ne 0 ] && exit $ret

exit $ret
