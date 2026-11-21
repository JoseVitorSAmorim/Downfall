CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2
TARGET = downfall

RAYLIB_PATH = /home/jose/raylib/src
INCLUDES = -I. -I$(RAYLIB_PATH)
LDFLAGS = -L$(RAYLIB_PATH) -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

SRC = main.cpp
HEADERS = World.hpp Player.hpp Weapon.hpp SquadAI.hpp Menu.hpp Audio.hpp Objectives.hpp MapEditor.hpp AssetManager.hpp Cutscene.hpp SaveSystem.hpp Network.hpp Navigation.hpp 

all: $(TARGET)

$(TARGET): $(SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(INCLUDES) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean