ouroboros - Ongoing Build Assistant
===================================


Installation
------------

	$ autoreconf --install
	$ mkdir build && cd build
	$ ../configure --enable-libconfig
	$ make && make install


Usage
-----

There are a few configuration options which might need to be tailored for ones requirements,
however default options should be good enough to start with. For all available options see the
exemplary configuration file. All of those options might be specified during the runtime as well.

Ouroboros acts like a wrapper, so the basic invocation looks like this:

	$ ouroboros supervised-applicaion.bin arg1 arg2


Example
-------

Assume we've got some Python based server which needs to be reloaded every time the code changes.
It might be pretty much annoying to restart it manually every time we've made some modifications
and we want to test them. With Ouroboros it is pretty straightforward.

`simple-server.py`:

	#!/usr/bin/python
	# simple-server.py - Simple HTTP Server

	import http.server
	import socketserver

	class Handler(http.server.SimpleHTTPRequestHandler):
	    pass

	HOST, PORT = "localhost", 8000
	httpd = socketserver.TCPServer((HOST, PORT), Handler)

	try:
	    print("Starting server: {}:{}".format(HOST, PORT))
	    httpd.serve_forever()
	except KeyboardInterrupt:
	    print("\rShutdown...")
	    httpd.shutdown()


Run server using Ouroboros:

	$ ouroboros -k SIGINT ./simple-server.py
	Starting server: localhost:8000

After making some modifications (and saving them) we will see the following output (assuming that
our changes did not break the code):

	Shutdown...
	Starting server: localhost:8000

But what if we made a mistake and the code can not be executed any more? We shouldn't be worried,
that's the answer. Remember, that our process is supervised and will be restarted every time
watched files are modified. So here is what will happen:

	Shutdown...
	  File "./simple-server.py", line 5
	  import break
	             ^
	SyntaxError: invalid syntax

And after the fix:

	Starting server: localhost:8000
