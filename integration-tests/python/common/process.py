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
