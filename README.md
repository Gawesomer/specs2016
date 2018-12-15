# specs2016
A re-writing of the specs pipeline stage from CMS, only changed quite a bit

"specs" is a command line utility for parsing and re-arranging text
input. It allows re-alignment of fields, some format conversion, and
re-formatting multiple lines into single lines or vice versa. Input
comes from standard input, and output flows to standard output.

This version is liberally based on the **CMS Pipelines User's Guide and Reference** ([link](https://publib.boulder.ibm.com/epubs/pdf/hcsj0c30.pdf)), especially chapters 16, 24, and 20.

The command line format is as follows:

  specs [switches] [spec-units]
  
Switches
========
* --toASCII -- causes output to be translated into ASCII if it's outside the range.
* --force-read-input -- forces specs to read every input line even if none of the spec units use it.  By default they won't.
* --specFile or -f -- reads the specification from a file rather than the command line.
* --verbose or -v -- outputs more information when something goes wrong.
* --stats -- output statistics on run time, and records read, and on records written. 
   
Spec Units
==========
Each spec unit specifies an action to be taken by the program. The spec unit may span from one to several command line arguments.

The most common spec unit is a *data field*, which consists of five arguments, three of which may be omitted:

    [fieldIdentifier] InputPart [conversion] OutputPart [alignment]
  
The **InputPart** argument may be any of the following:

* A range of characters, such as `5`, `3-7`, or `5.8`, the last one indicating 8 characters starting in the 5th position. Note that the indexing of characters is 1- rather than 0-based.
* A range of words, such as `w5` or `words 5-7`, where words are separated by one or more `wordseparator` characters -- spaces by default. The word indexing is 1-based.
* A range of fields, such as `fields 5` or `f5-7`, where fields are separated by exactly one `fieldseparator` characters -- a tab by default. The field indexing is 1-based.
* **TODclock** - a 64-bit formatted timestamp, giving microseconds since the Unix epoch.
* **DTODclock** - a 64-bit formatted timestamp, giving microseconds since the Unix epoch. The difference is that TODclock shows the time when this run of *specs* begun, while DTODclock gives the time of producing the current record.
* **NUMBER** - A record counter as a 10-digit decimal number.
* **TIMEDIFF** - an 8-char decimal number indicating the number of seconds since the invocation of the program.
* An **ID** keyword followed by a previously defined **FieldIdentifier**.
* The **NUMBER** keyword used as a counter for processed records.
* The **PRINT** keyword followed by a calculated expression
* A string literal, optionally enclosed by delimiters, such as `/TODclock/` or `'NUMBER'`. Note that to include the single quotes on the command line requires you to enclose them in double quotes.
* A **SUBSTring** of another InputPart.
    
The **OutputPart** argument specifies where to put the source:

* absolute position (such as `1`)
* range (such as `1-5` or `1.5`)
* `n` or `next` for placing the output immediately after the 
    previous output.
* `nw` or `nextword` for placing the output following a space 
    character after the previous output.
* `nf` or `nextfield` for placing the output following a tab 
    character after the previous output.
    
The alignment argument can be `l`, `c`, or `r`, for "left", "center", and "right" respectively.

The conversion argument can specify any of the following conversions:

* **rot13** - encrypts the bytes using the ROT-13 cipher
* **C2B** - converts characters to binary: "AB" --> "0010000100100010"
* **C2X** - converts characters to hexadecimal: "AB" --> "4142"
* **B2C** - converts binary to characters: "0010000100100010" --> "AB". Will throw an exception if called with an invalid character.
* **X2CH** - converts hexadecimal to characters: "4142" --> "AB". Will throw an exception if called with an invalid character.
* **b2x** - converts binary data to hex
* **D2X** - convert decimal to hex: "314159265" --> "12b9b0a1"  (not working yet)
* **X2D** - convert hex to decimal: "12b9b0a1" --> "314159265"  (not working yet)
* **ucase** - converts text to uppercase (not working yet)
* **lcase** - converts text to lowercase (not working yet)
* **BSWAP** - byte swap. reverses the order of bytes. "AB" --> "BA"
* **ti2f** - convert internal time format (8-byte microseconds since the epoch) to printable format using the conventions of strftime, plus %xf for fractional seconds, where x represents number of digits from 0 to 6.
* **tf2i** - convert printable time format to the internal 8-byte representation. 
 
There are also other spec units, that may be used:

* **read** - causes the program to read the next line of input. If we have already read the last line, the read line is taken to be the empty string.
* **readstop** - causes the program to read the next line of input. If we have already read the last line, no more processing is done for this iteration.
* **write**- causes the program to write to output and reset the output 
      line.
* **WordSeparator** and **FieldSeparator** declare a character to be the word of field separator respectively which affects word and field ranges.
* **redo** -- causes the current output line to become the new input line.  NOT IMPLEMENTED YET.
      
