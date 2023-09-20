# Variables
CC = C:\\msys64\\ucrt64\\bin\\gcc.exe
WINDRES = C:\\msys64\\ucrt64\\bin\\windres.exe
CFLAGS = -Wall -Wextra -O3 -march=native -funroll-loops -g -std=c17
LDFLAGS = -lgdi32 -luser32 -lshlwapi -mwindows
INCLUDE = -Isrc

# Source, Object, and Resource Files
SRC = src/main.c
OBJ = $(SRC:.c=.o)
RES = resources/icon.res

# Target Executable
TARGET = ReactionTimeTester.exe

all: $(TARGET)

$(TARGET): $(OBJ) $(RES)
	$(CC) $(CFLAGS) $(INCLUDE) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<

$(RES): resources/icon.rc
	$(WINDRES) $< -O coff -o $@

clean:
	rm -f $(OBJ) $(TARGET) $(RES)

.PHONY: all clean
