# NB-IoT Project with ESP32 and SIM7080

This project demonstrates how to use an ESP32 microcontroller with a SIM7080 module to perform NB-IoT connectivity and execute a GET request using AT commands.

## Project Overview

The project involves setting up an ESP32 to communicate with a SIM7080 NB-IoT module. The aim is to send an HTTP GET request to a specified server and handle the response.

## Components

- **ESP32**: The main microcontroller used for the project.
- **SIM7080**: The NB-IoT module responsible for network connectivity.
- **Wires and Connectors**: For connecting the ESP32 to the SIM7080 module.

## Connections

1. **ESP32 to SIM7080:**
   - TX of ESP32 to RX of SIM7080
   - RX of ESP32 to TX of SIM7080
   - GND of ESP32 to GND of SIM7080

## Software Overview

1. **AT Commands**: 
   - Used for configuring the SIM7080 and performing the GET request.
   - Examples include setting network credentials, initiating a connection, and sending an HTTP GET request.

2. **ESP-IDF *:
   - Use ESP-IDF to program the ESP32.
