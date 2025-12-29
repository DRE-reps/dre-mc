#!/usr/bin/python3
import serial
import time
import sys

DEBUG = 0

def parse_hex_input(uinput):
    """Parse hex bytes like '0x00 0x20 0x03' into bytes"""
    try:
        hex_parts = uinput.strip().split()
        byte_array = []
        for part in hex_parts:
            # Remove 0x prefix if present
            hex_val = part.replace('0x', '').replace('0X', '')
            byte_array.append(int(hex_val, 16))
        return bytes(byte_array)
    except ValueError:
        print("Error: Invalid hex format. Use: 0x00 0x20 0x03 ...")
        return None

def send_message_and_read_response_from_serial(data):
    global DEBUG
    ser.write(data)
    time.sleep(0.1)
    out = b''
    while ser.inWaiting() > 0:
        out += ser.read(1)
    if DEBUG:
        print(f"Sent: {data.hex(' ')}")
        print(f"Recv: {out.hex(' ')} ({out})")
    return out

def handle_response_and_print(ser_output, request_type):
    if len(ser_output) == 0:
        print("Empty response, system broken")
        return
    
    # Try to decode as UTF-8 first
    try:
        decoded = ser_output.decode('utf-8').strip()
    except UnicodeDecodeError:
        decoded = ser_output.hex(' ')
    
    if decoded.startswith("O"):
        print("RESP: OK")
    elif decoded.startswith("E"):
        print("RESP: ERROR")
    elif request_type == "FC__MS":
        print("FC33 speed: " + decoded + " m/s")
    elif request_type == "FC_RPM":
        print("FC33 rpm  : " + decoded)
    elif request_type == "MPU_AX":
        print("MPU6050 Ax: " + decoded + " m/ss")
    elif request_type == "KSPEED":
        print("Kalm speed: " + decoded + " m/s")
    elif request_type == "SEELOG":
        print(decoded)
    elif request_type == "SEERAN":
        print("vl53l0x ran: " + decoded + " sm")
    else:
        print(f"Raw response: {decoded}")

def user_input_listener():
    while True:
        try:
            uinput = input(">> ")
            if uinput == "exit":
                ser.close()
                sys.exit()
            elif uinput.startswith("poll "):
                uinput = uinput[5:]  # remove "poll " prefix
                if uinput == "all":
                    while True:
                        try:
                            handle_response_and_print(send_message_and_read_response_from_serial("FC__MS".encode()), "FC__MS")
                            handle_response_and_print(send_message_and_read_response_from_serial("FC_RPM".encode()), "FC_RPM")
                            handle_response_and_print(send_message_and_read_response_from_serial("MPU_AX".encode()), "MPU_AX")
                            handle_response_and_print(send_message_and_read_response_from_serial("KSPEED".encode()), "KSPEED")
                            handle_response_and_print(send_message_and_read_response_from_serial("SEERAN".encode()), "SEERAN")
                        except KeyboardInterrupt:
                            break
                else:
                    while True:
                        try:
                            data = uinput.encode()
                            handle_response_and_print(send_message_and_read_response_from_serial(data), uinput)
                        except KeyboardInterrupt:
                            break
            elif uinput.startswith("0x"):  # Hex bytes input
                hex_data = parse_hex_input(uinput)
                if hex_data:
                    response = send_message_and_read_response_from_serial(hex_data)
                    print(f"Sent hex: {uinput}")
                    handle_response_and_print(response, "HEX")
            else:  # Text command
                data = uinput.encode()
                handle_response_and_print(send_message_and_read_response_from_serial(data), uinput)
        except KeyboardInterrupt:
            ser.close()
            sys.exit()
        except Exception as e:
            print(f"Error: {e}")

try:
    ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=5)
    print(ser.name)
except:
    print("Check uart connection")
    sys.exit(1)

ser.isOpen()
print('Enter your commands below.')
print('Text: FC__MS, poll all, etc.')
print('Hex bytes: 0x00 0x20 0x03 0x40')
print('Use "exit" to leave or Ctrl+C to stop polling.\r\n')

user_input_listener()