Example:

   `ls -l` yields this:

    total 352
    -rw-r--r--@ 1 ynir  admin    574 Aug 25  2009 Makefile
    -rw-r--r--@ 1 ynir  admin   3542 Nov 23 00:21 README
    -rw-r--r--@ 1 ynir  admin    362 Nov 19 08:31 conversion.h
    -rw-r--r--  1 ynir  admin    984 Nov 11 17:45 ls.txt
    -rw-r--r--@ 1 ynir  admin   2233 Nov 23 00:03 main.cc
    -rw-r--r--  1 ynir  admin   9412 Nov 23 00:11 main.o
    -rw-r--r--@ 1 ynir  admin   6567 Nov 23 00:09 spec_build.cc
    -rw-r--r--  1 ynir  admin  16776 Nov 23 00:11 spec_build.o
    -rw-r--r--@ 1 ynir  admin   5494 Nov 19 08:30 spec_convert.cc
    -rw-r--r--  1 ynir  admin  17004 Nov 23 00:11 spec_convert.o
    -rw-r--r--@ 1 ynir  admin  11419 Nov 23 00:10 spec_params.cc
    -rw-r--r--  1 ynir  admin  21080 Nov 23 00:11 spec_params.o
    -rw-r--r--@ 1 ynir  admin    375 Nov 11 09:29 spec_vars.cc
    -rw-r--r--  1 ynir  admin   4800 Nov 23 00:11 spec_vars.o
    -rwxr-xr-x  1 ynir  admin  36740 Nov 23 00:11 specs
    -rw-r--r--@ 1 ynir  admin   1547 Nov 23 00:10 specs.h

Let's run it though a spec:

    ls -l | specs 12-* 1 redo w2 1 w4 d2x 8.8 r w8 17
The first spec unit converts it to this:

    1 ynir  admin    574 Aug 25  2009 Makefile
    1 ynir  admin   3542 Nov 23 00:21 README
    1 ynir  admin    362 Nov 19 08:31 conversion.h
    1 ynir  admin    984 Nov 11 17:45 ls.txt
    1 ynir  admin   2233 Nov 23 00:03 main.cc
    1 ynir  admin   9412 Nov 23 00:11 main.o
    1 ynir  admin   6567 Nov 23 00:09 spec_build.cc
    1 ynir  admin  16776 Nov 23 00:11 spec_build.o
    1 ynir  admin   5494 Nov 19 08:30 spec_convert.cc
    1 ynir  admin  17004 Nov 23 00:11 spec_convert.o
    1 ynir  admin  11419 Nov 23 00:10 spec_params.cc
    1 ynir  admin  21080 Nov 23 00:11 spec_params.o
    1 ynir  admin    375 Nov 11 09:29 spec_vars.cc
    1 ynir  admin   4800 Nov 23 00:11 spec_vars.o
    1 ynir  admin  36740 Nov 23 00:11 specs
    1 ynir  admin   1547 Nov 23 00:10 specs.h

Then after the redo, we get this:

    ynir        23e Makefile
    ynir        dd6 README
    ynir        16a conversion.h
    ynir        3d8 ls.txt
    ynir        8b9 main.cc
    ynir       24c4 main.o
    ynir       19a7 spec_build.cc
    ynir       4188 spec_build.o
    ynir       1576 spec_convert.cc
    ynir       426c spec_convert.o
    ynir       2c9b spec_params.cc
    ynir       5258 spec_params.o
    ynir        eae spec_vars.cc
    ynir       12c0 spec_vars.o
    ynir       8f84 specs
    ynir        60b specs.h

      
Alternatively, let's arrange this on multiple lines:

    ls -l | specs w9 1 write "Owner:" 3 w3 10 write "Size:" 3 w5 10-20 r

    Makefile
      Owner: ynir
      Size:          574
    README
      Owner: ynir
      Size:         5834
    conversion.h
      Owner: ynir
      Size:          362
    list.txt
      Owner: ynir
      Size:          978
    ls.txt
      Owner: ynir
      Size:          984
    main.cc
      Owner: ynir
      Size:         2233
    main.o
      Owner: ynir
      Size:         9412

Finally, let's make our own version of the multi-column display:

    ls -l | specs w9 1 read w9 26 read w9 51
                             Makefile                 README
    conversion.h             main.cc                  main.o
    spec_build.cc            spec_build.o             spec_convert.cc
    spec_convert.o           spec_params.cc           spec_params.o
    spec_vars.cc             spec_vars.o              specs
    specs.h

Differences from CMS Pipelines **specs**
===
* A colon (`:`) can be used in a from-to range, in addition to the hyphen and semicolon supported in CMS Pipelines, as in `5:7` or `5:-5`.
* **TimeDiff** does not exist in CMS pipelines.
* **ReDo** does not exist in CMS pipelines.
* **TODclock** - The mainframe version uses the ESA/390 architecture TOD clock, where the high-order 32 bits represent the time since the 1/1/1900 epoch in units of 1.048576 seconds and the other 32-bit represent fractions of this unit. We use the Unis epoch (based at 1/1/1970) with a 64-bit field of nanoseconds.
* **DTODclock** does not exist in CMS pipelines.
* No support for multiple input and output streams.
* Support tail labels in addition to the head labels from CMS Pipelines
* Exchanged the integer division and remainder operators to reflect C/Python rather than REXX. So **%** is the remainder operator, whereas **//** is the integer division operator.
