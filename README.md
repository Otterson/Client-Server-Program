# Client-Server-Program
The client-server program is the culmination of various different assignments in an Introduction to Computer Systems class. This project is intended to emulate the setting of a 
hospital, where data points for patients vital signs can be send to a central hub for processing and monitoring.

This project, designed around interprocess communication and hyper threading, has two main parts: the client side and server side. 
The client side spawns multiple threads of various types, responsible for requesting, processing and sorting the data received from the server. 
The server side creates and sends data points for various patients to be processed by the client. 

Additionally, the server can send patient files to the client to be processed as a whole instead of sending individual data points.
