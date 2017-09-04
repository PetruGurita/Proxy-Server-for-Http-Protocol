Implementation of a HTTP protocol capable of implementing the communication between a web browser and a HTTP server.
The program takes as parameter from the command line a port number and listens on all the station's IPs, only on the indicated port.
It listens to requests, parses the URL and checks if the data was already saved in cache ( if it is not present in cache, it will be saved afterwards).
The cache is cleared after the proxy is started.
