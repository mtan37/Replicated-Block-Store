# Replicated-Block-Store
Project 3 for the distributed system class

# Compiling

  ```
  > mkdir build
  > cd build
  > cmake ..
  > make
  ```
  > If you installed cmake locally, please add `-DCMAKE_PREFIX_PATH=$MY_INSTALL_DIR` to your cmake command

# Run the concurrency test

  ```
  # run this in the first terminal
  > ./server localhost localhost
  # run this in the second terminal. This call blocks on the server side indefinitely
  > ./concurrency_test 0
  # run this in the third terminal. This call should go through and prints an "Your request is working!!!!" on the server terminal
  > ./concurrency_test 1
  ```
