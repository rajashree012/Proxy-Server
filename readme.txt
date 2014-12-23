ECEN602 HW3 Programming Assignment 
(implementation of HTTP1.0 proxy server and client)
-----------------------------------------------------------------

Team Number: 22
Member 1 # Zhou, Shenmin (UIN: 823008644)
Member 2 # Polsani, Rajashree Rao (UIN: 223001584)
---------------------------------------

Description/Comments:
--------------------
1. proxy.c contains the code for proxy server
2. client.c contains code for clients (multiple clients are supported)
3. cache size is 10
4. file names on the client side are as mentioned in the grading assignment. file names in the cache have 'cache' as prefix(eg cachebrick.txt)
5. All the responses from web servers are assumed to contain expiry field.
6. Brief description of the architecture : 
	Client makes a http request to the proxy server. Then one of the 4 cases is possible
	a) If the file is already in cache and if it is not stale (expiry time > current time) then the same file is returned to the client.
	b) If the file is in the cache but it is stale, then a request is sent to the web server. In case the response contains 200 code then the file in cache is replaced along with the expiry time.
	c) If the file is in the cache and it is stale, then a request is sent to the web server. In case the response code is 304 then the expiry time is updated and the same file in the cache is sent to the client.
	d) If the file is not in the cache then it is requested from the web server and stored in the cache by replacing a least recently used file in case it is full.
7. Detailed comments are written in the code and logs are displayed on the screen.

Notes from your TA:
-------------------
1. While submitting, delete all the files and submit HTTP1.0 proxy server, client files along with makefile and readme.txt to Google Drive folder shared to each team.
2. Don't create new folder for submission. Upload your files to teamxx/HW3.
3. Do not make readme.txt as a google document. Keep it as a text file.
4. I have shared 'temp' folder for your personal (or team) use. You can do compilation or code share among team members using this folder.
5. Make sure to check file transfer between different machines. While grading, I'll use two different machines (lab 213A, Zachry) for your proxy server and client.
6. This is a standard protocol, your client should be compatible with proxy server of other teams. Your proxy server should be compatible with clients of other teams.

All the best. 

Unix command for starting proxy server:
------------------------------------------
./proxy PROXY_SERVER_IP PROXY_SERVER_PORT

Unix command for starting client:
----------------------------------------------
./client PROXY_SERVER_IP PROXY_SERVER_PORT URL

