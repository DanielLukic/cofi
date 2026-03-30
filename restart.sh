#!/bin/bash
make clean && make && systemctl --user restart cofi && journalctl --user -u cofi --no-pager -n 5
