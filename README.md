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