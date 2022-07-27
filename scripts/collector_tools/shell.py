import subprocess
import logging

from typing import List, Tuple


def cmd(*args: List[str], capture: bool = True) -> Tuple[str, str]:
    """
    :brief: Runs a command in the shell, with the provided arguments.

    :param args: a list of command plus arguments to pass to the command
    :param capture: whether to capture the output of the command rather than
                    writing it to the stdout of this process

    :return: tuple of (stdout, stderr)
    :raises: CalledProcessException on failure to run the command
    """

    logging.info(" ".join(args))

    # if this fails, a CalledProcessException is thrown, and the stdout/stderr
    # is contained within the exception, should it be useful to the caller
    process = subprocess.run(args, check=True, shell=True, capture_output=capture)
    return process.stdout, process.stderr


def make(target, *args):
    return cmd("make", *args, target)
