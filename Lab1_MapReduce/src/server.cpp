#include "master_service.hpp"
#include <thread>

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        std::cout << "Missing parameter! Input format is './master <number of reduce tasks> ../tests/pg*.txt'" << std::endl;
        return 1;
    }

    /*
        Parse arguments
    */ 
    int mapCount { argc - 2 };
    int reduceCount { std::stoi(argv[1])};

    std::vector<std::string> fileNames;
    for (int i=2; i<argc; i++) {
        fileNames.push_back(argv[i]);
    }


    /*
        Initialize master and start gRPC server
    */ 
    Master master { mapCount, reduceCount, fileNames };
    MasterServiceImpl service{ master };
    grpc::ServerBuilder builder;
    builder.AddListeningPort("0.0.0.0:5555", grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Master listening on port 5555" << std::endl;
    

    /*
        Start a separate thread to monitor when to exit master
    */
    std::thread monitorThread([&master, &service]() {
        while (true) {
            if (master.getMapRemaining() == 0 && master.getReduceRemaining() == 0)
            {
                std::cout << "All map and reduce tasks completed. Shutting down master server..." << std::endl;
                server->Shutdown();
                break;
            }
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    });


    // Block main thread until gRPC server shutdown
    server->Wait();

    // Join monitor thread before exiting
    monitorThread.join();
    return 0;
}

// ./master <number of reduce tasks> ../tests/pg*.txt