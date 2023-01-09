import argparse
from argparse import RawTextHelpFormatter
import os, sys
import random
import time
import subprocess


def main(num_ports, num_per_second):
    ports = [False] * (num_ports + 1)
    procs = [None] * (num_ports + 1)
    while True:
        time.sleep(1.0 / float(num_per_second))
        rand_port = random.randint(1, num_ports)
        if not ports[rand_port]: 
            try:
                ports[rand_port] = True
                procs[rand_port] = subprocess.Popen(["socat", "TCP-LISTEN:" + str(rand_port) + ",fork", "STDOUT"])
            except (subprocess.CalledProcessError, BlockingIOError) as e:
                print(e)
                ports[rand_port] = False
        else:
            try:
                ports[rand_port] = False
                procs[rand_port].terminate()
            except (subprocess.CalledProcessError, BlockingIOError) as e:
                print(e)
                ports[rand_port] = True


if __name__ == '__main__':
    description = ('Opens and closes ports randomly')
    parser = argparse.ArgumentParser(
                                    description=description,
                                    formatter_class=RawTextHelpFormatter
                                    )

    num_ports_help = 'The number of ports in the range of ports to be opened and closed. '
    num_ports_help += 'I.e if numPorts is 1000 the ports that will be opened and closed will be in the range [1-1000].'

    parser.add_argument('numPorts', type=int, help=num_ports_help)
    parser.add_argument('numPerSecond', help='The number of ports to be opened and closed per second')
    args = parser.parse_args()

    num_ports = args.numPorts
    num_per_second = args.numPerSecond

    main(num_ports, num_per_second)
