#!/usr/bin/env bash
set -e
make && ./bin/sysmon --api
