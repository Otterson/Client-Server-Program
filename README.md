# Client-Server-Program
The client-server program is the culmination of various different assignments in an Introduction to Computer Systems class. This project is intended to emulate the setting of a 
hospital, where data points for patients vital signs can be send to a central hub for processing and monitoring.

The client-server program is designed around hyperthreading and interprocess communication and uses mutex/semaphore objects to allow for processes to access and edit shared memory one at a time to prevent inter-process interference and memory corruption.

The client side spawns multiple threads of various types, responsible for requesting, processing and sorting the data received from the server, which creates and sends data points for various patients to be processed by the client. The server can also send files to be processed as a whole instead of sending individual data points.
