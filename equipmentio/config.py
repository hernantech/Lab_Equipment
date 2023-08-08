
class config:
    ##Initializer function
    #
    # Initializes parameters as variables under
    def __init__(self):
        self.sync = None #synchonization parameter, default value is false for asyncronous
        self.rate = None #integer value of frequency, default is  100 Hz
        self.LockInAmplifier = None #IP addresses of the lock-in amplifiers (SR865)
        self.LockInAmplifier2 = None #IP address for SR860

    ## Default mode for no spectrum capture, sets parameters as such
    #
    #
    def default(self):
        self.sync = False
        self.rate = 100 #default sample rate, 100Hz
        self.LockInAmplifier = '192.168.1.17'  # default addresses of the lock-in amplifiers (SR865)
        self.LockInAmplifier2 = '192.168.1.18'  # default address for SR860

    ## Sweep mode for spectrum capture
    #
    #tweaks parameters to be optimized for spectrum capture
    def sweep(self):

