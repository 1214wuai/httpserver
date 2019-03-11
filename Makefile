.PHONY:all
all:httpserver upload

httpserver:httpserver.cpp
	g++ -o $@ $^ -l pthread -std=c++11

upload:upload.cpp
	g++ -o $@ $^ -l pthread -std=c++11

.PHONY:clean
clean:
	rm httpserver upload -f
