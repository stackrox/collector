import re


class Process:
    def __init__(self, data: str):
        parts = data.split(':', 6)

        self.name = parts[0]
        self.exe = parts[1]

        self.uid = int(parts[2])
        self.gid = int(parts[3])
        self.pid = int(parts[4])

        self.args = parts[5]

    def __str__(self) -> str:
        return f"(name={self.name!r} ({self.exe!r}), args={self.args!r}, user={self.uid}:{self.gid}, pid={self.pid})"


class ProcessLineage:
    def __init__(self, data: str):
        parts = data.split(':', 6)

        self.name = parts[0]
        self.exe = parts[1]
        self.parent_uid = int(parts[3])
        self.parent_exe = parts[5]

    def __str__(self) -> str:
        return f"(name={self.name!r} ({self.exe!r}), parent_uid=${self.parent_uid}, parent_exe={self.parent_exe!r})"


class ProcessOriginator:
    def __init__(self, data: str):
        assert data and data != "<nil>"

        matches = re.match(r"process_name:(.*)process_exec_file_path:(.*)process_args:(.*)$", data)

        if not matches:
            matches = re.match(r"process_name:(.*)process_exec_file_path:(.*)$", data)
        else:
            self.process_args = matches.group(3)

        assert matches is not None

        self.process_name = matches.group(1).rstrip('"').strip('"')
        self.exec_path = matches.group(2).rstrip('"').strip('"')
