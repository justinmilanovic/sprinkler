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

def delete_file(filename):
	try:
		os.remove(filename)
	except OSError as e:
                print("File not found")
                print("-"*40)
		sys.exit(1)

def get_args_length():
	return len(sys.argv)

def print_jobs():


        s = Scheduler()
        temp = s.print_jobs()
        for inx in temp:
                print(inx)

                

def print_log():
	try:
		f = open(LOGFILE, 'r')
	except IOError as e:
		print("No log to print")
		print("-"*40)
		sys.exit(1)
	else:
		for line in f:
			print(line)
			
def get_job():
	if get_args_length() == 7:       			
		return {'zone':sys.argv[2], 'month':sys.argv[3], 'day':sys.argv[4], 'hour':sys.argv[5], 'minute':sys.argv[6]}
			
	else:
		print("invalid command - ZONE MONTH DAY HOUR MINUTE")
		print("-"*40)
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
                print("-"*40)
		commit_job()
		print("-"*40)
	elif command == 'delete':
                print("-"*40)
		print("Deleting all jobs")
		delete_file(PATH_CRON)
		print("-"*40)
	elif command == 'print':
		print("-"*40)
		print_jobs()
		print("-"*40)

	elif command == 'log':
                print("-"*40)
		print_log()
		print("-"*40)
	elif command == 'clearlog':
                print("-"*40)
                print("Clearing Log")
		delete_file(LOGFILE)
		print("-"*40)
	else:
                print("-"*40)
		print("Command not found")
                print("-"*40)






