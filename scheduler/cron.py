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

        def print_jobs(self):
                
                jobs = self.render()
                list=[]
                string_=''
                
                for job in jobs:
                      s = ''  
                      if not job == '\n':
                            string_ += job
                      else:
                            list.append(string_)
                            string_=''
                            
                frmt_list=[]
                for inx in list:
                       inx = inx.split()
                       
                       if not (len(inx)) == 0:
                               string_ = "Zone " + inx[7] + " Every " + inx[3] + " on " + inx[4] + " at " + inx[1] + ":" + inx[0]
                               frmt_list.append(string_)

                return frmt_list

                

