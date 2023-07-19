
import config

class eqIO(config):
    def __init__(self, config):
        config.__init__() #initializing config's initializer
        print("equipment IO initialized")
        self.parameter = None

    def checks(self):
