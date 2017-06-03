tcpdaemon -m LF -l 0:9527 -s test_callback_http_echo.so -c 10 --tcp-nodelay --tcp-linger 1 --logfile $HOME/log/test_callback_http_echo.log --loglevel-debug --daemon-level
