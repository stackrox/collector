# Purpose
The intention of `run-tests.sh` is to test that `get-builder-flavor.sh` is working as desired. 


# How to run
To run the tests execute the following

```
./run-tests.sh
```

# Create your own tests
To create and run your own tests create a file <my-tests> with the following format

```
version distro expected_build
```

See `TestInput.txt` for an example

Then execute the following

```
./run-tests.sh <my-tests>
```

The default tests file is TestInput.txt
