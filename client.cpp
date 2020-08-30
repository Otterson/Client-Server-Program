#include "common.h"
#include "BoundedBuffer.h"
#include "Histogram.h"
#include "common.h"
#include "HistogramCollection.h"
#include "Reqchannel.h"
#include "FIFOreqchannel.h"
#include "MQreqchannel.h"

using namespace std;

RequestChannel* create_new_channel(RequestChannel* mainchan, string ival, int mb){
  char name [1024];
  MESSAGE_TYPE m = NEWCHANNEL_MSG;
  mainchan->cwrite(&m, sizeof(m));
  mainchan->cread(name, 1024);
  RequestChannel* newchan = 0;
  if(ival == "f"){
    newchan = new FIFORequestChannel(name, RequestChannel::CLIENT_SIDE);
  }else if(ival == "q"){
    newchan = new MQRequestChannel(name, RequestChannel::CLIENT_SIDE, mb);
  }
  return newchan;
}

void patient_thread_function(int n, int pno, BoundedBuffer* request_buffer){
    datamsg d(pno, 0.0, 1);
    double resp = 0;
    for(int i = 0; i < n; i++){
      request_buffer->push((char *) &d, sizeof(datamsg));
      d.seconds += 0.004;
    }
}

void file_thread_function(string filename, BoundedBuffer* request_buffer, RequestChannel* chan, int mb){
  string recvfilename = "recv/" + filename;
  char buf[1024];
  filemsg f(0,0);
  memcpy(buf, &f, sizeof(f));
  strcpy(buf + sizeof(f), filename.c_str());
  chan->cwrite(buf, sizeof(f) + filename.size() + 1);
  __int64_t filelength;
  chan->cread(&filelength, sizeof(filelength));
  FILE* file = fopen(recvfilename.c_str(), "w");
  fseek(file, filelength, SEEK_SET);
  fclose(file);

  
  filemsg* fm = (filemsg *) buf;
  __int64_t remlen = filelength;

  while(remlen > 0){
    fm->length = min(remlen, (__int64_t) mb);
    request_buffer->push(buf, sizeof(filemsg) + filename.size() + 1);
    fm->offset += fm->length;
    remlen -= fm->length;
  }

}

void worker_thread_function(RequestChannel* chan, BoundedBuffer* request_buffer, HistogramCollection* hc, int mb){
  char buf[1024];
  double resp = 0;

  char recvbuf[mb];
  while(true){
      request_buffer->pop(buf, 1024);
      MESSAGE_TYPE* m = (MESSAGE_TYPE *) buf;
if(*m == QUIT_MSG){
        chan->cwrite(m, sizeof(MESSAGE_TYPE));
        delete chan;
        break;
      }

      else if(*m == DATA_MSG){
        chan->cwrite(buf, sizeof(datamsg));
        chan->cread(&resp, sizeof(double));
        hc->update(((datamsg *) buf)->person, resp);
      }else if(*m == FILE_MSG){
        filemsg* fm = (filemsg*) buf;
        string filename = (char *)(fm + 1);
        int sz = sizeof(filemsg) + filename.size() + 1;
        chan->cwrite(buf, sz);
        chan->cread(recvbuf, mb);

        string recvfilename = "recv/" + filename;
        FILE* file = fopen(recvfilename.c_str(), "r+");
        fseek(file, fm->offset, SEEK_SET);
        fwrite(recvbuf, 1, fm->length, file);
        fclose(file);
      }
  }
}


int main(int argc, char *argv[]){
  int n = 10000;    //default number of requests per "patient"
  int p = 5;     // number of patients [1,15]
  int w = 100;    //default number of worker threads
  int b = 100; 	// default capacity of the request buffer, you should change this default
	int m = MAX_MESSAGE; 	// default capacity of the message buffer
  char* m2 = "256";
  string f = "";
  srand(time_t(NULL));

  int opt = -1;
  string ival = "f";

  while((opt = getopt(argc, argv, "m:n:b:w:p:f:i:")) != -1){
    switch(opt){
      case 'm':
        m = atoi(optarg);
        m2 = optarg;
        break;
      case 'n':
        n = atoi(optarg);
        break;
      case 'p':
        p = atoi(optarg);
        break;
      case 'b':
        b = atoi(optarg);
        break;
      case 'w':
        w = atoi(optarg);
        break;
      case 'f':
        f = optarg;
        break;
      case 'i':
        ival = optarg;
        break;
    }
  }

  int pid = fork();
  if (pid == 0){
    execl ("server", "server", "-m", m2, "-i", ival.c_str(), (char *)NULL);
  }

	RequestChannel* chan;
  if(ival == "f"){
    chan = new FIFORequestChannel("control", RequestChannel::CLIENT_SIDE);
  }else if(ival == "q"){
    chan = new MQRequestChannel("control", RequestChannel::CLIENT_SIDE, m);
  }
  BoundedBuffer request_buffer(b);
	HistogramCollection hc;
  string filename = f;

  //creating histograms and adding to the histogram collection hc
  for(int i = 0; i < p; i++){
    Histogram* h = new Histogram(10, -2.0, 2.0);
    hc.add(h);
  }

  //creating 'w' worker channels
  RequestChannel* wchans [w];
  for(int i = 0; i < w; i++){
    wchans[i] = create_new_channel(chan, ival, m);
  }


  struct timeval start, end;
  gettimeofday (&start, 0);

  if (f != ""){ //If it is a file request
    //Create threads
    thread filethread(file_thread_function, filename, &request_buffer, chan, m);
    thread workers [w];

    for(int i = 0; i < w; i++){
      workers[i] = thread(worker_thread_function, wchans[i], &request_buffer, &hc, m);
    }
    cout<<"worker threads started"<<endl;

    //Join file thread
    filethread.join();
    cout << "File thread finished" << endl;

    for(int i = 0; i < w; i++){
      MESSAGE_TYPE q = QUIT_MSG;
      request_buffer.push((char*) &q, sizeof(q));
    }

    //Join worker threads
    for(int i = 0; i < w; i++){
      workers[i].join();
    }
    cout << "Worker threads finished" << endl;

  }else{ 
    thread patient [p];
    for(int i = 0; i < p; i++){
      patient[i] = thread(patient_thread_function, n, i+1, &request_buffer);
    }
    cout<<"Patient threads started"<<endl;

    thread workers [w];
    for(int i = 0; i < w; i++){
      workers[i] = thread(worker_thread_function, wchans[i], &request_buffer, &hc, m);
    }

    for(int i = 0; i < p; i++){
      patient[i].join();
    }

    cout << "Patient threads finished" << endl;

    for(int i = 0; i < w; i++){
      MESSAGE_TYPE q = QUIT_MSG;
      request_buffer.push((char*) &q, sizeof(q));
    }

    for(int i = 0; i < w; i++){
      workers[i].join();
    }
    cout << "Worker threads finished" << endl;

  	hc.print ();
  }

  gettimeofday (&end, 0);

  int secs = (end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec)/(int) 1e6;
  int usecs = (int)(end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec)%((int) 1e6);
  cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;

  //cleaning main channel
  MESSAGE_TYPE q = QUIT_MSG;
  chan->cwrite ((char *) &q, sizeof (MESSAGE_TYPE));
  cout << "All Done!!!" << endl;
  delete chan;

}
