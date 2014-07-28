import sys, os
from datetime import datetime
from cron import Scheduler

PATH_PYTHON = '/usr/bin/python'
PATH_SCRIPT = '/root/Desktop/scheduler/gpio.py'
PATH_LOG = '/root/Desktop/scheduler/log.txt'
PATH_CRON = '/var/spool/cron/crontabs/root'
LOGFILE = 'log.txt'

def current_time():
	return str(datetime.now())

def delete_crontab():
	try:
		os.remove(PATH_CRON)
	except OSError as e:
		pass

def get_args_length():
	return len(sys.argv)

def print_jobs():
	s = Scheduler()
	print(s.render())		

def print_log():
	try:
		f = open(LOGFILE, 'r')
	except IOError as e:
		print(current_time() + str(type(e)))
		sys.exit(1)
	else:
		for line in f:
			print(line)
			
def get_job():
	if get_args_length() == 7:       			
		return {'zone':sys.argv[2], 'month':sys.argv[3], 'day':sys.argv[4], 'hour':sys.argv[5], 'minute':sys.argv[6]}
			
	else:
		print("missing argument - ZONE MONTH DAY HOUR MINUTE")
		sys.exit(1)

def commit_job():

	job = get_job()
	s = Scheduler() 
	s.set_command(PATH_PYTHON, PATH_SCRIPT, PATH_LOG, job['zone'])
	s.set_job(job['day'], job['hour'], job['minute'], job['month'])
	print_jobs()
	
if __name__ == "__main__":

	command = sys.argv[1]
	if command == 'set':
		commit_job()
	elif command == 'delete':
		print("Deleting all jobs")
		delete_crontab()
	elif command == 'print':
		print_jobs()
	elif command == 'log':
		print_log()
	else:
		print("Command not found")







