import sys
from datetime import datetime
from raspipins import Raspipin

MAPPING = {1:7, 2:11, 3:13, 4:15, 5:16, 6:18}

def current_time():
	return str(datetime.now())

def get_zone():
        try:
                if len(sys.argv) > 2:
                        raise ValueError
	
                zone = sys.argv[1]

                if not zone.isdigit():
                	raise TypeError
                
        except IndexError as e:
                exc = str(type(e))
                print( exc + " @ " + current_time() + ">> - missing arg in gpio.py call")
                sys.exit(1)

        except TypeError as e:
                exc = str(type(e))
                print(exc + " @ " + current_time() + ">> - arg in gpio.py call must by of type int")
                sys.exit(1)

        except ValueError as e:
                exc = str(type(e))
                print(exc + " @ " + current_time() + ">> - allowed only one arg in gpio.py call")
                sys.exit(1)
                
        return int(zone)
        



rp = Raspipin(MAPPING)
print("Zone" + str(get_zone()) + " activated " + current_time())
rp.button_press(get_zone())
rp.cleanup()




