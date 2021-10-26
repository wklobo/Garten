#!/usr/bin/python
# -*- coding: utf-8 -*-
# 2014-04-01; 
import os

vers = open("version.h")
temp = open("version.temp", "w")
for line in vers:
  myline = line.rstrip()
  pos = myline.find(" BUILD ")
  if pos > 0:
    vs = myline[pos+7:pos+7+10]
    vi=int(vs)+1
    newline = myline[0:pos+7] + str(vi)
    temp.write(newline + '\n')
    print newline
  else:
    temp.write(myline + '\n')
vers.close()
temp.close()
os.rename("version.h", "version.@@@")
os.rename("version.temp", "version.h")
os.remove("version.@@@")
