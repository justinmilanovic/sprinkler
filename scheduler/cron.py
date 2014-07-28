import sys
from crontab import CronTab

class Scheduler(CronTab):

	def __init__(self):

		super(Scheduler, self).__init__()
		self._cmd = None
		self._job = None

	def set_command(self, path_python, path_script, path_log, zone):

		self._cmd = path_python + " " + path_script + " " + zone + " >>" + path_log
		self._job = super(Scheduler, self).new(self._cmd)

	def set_job(self, day, hour, minute, month):
		
		self._job.month.on(month)
		self._job.dow.on(day)
		self._job.hour.on(hour)
		self._job.minute.on(minute)
		super(Scheduler, self).write()



