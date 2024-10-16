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

using namespace std;

int main (int argc, char *argv[]) {
    int opt;
    int p = 1; 
    double t = 0.0;
    int e = 1;
    int buff_size = MAX_MESSAGE;
    bool newChanReq = false;
    vector<FIFORequestChannel*> channels;
    
    string filename = "";
    while ((opt = getopt(argc, argv, "p:t:e:f:")) != -1) {
        switch (opt) {
            case 'p':
                p = atoi (optarg); //atoi converts ASCII into int. stores value of the argument of the user to p.
                break;
            case 't':
                t = atof (optarg); //optarg gets the argv?
                break;
            case 'e':
                e = atoi (optarg);
                break;
            case 'f':
                filename = optarg;
                break;
        }
    }

    // Part 1: Fork server process
    pid_t server_request = fork();
    if(server_request == 0)
    {
        // Child process: Start server with the correct buffer size
        cout << "Starting server process..." << endl;  // [NEW: Debug statement]
        char* argu [] = {(char*)("./server"), (char*)("-m"), (char*)std::to_string(buff_size).c_str(), nullptr};
        execvp(argu[0], argu);
        perror("Error executing server");  // [NEW: Handle execvp failure]
        exit(1);  // Ensure the child process exits if execvp fails
    }
    else if (server_request < 0)
    {
        perror("Fork failed");  // [NEW: Error handling for fork failure]
        exit(1);
    }
    else
    {
        cout << "Server started with PID: " << server_request << endl;  // [NEW: Print server PID]
    }

    // Create the control channel
    FIFORequestChannel cont_chan("control", FIFORequestChannel::CLIENT_SIDE);
    cout << "Created control channel: control" << endl;  // [NEW: Debug message to confirm channel creation]
    channels.push_back(&cont_chan);

    // Handle the new channel request if required [MODIFIED]
    if (newChanReq) 
    {
        MESSAGE_TYPE nc = NEWCHANNEL_MSG;
        cont_chan.cwrite(&nc, sizeof(MESSAGE_TYPE));  // Send new channel request
        cout << "Requesting new channel..." << endl;  // [NEW: Debug statement]
        
        char buf0;
        string chanName;
        cont_chan.cread(&buf0, sizeof(char));
        while (buf0 != '\0')
        {
            chanName.push_back(buf0);
            cont_chan.cread(&buf0, sizeof(char));
        }
        
        cout << "New channel name received: " << chanName << endl;  // [NEW: Debug statement]
        
        FIFORequestChannel* new_chan = new FIFORequestChannel(chanName, FIFORequestChannel::CLIENT_SIDE);
        channels.push_back(new_chan);
    }
    
    // Use the latest channel for data requests
    FIFORequestChannel chan = *(channels.back()); 

    // Part 2: Requesting a data point [MODIFIED]
    if (filename == "" && t != -1 && e != -1) // ECG data point request
    { 
        char buf[MAX_MESSAGE];
        datamsg x(p, t, e);  // Use user input for person, time, and ecg number
        memcpy(buf, &x, sizeof(datamsg));
        chan.cwrite(buf, sizeof(datamsg));  // Send data request
        double reply;
        chan.cread(&reply, sizeof(double));  // Receive data point
        cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
    }

    // Part 3: Requesting a file [MODIFIED]
    else if (filename != "")
    {
        // Requesting file length
        filemsg fm(0, 0); 
        int len = sizeof(filemsg) + (filename.size() + 1);
        char* buf2 = new char[len];
        char* buf3 = new char[buff_size];  // Buffer for receiving file data
        memcpy(buf2, &fm, sizeof(filemsg));
        strcpy(buf2 + sizeof(filemsg), filename.c_str());
        
        // Send file length request
        chan.cwrite(buf2, len);
        int64_t filesize = 0;
        chan.cread(&filesize, sizeof(int64_t));
        
        cout << "Requested file: " << filename << " of size: " << filesize << " bytes" << endl;  // [NEW: Debug statement]
        
        // Creating and opening the file in "received" directory
        ofstream ofile("received/" + filename, ios::binary);  // Open as binary
        if (!ofile.is_open())
        {
            cerr << "Error: Could not open file in received directory." << endl;
            delete[] buf2;
            delete[] buf3;
            return -1;
        }

        // Loop over file segments to receive in chunks [MODIFIED]
        int loop_count = filesize / buff_size;
        for (int i = 0; i < loop_count; i++)
        {
            filemsg* file_req = (filemsg*)buf2;
            file_req->offset = i * buff_size;
            file_req->length = buff_size;
            
            chan.cwrite(buf2, len);  // Send request for file chunk
            chan.cread(buf3, buff_size);  // Receive file chunk
            
            ofile.write(buf3, buff_size);  // Write to file
            cout << "Received chunk " << i + 1 << " of size " << buff_size << endl;  // [NEW: Debug statement]
        }

        // Handle any remaining bytes
        int remaining_bytes = filesize % buff_size;
        if (remaining_bytes > 0)
        {
            filemsg* file_req = (filemsg*)buf2;
            file_req->offset = loop_count * buff_size;
            file_req->length = remaining_bytes;
            
            chan.cwrite(buf2, len);  // Request last chunk
            chan.cread(buf3, remaining_bytes);  // Receive last chunk
            
            ofile.write(buf3, remaining_bytes);  // Write last chunk
            cout << "Received last chunk of size " << remaining_bytes << endl;  // [NEW: Debug statement]
        }
        
        ofile.close();  // Close the file after transfer
        delete[] buf2;
        delete[] buf3;
    }

    // If a new channel was requested, close and clean it up [MODIFIED]
    if (newChanReq) 
    {
        MESSAGE_TYPE m = QUIT_MSG;
        chan.cwrite(&m, sizeof(MESSAGE_TYPE));  // Quit new channel
        delete channels.back();  // Clean up new channel
        chan = *(channels.front());  // Revert to the control channel
        cout << "Switched back to control channel." << endl;  // [NEW: Debug statement]
    }

    // Closing the main channel    
    MESSAGE_TYPE m = QUIT_MSG;
    chan.cwrite(&m, sizeof(MESSAGE_TYPE));
}


