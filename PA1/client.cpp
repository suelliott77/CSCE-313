/*
    Author of the starter code
    Yifan Ren
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 9/15/2024
	
    Please include your Name, UIN, and the date below
    Name: Sutton Elliott
    UIN: 531008819
    Date: 09/16/2024
*/

#include "common.h"
#include "FIFORequestChannel.h"
#include <chrono> // For measuring time

using namespace std;
using namespace std::chrono;

int main (int argc, char *argv[]) {
    int opt;
    int p = -1;
    double t = -1;
    int e = -1;
    int buffer = MAX_MESSAGE;
    bool new_chan = false;
    vector<FIFORequestChannel*> channels;

    string filename = "";
    // Add other arguments here
    while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
        switch (opt) {
            case 'p':
                p = atoi(optarg);
                break;
            case 't':
                t = atof(optarg);
                break;
            case 'e':
                e = atoi(optarg);
                break;
            case 'f':
                filename = optarg;
                break;
            case 'm': // setting a buffer size from user input
                buffer = atoi(optarg);
                break;
            case 'c': // flag for requesting a new channel
                new_chan = true;
                break;
        }
    }

    // give arguments for the server
    // server needs './server'
    pid_t server_request = fork();
    if (server_request == 0) {
        char* argu [] = {(char*)("./server"), (char*)("-m"), (char*)std::to_string(buffer).c_str(), nullptr};
		execvp(argu[0], argu);
    } 
    else if (server_request < 0) {
        perror("Fork failed");
        exit(1);
    }

    // Task 1:
    // Run the server process as a child of the client process
    FIFORequestChannel control_chan("control", FIFORequestChannel::CLIENT_SIDE);
    channels.push_back(&control_chan);

    // Task 4:
    // Request a new channel
    if (new_chan) {
        MESSAGE_TYPE nc = NEWCHANNEL_MSG;
        control_chan.cwrite(&nc, sizeof(MESSAGE_TYPE));
        char buf0;
        string chanName;
        control_chan.cread(&buf0, sizeof(char));
        while (buf0 != '\0') {
            chanName.push_back(buf0);
            control_chan.cread(&buf0, sizeof(char));
        }
        FIFORequestChannel* new_chan_ptr = new FIFORequestChannel(chanName, FIFORequestChannel::CLIENT_SIDE);
        channels.push_back(new_chan_ptr);
    }

    FIFORequestChannel& chan = *channels.back();

    // Task 2:
    // Request data points
    // Request 1000 data points if no specific filename or data point p, t, e if given
    if (filename == "" && t == -1 && e == -1) {
        ofstream file;
        filename = "received/x1.csv";
        file.open(filename);
        if (!file.is_open()) {
            cerr << "Error opening file for writing: " << filename << endl;
            return -1;
        }
        double time = 0;
        int count = 0;

        while (count < 1000) {
            file << time;
            for (int i = 1; i <= 2; i++) { // request for both ecg1 and ecg2
                char buf[MAX_MESSAGE];
                datamsg x(p, time, i);

                memcpy(buf, &x, sizeof(datamsg));
                chan.cwrite(buf, sizeof(datamsg));
                double reply;
                chan.cread(&reply, sizeof(double));
                file << "," << reply;
            }
            file << "\n";
            time += 0.004;
            count++;
        }
        file.close();
    }

    // Task 3:
    // Request files
    else if (filename != "") {
        // Start measuring transfer time
        auto start = high_resolution_clock::now();

        // Check if the file exists before proceeding
        // Receive the file length into filesize in bytes
        filemsg fm(0, 0);
        int len = sizeof(filemsg) + (filename.size() + 1);
        char* buf2 = new char[len];
        char* buf3 = new char[buffer];
        memcpy(buf2, &fm, sizeof(filemsg));
        strcpy(buf2 + sizeof(filemsg), filename.c_str());
        chan.cwrite(buf2, len);

        __int64_t file_length = 0;
        chan.cread(&file_length, sizeof(__int64_t));

        ofstream ofile;
        ofile.open("received/" + filename, ios::binary);
        if (!ofile.is_open()) {
            cerr << "Error opening file: received/" << filename << endl;
            delete[] buf2;
            delete[] buf3;
            return -1;
        }

        if (file_length < 0) { // Handle non-existent files
            cerr << "Error: Requested file '" << filename << "' does not exist on the server." << endl;
            delete[] buf2;
            delete[] buf3;
            return -1;
        }
        cout << "The length of " << filename << " is " << file_length << endl;

        int loop_count = floor((double)file_length / double(buffer));
        for (int i = 0; i < loop_count; i++) {
            filemsg* file_req = (filemsg*)buf2;
            file_req->offset = buffer * i;
            file_req->length = buffer;
            chan.cwrite(buf2, len);
            chan.cread(buf3, file_req->length);
            ofile.write(buf3, buffer);
        }

        // Request the final chunk
        filemsg* file_req = (filemsg*)buf2;
        file_req->offset = buffer * loop_count;
        file_req->length = file_length % buffer;
        chan.cwrite(buf2, len);
        chan.cread(buf3, file_req->length);
        ofile.write(buf3, file_length % buffer);

        ofile.close();
        delete[] buf2;
        delete[] buf3;

        // Stop measuring transfer time
        auto stop = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(stop - start);
        cout << "File transfer time: " << duration.count() << " milliseconds." << endl;

        cout << "You can use the following command to verify the file:" << endl;
        cout << "diff BIMDC/" << filename << " received/" << filename << endl;
    } 

    // Task 5:
    // Handle specific data requests
    else {
        char buf[MAX_MESSAGE]; // 256
        datamsg x(p, t, e);
        memcpy(buf, &x, sizeof(datamsg));
        chan.cwrite(buf, sizeof(datamsg));

        double reply;
        chan.cread(&reply, sizeof(double));
        cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
    }

    // Closing the channel and cleaning up if new channel was requested
    if (new_chan) {
        MESSAGE_TYPE m = QUIT_MSG;
        chan.cwrite(&m, sizeof(MESSAGE_TYPE));
        delete channels.back();
        chan = *(channels.front());
    }

    // Task 5:
    // Closing all the channels
    MESSAGE_TYPE m = QUIT_MSG;
    chan.cwrite(&m, sizeof(MESSAGE_TYPE));
}
