
default:	build

clean:
	rm -rf Makefile objs

build:
	$(MAKE) -f objs/Makefile
	$(MAKE) -f objs/Makefile manpage

install:
	$(MAKE) -f objs/Makefile install

upgrade:
	/home/guangtong/nginx_test/sbin/nginx -t

	kill -USR2 `cat /home/guangtong/nginx_test/logs/nginx.pid`
	sleep 1
	test -f /home/guangtong/nginx_test/logs/nginx.pid.oldbin

	kill -QUIT `cat /home/guangtong/nginx_test/logs/nginx.pid.oldbin`
