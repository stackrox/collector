#!python3

import argparse
import shutil
import os
import subprocess
import yaml


g_config_root = "ci-operator/config/stackrox/collector"
g_steps_root = "ci-operator/step-registry/stackrox/collector"


class OpenshiftRelease:
    def __init__(self, root, version, dry_run=False):
        self.root = root
        self.dry_run = dry_run
        self.version = version
        self.version_str = f"release-{version}"

    def from_root(self, *paths):
        """
        Helper for getting a path from the root of the repository

        e.g. from_root('ci-operator/config') -> /path/to/repo/ci-operator/config
        """
        return os.path.join(self.root, *paths)

    def step_from_root(self, *paths):
        """
        Helper for getting a file relative to stackrox/collector's step
        registry.

        e.g. step_from_root('release-3.11', 'integration-tests')
             -> /path/to/repo/ci-operator/step-registry/stackrox/collector/release-3.11/integration-tests
        """
        return self.from_root(g_steps_root, *paths)

    def config_from_root(self, name):
        """
        Helper for getting a config file relative to stackrox/collector's config
        directory.

        e.g. config_from_root('stackrox-collector-master.yaml')
             -> /path/to/repo/ci-operator/config/stackrox/collector/stackrox-collector-master.yaml
        """
        return self.from_root(g_config_root, name)

    def copy(self, src: str, dest: str):
        """
        Wrapper around file / directory copying. If dry_run is set,
        only the log message is printed and copying does not actually occur
        """
        print(f"[COPY ] {src} ->\n        {dest}")
        if self.dry_run:
            return

        shutil.copy(src, dest)

    def mkdir(self, path: str):
        """
        Wrapper around directory creation. If dry_run is set, only the log
        message is printed and creation does not actually occur
        """
        print(f"[MKDIR] {path}")

        if self.dry_run:
            return

        os.mkdir(path)

    def update(self):
        """
        Runs make update from the repository root directory. If dry_run is
        set only the log message is printed and make is not run
        """
        print(f"[EXEC ] make -C {self.root} update")

        if self.dry_run:
            return

        subprocess.check_call(["make", "-C", self.root, "update"])

    def copy_config(self):
        """
        Copies the master configuration as it currently stands, to a release
        configuration based on the configured release version.

        The release config is then modified in the following ways:

            - image promotion is removed
            - all pre-submit tests are removed
            - remaining post-submit tests are modified to use versioned
              workflows.
        """
        release_config_filename = f"stackrox-collector-{self.version_str}.yaml"
        master_config = self.config_from_root("stackrox-collector-master.yaml")
        release_config = self.config_from_root(release_config_filename)

        self.copy(master_config, release_config)

        if self.dry_run:
            print(f"[WRITE] would modify references in {release_config_filename}")
            return

        print(f"[WRITE] modifying references in {release_config_filename}")

        with open(release_config) as config_file:
            cfg = yaml.load(config_file, Loader=yaml.Loader)

            # remote image promotion
            del cfg["promotion"]

            # remove all the tests not marked as post-submit
            cfg["tests"] = [
                test for test in cfg["tests"] if test.get("postsubmit", False)
            ]

            for test in cfg["tests"]:
                if "steps" not in test:
                    continue

                if "workflow" in test["steps"]:
                    workflow = test["steps"]["workflow"]
                    workflow = workflow.replace(
                        "stackrox-collector-", f"stackrox-collector-{self.version_str}-"
                    )
                    test["steps"]["workflow"] = workflow

            cfg_data = yaml.dump(cfg, Dumper=yaml.Dumper)

        # dump the config back to disk
        with open(release_config, "w") as config_file:
            config_file.write(cfg_data)

    def copy_steps(self):
        """
        Copies all step-registry files into a release directory, to scope
        them off from the usual 'master' step-registry. Most of the files need
        to be renamed, and references to each other within the files need to
        be changed as well.

        The renaming is based on the directory structure relative to
        ci-operator/step-registry.

        e.g. ci-operator/step-registry/stackrox/collector/integration-tests/stackrox-collector-integration-tests-workflow.yaml

             becomes

            ci-operator/step-registry/stackrox/collector/release-<version>/integration-tests/stackrox-collector-release-<version>-integration-tests-workflow.yaml
        """
        self.mkdir(self.step_from_root(self.version_str))
        self.copy(self.step_from_root('OWNERS'), self.step_from_root(self.version_str, 'OWNERS'))

        # get all top-level steps that aren't release directories
        subdirs = next(os.walk(self.from_root(g_steps_root)))[1]
        subdirs = filter(lambda d: "release" not in d, subdirs)

        for subdir in subdirs:
            self.mkdir(self.step_from_root(self.version_str, subdir))

            for root, dirs, files in os.walk(self.step_from_root(subdir)):
                for dir in dirs:
                    #
                    # we need to reconstruct the path to include the release
                    # directory, so using relpath we can get the part of the
                    # path relative to ci-operator/step-registry/stackrox/collector
                    # and then tack it onto the end of the release directory
                    #
                    src = os.path.join(root, dir)
                    relative_src = os.path.relpath(src, self.from_root(g_steps_root))

                    self.mkdir(self.step_from_root(self.version_str, relative_src))

                for file in files:
                    ext = os.path.splitext(file)[1]
                    src = os.path.join(root, file)

                    # As above - we need to reconstruct the path to insert
                    # the release directory
                    dest = os.path.relpath(src, self.from_root(g_steps_root))
                    dest = self.step_from_root(self.version_str, dest)

                    if ext in (".sh", ".yaml", ".json"):
                        #
                        # Any reference to a stackrox-collector- ref/workflow must
                        # be renamed, to the version-specific version.
                        #
                        dest = dest.replace(
                            "stackrox-collector-",
                            f"stackrox-collector-{self.version_str}-",
                        )

                    self.copy(src, dest)

                    if ext == ".yaml":
                        self.fixup_step_references(dest)

    def fixup_step_references(self, release_file):
        """
        Fixes up internal references to yaml files within the step registry.
        It follows the same naming patterns as the file paths, in that
        the release version is inserted into `as:` `ref:` and `commands:`
        directives in the yaml
        """
        if self.dry_run:
            print(f"[WRITE] Would rename references in {release_file}")
        else:
            print(f"[WRITE] Renaming references in {release_file}")
            with open(release_file) as f:
                content = f.read()

            #
            # This is a pretty basic but effective way of performing this
            # replacement, and avoids overzealous replacement of other parts
            # of the file.
            #
            # The alternative of loading as a yaml object, and replacing in
            # a more structured way, is probably over complicated for what
            # we're trying to achieve, but could be implemented if this
            # needs to be expanded.
            #
            content = content.replace(
                "as: stackrox-collector-", f"as: stackrox-collector-{self.version_str}-"
            )
            content = content.replace(
                "commands: stackrox-collector-",
                f"commands: stackrox-collector-{self.version_str}-",
            )
            content = content.replace(
                "ref: stackrox-collector-",
                f"ref: stackrox-collector-{self.version_str}-",
            )
            with open(release_file, "w") as f:
                f.write(content)


def main(args):
    osr = OpenshiftRelease(args.openshift_release, args.version, dry_run=args.dry_run)

    osr.copy_config()
    osr.copy_steps()

    osr.update()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Collector OSCI release script")

    parser.add_argument(
        "openshift_release", help="Full path to openshift/release repository"
    )
    parser.add_argument(
        "version", help="The release version in {major}.{minor} format, e.g. 3.12"
    )
    parser.add_argument(
        "--dry-run",
        "-d",
        action="store_true",
        help="Perform a dry run, printing actions that would be taken.",
    )

    args = parser.parse_args()

    main(args)
