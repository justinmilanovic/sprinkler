#!/usr/bin/python

import sys, os
from datetime import datetime
from cron import Scheduler

PATH_PYTHON = '/usr/bin/python'
PATH_SCRIPT = '/root/Desktop/scheduler/gpio.py'
PATH_LOG = '/root/Desktop/scheduler/log.txt'
PATH_CRON = '/var/spool/cron/crontabs/root'
FILENAME = 'schedule.txt'

def current_time():
	return str(datetime.now())

def file_empty(path):
    return os.stat(path).st_size==0

def delete_crontab():
	try:
		os.remove(PATH_CRON)
	except OSError as e:
		pass	


def readtxt(filename):
	list=[]

	try:
		f = open(filename, 'r')

	except IOError as e:
		exc = str(type(e))
		print( exc + " @ " + current_time() + ">> - missing schedule.txt file")
		sys.exit(1)

	if file_empty(FILENAME):
		print("Removing all jobs in scheduler")
		delete_crontab()
		sys.exit(1)

	else:

		for line in f:
			line = line.split()
			list.append({'zone':line[0], 'month':line[1], 'day':line[2], 'hour':line[3], 'minute':line[4]})
		return list

		
if __name__=='__main__':
	
	delete_crontab()
	jobs = readtxt(FILENAME)

	s = Scheduler() 
	for job in jobs:
		s.set_command(PATH_PYTHON, PATH_SCRIPT, PATH_LOG, job['zone'])
		s.set_job(job['day'], job['hour'], job['minute'], job['month'])

	print(s.render())


