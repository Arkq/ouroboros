Ouroboros - Ongoing Build Assistant
===================================

Developing (building) service-based systems might be a really painful process, especially when it
comes to testing it. In the most strict scenario you will need a running server instance, and
every time a new code is deployed you will have to restart it. If you are using some framework,
this task is probably done for you automatically. But what if you grew out of frameworks and to-do
list apps, and want to make something for yourself. So in this case you have to make your own
developing environment - service reloading and crash maintaining.

Recently, I was put in such a situation and after a while I was shocked, that there is no black
box like solution for this task. There is a framework way or the highway - simply put, you are out
of luck.

This project aims to fill this gap. Ouroboros is nothing more but a simple tool which will restart
any application after some watched file has been modified. So now you can skip the part with
developing your own reloading mechanism, and focus on more important things like what you've
wanted to do in the first place. Besides, unless you are creating some AI thingy, you won't need
any development related tools in you final product, will you?


Installation
------------

	$ autoreconf --install
	$ mkdir build && cd build
	$ ../configure --enable-libconfig --enable-iniparser --enable-server
	$ make && make install

Optional dependencies:

* [libconfig](http://www.hyperrealm.com/libconfig/) - Support for loading configuration from a
  file. Note, that it is not possible to configure all available options via the command line
  arguments, so it is highly advised to compile ouroboros with the libconfig enabled.
* [iniparser](http://ndevilla.free.fr/iniparser/) - Support for INI-like configuration files.


Usage
-----

There are a few configuration options which might need to be tailored for ones requirements,
however default options should be good enough to start with. For all available options see the
exemplary [configuration file](/doc/example.conf). Most of these options - the important ones -
might be specified during the runtime as well.

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


Similar projects
----------------

1. [inotifywait](https://github.com/rvoicilas/inotify-tools/wiki) - wait for a particular event on a set of files
2. [watchman](https://facebook.github.io/watchman/) - a file watching service developed by Facebook
