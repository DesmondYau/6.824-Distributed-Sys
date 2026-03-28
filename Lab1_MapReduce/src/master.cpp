#include <iostream>
#include <vector>
#include "./buttonrpc-master/buttonrpc.hpp"

class Master
{
public:
    Master(int mapCount, int reduceCount);
    void getAllFiles(int argc, char* argv[]);
    int getMapCount();
    int getReduceCount();


private:
    int mapCount_;                                // number of map tasks
    int reduceCount_;                             // number of reduce tasks
    std::vector<std::string> files_;              // vector storing all input files

};

Master::Master(int mapCount, int reduceCount)
    : mapCount_ { mapCount }
    , reduceCount_ { reduceCount }
{}

void Master::getAllFiles(int argc, char* argv[])
{
    for (int i=2; i<argc; i++)
    {
        files_.emplace_back(argv[i]);
    }
}

int Master::getMapCount() {
    return mapCount_;
}

int Master::getReduceCount() {
    return reduceCount_;
}


int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cout << "Missing parameter! Input format is './master ../testfiles/pg*.txt'" << std::endl;
        return 1;
    }


    buttonrpc server;
    server.as_server(5555);

    Master master { 8, 12 };
    master.getAllFiles(argc, argv);
    server.bind("getMapCount", &Master::getMapCount, &master);
    server.bind("getReduceCount", &Master::getReduceCount, &master);
	
    std::cout << "Running RPC server on: " << 5555 << std::endl;
    server.run();
    return 0;


}