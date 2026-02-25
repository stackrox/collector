
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include <rpm/header.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmlib.h>
#include <rpm/rpmts.h>

#include "avl-cmp.h"
#include "avl.h"

struct package {
  struct avl_node packages;

  const char* name;

  // The requirements of this package
  struct require {
    const char* name;
    struct require* next;
  }* reqs;  // not using 'requires' as clang-format doesn't like it
};

// List of 'provided' items and the pkg that provides each of them.
// There can be several entries for the same name.
struct provide {
  struct avl_node provides;

  const char* name;
  struct package* pkg;
};

struct avl_tree packages;
struct avl_tree provides;

// Iterate over the list of installed packages.
// Fills the 'packages' list and the 'provides' list.
static void enumerate_packages() {
  rpmts ts;
  rpmdbMatchIterator mi;
  Header h;

  ts = rpmtsCreate();

  mi = rpmtsInitIterator(ts, 0, NULL, 0);

  if (!mi) {
    fprintf(stderr, "Error creating database iterator \n");
    rpmtsFree(ts);
    return;
  }

  rpmtd name_td = rpmtdNew();

  while ((h = rpmdbNextIterator(mi)) != NULL) {
    struct package* pkg = (struct package*)malloc(sizeof(*pkg));

    pkg->name = strdup(headerGetString(h, RPMTAG_NAME));
    pkg->packages.key = pkg->name;
    pkg->reqs = NULL;

    avl_insert(&packages, &pkg->packages);

    if (headerGet(h, RPMTAG_REQUIRENAME, name_td, HEADERGET_DEFAULT)) {
      const char* reqname;
      while ((reqname = rpmtdNextString(name_td)) != NULL) {
        struct require* req = (struct require*)malloc(sizeof(*req));

        req->name = strdup(reqname);
        req->next = pkg->reqs;
        pkg->reqs = req;
      }
    }
    if (headerGet(h, RPMTAG_PROVIDENAME, name_td, HEADERGET_DEFAULT)) {
      const char* provname;
      while ((provname = rpmtdNextString(name_td)) != NULL) {
        struct provide* prov = (struct provide*)malloc(sizeof(*prov));

        prov->name = strdup(provname);
        prov->provides.key = prov->name;
        prov->pkg = pkg;

        avl_insert(&provides, &prov->provides);
      }
    }
  }

  rpmtdFree(name_td);
  rpmdbFreeIterator(mi);
  rpmtsFree(ts);

  return;
}

// Remove from the 'packages' list all packages providing 'depname',
// and their transitive requirements.
// The visited dependency graph is printed to stderr.
static void remove_recursively(const char* depname) {
  struct provide* provide;

  provide = avl_find_element(&provides, depname, provide, provides);
  if (provide) {
    do {
      struct package* pkg = provide->pkg;

      fprintf(stderr, "%s->[%s] ", depname, pkg->name);

      if (pkg->packages.list.next == NULL) {
        // the package was already removed from the tree
        break;
      }
      avl_delete(&packages, &pkg->packages);
      struct require* require = pkg->reqs;
      while (require != NULL) {
        fprintf(stderr, "[%s]->%s ", pkg->name, require->name);
        remove_recursively(require->name);
        require = require->next;
      }

      if (avl_is_last(&provides, &provide->provides)) {
        break;
      }
      provide = avl_next_element(provide, provides);
    } while (!provide->provides.leader);
  }
}

// Iterate over the 'packages' list and print each name
static void print_packages() {
  struct package* pkg;
  avl_for_each_element(&packages, pkg, packages) {
    printf("%s ", pkg->name);
  }
}

int main(int argc, char** argv) {
  avl_init(&packages, &avl_strcmp, false, NULL);
  avl_init(&provides, &avl_strcmp, true, NULL);

  // configure the RPM environment with the default
  rpmReadConfigFiles(NULL, NULL);

  // Load all the package names and require/provide relationships
  enumerate_packages();

  // For every argument provided, remove the packages providing
  // this dependency from the list
  fprintf(stderr, "Walking dependencies:\n");
  while (argc > 1) {
    const char* depname = argv[argc - 1];
    remove_recursively(depname);
    argc--;
  }
  fprintf(stderr, "\n=========\n");

  // Print the remaining packages to stdout
  print_packages();

  return 0;
}
