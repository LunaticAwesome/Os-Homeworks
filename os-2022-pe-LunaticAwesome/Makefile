CC=g++
CFLAGS=-c -Wall
LDFLAGS=
SOURCES= main.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=pe-parser

all: $(SOURCES) $(EXECUTABLE)

validation-pe-tests: all
	python3 -m tests ValidatingPeTestCases -f

import-dll-tests: all
	python3 -m tests ImportDllTestCases -f

import-function-tests: all
	python3 -m tests ImportFunctionTestCases -f

export-function-tests: all
	python3 -m tests ExportFunctionTestCases -f

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	-rm $(OBJECTS) $(EXECUTABLE)
