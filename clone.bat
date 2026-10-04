@echo off

cd /d "thirdparty"

git clone https://github.com/wxWidgets/wxWidgets wxwidgets

cd /d "wxwidgets"

git fetch --all --tags

git checkout 670835aec7e54b9e9d5a50d6a1aea1e8ba0bcdb2

git submodule update --init --recursive
