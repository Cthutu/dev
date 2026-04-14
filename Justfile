default:
    just --list

python-env:
    @uv -q --no-progress --directory build sync

build *args: python-env
    uv --directory build run python ../build/build.py {{args}}

build-release *args: python-env
    uv --directory build run python ../build/build.py -r {{args}}

test *args: python-env
    uv --directory build run python ../build/test.py {{args}}

test-release *args: python-env
    uv --directory build run python ../build/test.py -r {{args}}

format: python-env
    uv --directory build run python ../build/format.py

run *args: python-env
    uv --directory build run python ../build/run.py {{args}}

run-release *args: python-env
    uv --directory build run python ../build/run.py -r {{args}}

clean:
    rm -rf _bin _obj
    rm -rf build/.venv
    rm -rf build/__pycache__

alias b := build
alias br := build-release
alias t := test
alias tr := test-release
alias f := format
alias r := run
alias rr := run-release
alias c := clean

test-network:
    just build demo-client demo-server
    just run demo-client &
    just run demo-server
    wait
