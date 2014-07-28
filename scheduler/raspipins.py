import RPi.GPIO as GPIO
import time

class Raspipin(object):

	def __init__(self, mapping):
		GPIO.setmode(GPIO.BOARD)
		self._mapping = mapping

	def get_pin(self, zone):
		return self._mapping[zone]

	def button_press(self, zone, on_time=10):
		pin_num = self.get_pin(zone)
		GPIO.setup(pin_num, GPIO.OUT, initial=0)
		GPIO.output(pin_num,True)
		time.sleep(on_time)
		GPIO.output(pin_num,False)

	def cleanup(self):	
		GPIO.cleanup()


if __name__ == "__main__":

	import sys
	zone = int(sys.argv[1])
        mapping = {1:7, 2:11, 3:13, 4:15, 5:16, 6:18}
        
	rp = Raspipin(mapping)
	
	print("turn on pin " + str(rp.get_pin(zone)))

	rp.button_press(zone)

	rp.cleanup()

	
