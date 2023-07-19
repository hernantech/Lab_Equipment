
class config:
    ##Initializer function
    #
    # Initializes paramters as variables under
    def __init__(self):
        self.sync = False #synchonization parameter, default value is false for asyncronous
        self.rate = 100 #integer value of frequency, default is  100 Hz


    def tweak(self, value):
        self.