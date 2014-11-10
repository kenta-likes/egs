If multiple threads are on one socket, and one thread calls close_connection while still in use, this is considered an user error and has undefined behavior.
