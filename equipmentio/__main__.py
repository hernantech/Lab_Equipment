import sys
import equipmentioClass as eq

def main():
    try:
        print("attempting to initialize Devices")
        equipment = eq(eq.config)
        eq.test(default=off, thing = 2, otherthing=5)
    except:
        print("an error occured. Please see the error message for debugging. ")

