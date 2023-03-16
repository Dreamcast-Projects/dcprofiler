# dcprofile

This project utilizes Moops DreamHAL perfcounter source code to count cycles.  So major credit to Moop.  https://github.com/sega-dreamcast/dreamhal

There is two parts:  
1. The files you include in your project (profiler.c, profiler.h)
2. The application that parses the data your dreamcast application generates (pvtrace).  This application is based on 
https://web.archive.org/web/20130528172555/http://www.ibm.com/developerworks/library/l-graphvis/ but I added a ton of stuff/details.


Instructions:
1. Add profiler.c and profiler.h to you projects source directory.
2. Edit your makefile to add '-g -finstrument-functions' to your CFLAGS and 'profiler.o' to your OBJS.
3. I use the following command to send elf file to Dreamcast:
  sudo /opt/toolchains/dc-utils/dc-tool-ip -c "/PATH/TO/PROJECT" -t 192.168.1.137 -x main.elf
4. Profiling automatically starts when you start running the application so make sure to call shutdownProfiling() to stop profiling.
5. Run pvtrace application on your elf with the command:
  ```pvtrace main.elf```

This assumes a couple of things.  That your 'sh-elf-addr2line' application is in '/opt/toolchains/dc/sh-elf/bin/' directory and that your 
.elf file and trace.txt(generated by your application) file are the same directory. 

6.  That will generate a graph.dot file that you can use the following command to generate a jpg:
  ```dot -Tjpg graph.dot -o graph.jpg```

I installed dot using brew:  
  brew install graphviz

Another method to view your DOT file and convert it to an image is to paste the contents of the .DOT file in this online editor: https://dreampuf.github.io/GraphvizOnline/

