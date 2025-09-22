#!/bin/bash
# Script to check sequencer state using pypylon

echo "============================================"
echo "Checking Sequencer State and Configuration"
echo "============================================"
echo ""

# Check if venv exists and use it, otherwise use system python
if [ -f "venv/bin/python" ]; then
    echo "Using virtual environment..."
    venv/bin/python check_sequencer_state.py
else
    echo "Using system Python..."
    python3 check_sequencer_state.py
fi

echo ""
echo "============================================"
echo "Done checking sequencer state"
echo "============================================"