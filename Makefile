# Compiler and flags
CXXFLAGS += -std=c++17 -Wall -pthread $(GST_CFLAGS) $(EXTRA_CXXFLAGS)
LDFLAGS += $(GST_LIBS) $(EXTRA_LDFLAGS) \
           -ltensorflowlite -ledgetpu -lusb-1.0

# Source and header files
SRCS := src/camerastreamer.cc \
        src/inferencewrapper.cc \
        src/main.cc

OBJS := $(SRCS:.cc=.o)

HEADERS := src/camerastreamer.h \
           src/inferencewrapper.h

# Target binary
TARGET := coral-camera

# GStreamer configuration (pkg-config)
GST_CFLAGS := $(shell pkg-config --cflags gstreamer-1.0 gstreamer-plugins-base-1.0)
GST_LIBS := $(shell pkg-config --libs gstreamer-1.0 gstreamer-plugins-base-1.0)

# Rules
.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cc $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