// int main (int argc, char *argv[]) {
// 	vector<FIFORequestChannel*> channels;

// 	FIFORequestChannel* chan = new FIFORequestChannel("control", FIFORequestChannel::CLIENT_SIDE);
//     channels.push_back(chan); 
// 	int opt;
// 	int p = 1;
// 	double t = 0.0;
// 	int e = 1;
// 	int buff_size= MAX_MESSAGE;
// 	string filename = "";

// 	//Add other arguments here
// 	while ((opt = getopt(argc, argv, "p:t:e:f:")) != -1) {
// 		switch (opt) {
// 			case 'p':
// 				p = atoi (optarg);
// 				break;
// 			case 't':
// 				t = atof (optarg);
// 				break;
// 			case 'e':
// 				e = atoi (optarg);
// 				break;
// 			case 'f':
// 				filename = optarg;
// 				break;
// 		}
// 	}

// 	//Task 1:
// 	//Run the server process as a child of the client process
// 	pid_t server_pid = fork();

// 	if (server_pid < 0) {
//     	// Fork failed
//     	perror("fork failed");
//     	exit(1);
// 	}
// 	else if (server_pid == 0) {
//     	// Execute the server with buffer size as an argument
//     	char* args[] = {(char*)("./server"), (char*)"-m", (char*)std::to_string(buff_size).c_str(), nullptr};
//     	execvp(args[0], args);
//     	perror("execvp failed");  // In case execvp fails
//     	exit(1);  // Exit child process
// 	} 

// 	//Task 4:
// 	//Request a new channel
// 	MESSAGE_TYPE new_channel = NEWCHANNEL_MSG;
// 	chan->cwrite(&new_channel, sizeof(MESSAGE_TYPE));  // Request a new channel

// 	char new_channel_name[100];
// 	chan->cread(new_channel_name, sizeof(new_channel_name));  // Read the name of the new channel

// 	FIFORequestChannel* new_chan = new FIFORequestChannel(new_channel_name, FIFORequestChannel::CLIENT_SIDE);
// 	channels.push_back(new_chan);  // Store the new channel in the vector for future use

// 	//Task 2:
// 	//Request data points
//     char buf[MAX_MESSAGE];
	
// 	datamsg x(1, 0.0, 1);
// 	chan->cwrite(&x, sizeof(datamsg));  // Send the data request
// 	double reply;
// 	chan->cread(&reply, sizeof(double));  // Read the ECG response
// 	cout << "For person " << p << ", at time " << t << ", the value of ECG " << e << " is " << reply << endl;
	
// 	//Task 3:
// 	//Request files
// 	filemsg fm(0, 0);
// 	string fname = "1.csv";
	
// 	int len = sizeof(filemsg) + (fname.size() + 1);
// 	char* buf2 = new char[len];
// 	memcpy(buf2, &fm, sizeof(filemsg));
// 	strcpy(buf2 + sizeof(filemsg), fname.c_str());
// 	chan -> cwrite(buf2, len);

// 	delete[] buf2;

// 	__int64_t file_length;
// 	chan -> cread(&file_length, sizeof(__int64_t));
// 	cout << "The length of " << fname << " is " << file_length << endl;

// 	filemsg* fm_pointer = (filemsg*) buf2;
// 	__int64_t offset = 0;
// 	int remaining = file_length;

// 	ofstream output("received/" + fname, ios::binary);  // Open the file once at the beginning

// 	while (remaining > 0) {
//     	int transfer_size = std::min(static_cast<int>(remaining), buff_size - static_cast<int>(sizeof(filemsg)));
//     	fm_pointer->offset = offset;
//     	fm_pointer->length = transfer_size;

//     	chan->cwrite(buf2, len);
//     	chan->cread(buf, transfer_size);

//     	output.write(buf, transfer_size);  // Write the chunk to the file

//     	offset += transfer_size;
//     	remaining -= transfer_size;
// 	}

// 	output.close();  // Close the file after the entire transfer is complete
// 	delete[] buf2; // Free the memory allocated for buf2

	
// 	//Task 5:
// 	// Closing all the channels
// 	for (auto ch : channels) {
//     	MESSAGE_TYPE m = QUIT_MSG;
//     	ch -> cwrite(&m, sizeof(MESSAGE_TYPE));
// 		delete ch; // clean up
// 	}

// 	MESSAGE_TYPE m = QUIT_MSG;
// 	chan -> cwrite(&m, sizeof(MESSAGE_TYPE));
// 	delete chan; // clean up the main channel
// }