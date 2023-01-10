import re

from common.process import ProcessOriginator


class Endpoint:
    def __init__(self, data: str):
        parts = line.split('|')

        assert len(parts) == 5

        self.protocol = parts[1]
        self.listen_address = ListenAddress(parts[2])
        self.close_timestamp = parts[3]
        self.originator = ProcessOriginator(parts[4])


class ListenEndpoint:
    def __init__(self, data: str):
        assert data and data != "<nil>"

        matches = re.match(r"address_data:(.*) port:(.*) ip_network:(.*)", data)

        assert matches is not None

        self.address_data = matches.group(1)
        self.port = int(matches.group(2))
        self.ip_network = matches.group(3)

