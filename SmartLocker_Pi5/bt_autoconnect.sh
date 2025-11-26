#!/bin/bash

MAC="14:2B:2F:A7:94:22"   # TTGO MAC
CHAN=1                    # SPP usually channel 1

while true; do
  # In case something was left bound, free it
  sudo rfcomm release 0 >/dev/null 2>&1

  echo "$(date) -> Trying to connect to $MAC on channel $CHAN..."

  # This call BLOCKS while connected.
  # When the link drops or fails, it returns and the loop repeats.
  sudo rfcomm connect 0 "$MAC" "$CHAN"

  echo "$(date) -> rfcomm exited with code $?; retrying in 3 seconds..."
  sleep 3
done
