import sys
import equipmentioClass as eq

def main():
    try:
        print("attempting to initialize Devices")
        equipment = eq(eq.config)

    except:
        print("an error occured. Please see the error message for debugging. ")

