If multiple threads are on one socket, and one thread calls close_connection while still in use, 
this is considered an user error and causes undefined behavior.
If multiple threads try to call send on one socket, 
this is considered an user error and causes undefined behavior. 
