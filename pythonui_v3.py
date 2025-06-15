"""
Python interface for the cable suspended wall robot.
Used for sending repeated commands by reading csv files.
Includes the option to write csv files containing 2 alternating commands.
"""

import serial
import time
import os

#config
PORT = 'COM3'
BAUD = 9600

FILE_COUNT = 0 # number of csv files in the directory

# Absolute path of this python file
script_dir = os.path.dirname(os.path.abspath(__file__))

# initialize the serial connection
ser = serial.Serial(PORT, BAUD, timeout=2)
time.sleep(2) # Wait for arduino to reset

def create_commands(x0, y0, x1, y1, reps):
    """
    Create a list of commands. Currently supports only two different MOVE commands which alternate.
    The user gives starting commands and destination commands. The probe alternates between those
    as quickly as possible for the amount of reps the user gives.

    Args:
        - x0, y0: starting coordinates
        - x1, y1: destination coordinates
        - reps: amount of repetition
    
    Returns: 
        - commands[] list

    Errors:
        - Added later
    """
    commands = []
    cmd1 = ('MOVE', x1, y1)
    cmd2 = ('MOVE', x0, y0)
    for i in range(reps):
        commands.append(cmd1)
        commands.append(cmd2)
    write_to_file(commands)

def write_to_file(commands):
    """
    Function for writing the commands in a list to a csv file in the following format: <MOVE, X, Y>
    Arduino expects to receive data in said format.
    """
   
    filename = f"commands{FILE_COUNT+1}.csv"
    with open(filename, 'w') as file:
        for cmd, x, y in commands:
            line = f"<{cmd}, {x}, {y}>\n"
            file.write(line)
    print(f"Saved file {filename} to {script_dir}")

def list_files(file_extension):
    """
    Helper function to list all the csv files in the directory of pythonui.
    Also keeps count of the amount of files in the directory
    """
    global FILE_COUNT
    files = [f for f in os.listdir(script_dir) if f.endswith(file_extension)]
    for file in files:
        FILE_COUNT += 1
    return files

def wait_for_confirmation():
    """
    Function that waits for confirmation from arduino,
    that a command was succesfully executed
    """
    while True:
        if ser.in_waiting:
            line = ser.readline().decode('ascii').strip()
            if line == "DONE":
                print(f"[Arduino] {line}")
                return
            print(f"[Arduino] {line}")


def send_command(cmd):
    """
    Function for sending commands to the Arduino via serial connection
    """
    print(f"--> Sending: {cmd}")
    ser.write((cmd.encode('ascii') + b'\0'))
    wait_for_confirmation()

def run_file(filename):
    """
    Read file and check that commands are of the correct format for sending
    """
    start_time = time.time()
    with open(filename, 'r') as file:
        for cmd in file:
            cmd = cmd.strip()
            if cmd.startswith('<') and cmd.endswith('>'):  # read all lines in the file and split them into parts
                print(cmd)
                send_command(cmd)
        print("DONE")
        print("Time elapsed for move in seconds: %s" % (time.time() - start_time))

def get_valid_input(prompt, min_value, max_value):
    """
    Checks that the values the user chose are integers
    and in the selected boundaries
    """
    while True:
        try:
            value = int(input(prompt))
        except ValueError:
            print("Integers please!")
            continue
        if value > max_value:
            print(f"Value has to be between {min_value} and {max_value}")
            continue
        if value < min_value:
            print(f"Value has to be between {min_value} and {max_value}")
            continue
        return value

def main():
    """
    Main user interface
    """
    files = list_files(".csv")

    print("\n---Welcome!---")
    print("This program allows the sending of commands in quick succession by reading commands from a CSV file\n")
    print("It is possible to write CSV files where 2 different commands are alternating")
    print("As of now the program can only be quit by ctrl+c :)")
    print("(S)end commands")
    print("(W)rite File")

    while True:
        selection = input("Select action: (s, w)").lower()
        if selection == "s":
            print("Available csv files:")
            files = list_files(".csv")
            for file in files:
                print(file)
            try:
                filename = input("Select the desired file by typing its name: ")
                run_file(filename)
            except FileNotFoundError: #raised if file not found
                print("Incorrect file name! Try again!")
            except ZeroDivisionError: #raised if file is empty
                print("File to be sent must not be empty!")
            continue 

        if selection == "w":
            motor_distance = int(input("Give distance between motors in mm: "))
            #print(FILE_COUNT)
            list_files(".csv")
            print("Choose commands to be written")
            x0 = get_valid_input(f"Give start coordinate x0 between {-motor_distance/2} and {motor_distance/2}\n", -motor_distance/2, motor_distance/2)
            y0 = get_valid_input(f"Give start coordinate y0 between {-motor_distance/2} and {motor_distance/2}\n", -motor_distance/2, motor_distance/2)
            x1 = get_valid_input(f"Give target coordinate x1 between {-motor_distance/2} and {motor_distance/2}\n", -motor_distance/2, motor_distance/2)
            y1 = get_valid_input(f"Give target coordinate y1 between {-motor_distance/2} and {motor_distance/2}\n", -motor_distance/2, motor_distance/2)
            reps = get_valid_input("Give amount of repetitions between 1 and 100\n", 1, 100)
            create_commands(x0, y0, x1, y1, reps)
            continue
        print("Invalid input!")

# Wait for Arduino's setup to finish
print("Waiting for Arduino")
while True:
    if ser.in_waiting:
        line = ser.readline().decode().strip()
        print(f"[ARDUINO] {line}")
        if "setup done" in line:
            break
main()
    
