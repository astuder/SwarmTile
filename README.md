# Swarm Tile Library

This repository contains an Arduino library to integrate the Swarm Tile. Swarm is a satellite network designed for low-cost IoT communication. 

**Note: This project is not affiliated with Swarm Technologies Inc. This library is NOT developed, supported or endorsed by Swarm Technologies Inc.**

# Documentation

tbd

# Known Limitations

## Wakeup immediatly after Sleep

The Tile takes a few seconds to enter sleep mode. Calling `Wakeup` is to soon after `Sleep` may result in confusing error messages. 

To avoid this error, sleep for 20 seconds or longer.

## DBXTOHIVEFULL

With Tile FW 1.0.0, the example code sometimes puts the Tile into a state where `sendMessage` returns with a `DBXTOHIVEFULL` error.

To resolve this error send the following command: `$RS dbinit`
