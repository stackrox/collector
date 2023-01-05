import time

from boltdb import BoltDB


def test_collector(docker_client, collector, db_path):
    docker_client.containers.run("hello-world:latest", remove=True)

    time.sleep(10)

    with BoltDB(db_path).view() as db:
        db.bucket(b"Process")
