# Magnonics Lab equipment library

## Requirements 
 - Virtual environment manager (Conda/Miniconda/venv/etc)
 - Python 2.7, 3.1+
 - Docopt, vxi-11 (python)
 - UNIX-based system, i.e. Linux/MacOS (no guarantees for windows, has not been tested)

## Structure
 - To be added

## Installation Guide
 - Also TBA

## Notes
### What still needs to be done:
 - Hardcoding device options, i.e. SR865 needs default values (no command line values) so that the test(options) function may be called from the equipmentioClass class
 - Setting up fault-catching control flow
   - This includes verifying hardware connections and capturing those errors
 - Finalizing config class (to be inherited by equipmentIO class)
   - Deciding whether to inherit the class as an object within equipmentIO (accessing via self.config.method_or_parameter)or to inherit all methods and objects (self.inherited_method_or_parameter), which overwrites and requires namechecking
 - Finalizing main class calls
 - deciding to keep/structure with a start.py wrapper or just main
 - ### develop test suite for vxi-11 to command a stream, send a request, and still collect the data through the entire time

## Priority (1)
 - Try to get a true data livestream (no time/byte limit), as fast/reliable as possible
 - Send comments to equipment in parallel (if supported, the communication protocol may not allow this due to locks)
   - If not possible, try to establish a continuous livestream via TCP/IP
   - In parallel, send commands via GPIB (to tweak parameters)
     - If not possible (priority 3)
       - Create stream composed of chunks and send comments between packages (30ms delay, 10% loss max at 3Hz)