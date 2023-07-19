import config
from equipmentio.Devices import cap860 as cap


class eqIO(config):
    def __init__(self, config):
        cap.dut_config()
        config.__init__() #initializing config's initializer
        print("Devices IO initialized")
        self.parameter = None

    def hardware_checks(self):
        print("checking hardware IO")
        try:


        except:
            print("An error has occurred with hardware IO. Please check connections, ports, and IP addresses")
